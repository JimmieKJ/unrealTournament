// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashTrackerPrivatePCH.h"


DEFINE_LOG_CATEGORY(LogCrashTracker);

/*-----------------------------------------------------------------------------
	Stat declarations.
-----------------------------------------------------------------------------*/

DECLARE_CYCLE_STAT(TEXT("Total Time"),STAT_TotalTime,STATGROUP_CrashTracker);
DECLARE_CYCLE_STAT(TEXT("Send Frame To Compressor"),STAT_SendFrameToCompressor,STATGROUP_CrashTracker);
DECLARE_CYCLE_STAT(TEXT("Wait For Previous Frame To Compress"),STAT_WaitForPreviousFrameToCompress,STATGROUP_CrashTracker);

namespace CrashTrackerConstants
{
	static const int32 MaxVideoFrameCompressedSize = 400 * 1024; // 400 KB

	static const int32 JPEGQuality = 85; // high quality

	static const int32 VideoFramesToCapture = 200;
	static const float CaptureFrequency = 0.1f; // 10 fps

	static const float SecondsToKeyPressFade = 2.f;
	static const int32 MaxKeyPressesInBuffer = 15;

	// Crash tracker video stream is scaled down by 50% to reduce the about of system resources used while
	// working in the editor during capture
	static const float ScreenScaling = 0.5f;
}


FCompressedDataFrame::FCompressedDataFrame(int InBufferSize)
	: Data(NULL)
	, BufferSize(InBufferSize)
	, ActualSize(0)
{
	Data = new uint8[BufferSize];
}


FCompressedDataFrame::~FCompressedDataFrame()
{
	delete [] Data;
}


FAsyncImageCompress::FAsyncImageCompress(IImageWrapperPtr InImageWrapper, class FCompressedDataFrame* InDataFrame)
	: ImageWrapper(InImageWrapper)
	, DataFrame(InDataFrame)
{}


void FAsyncImageCompress::DoWork()
{
	TArray<uint8> CompressedData = ImageWrapper->GetCompressed(CrashTrackerConstants::JPEGQuality);
	int32 CompressedSize = CompressedData.Num();

	if (CompressedSize <= CrashTrackerConstants::MaxVideoFrameCompressedSize)
	{
		DataFrame->ActualSize = CompressedSize;

		FMemory::Memcpy(DataFrame->Data, CompressedData.GetData(), CompressedSize);
	}
	else
	{
		UE_LOG(LogCrashTracker, Warning, TEXT("Crash Tracker Size of %i bytes was too large. Frame Dropped."), CompressedSize);
		DataFrame->ActualSize = 0;
	}
}


FString FCrashVideoCapture::CaptureVideoPath;


FCrashVideoCapture::FCrashVideoCapture()
	: CurrentBufferIndex(0)
	, Width(0)
	, Height(0)
	, CaptureSlateRenderer(NULL)
	, CurrentAccumSeconds(0.f)
	, CurrentFrameCaptureIndex(0)
	, bIsRunning(false)
	, KeypressBuffer()
	, CompressedFrames()
	, AsyncTask(NULL)
	, CVarCrashTrackerIsRunning(
		TEXT("CrashTracker.IsRunning"),
		bIsRunning,
		TEXT("Whether to toggle the running of crash video capturing in the background."))
{
	Buffer[0] = Buffer[1] = NULL;
}


FCrashVideoCapture::~FCrashVideoCapture()
{
	if (bIsRunning)
	{
		EndCapture(/*bSaveCapture=*/false);
	}
}


void FCrashVideoCapture::BeginCapture(FSlateRenderer* SlateRenderer)
{
	IFileManager::Get().Delete(*CaptureVideoPath);
	
	CaptureSlateRenderer = SlateRenderer;
	CurrentAccumSeconds = 0.f;
	CurrentFrameCaptureIndex = 0;
	
	KeypressBuffer.Empty();

	Buffer[0] = Buffer[1] = NULL;
	CurrentBufferIndex = 0;
	
	CompressedFrames.Empty(CrashTrackerConstants::VideoFramesToCapture);
	for (int32 i = 0; i < CrashTrackerConstants::VideoFramesToCapture; ++i)
	{
		CompressedFrames.Add(new FCompressedDataFrame(CrashTrackerConstants::MaxVideoFrameCompressedSize));
	}

	bIsRunning = true;
}


void FCrashVideoCapture::EndCapture(bool bSaveCapture)
{
	UE_LOG(LogCrashTracker, Log, TEXT("Ending crash video capture."));

	if (bSaveCapture && bIsRunning)
	{
		// ensure we capture the last frame
		if (IsInGameThread() && IsRenderingThreadHealthy())
		{
			Update(CrashTrackerConstants::CaptureFrequency);
		}
		else
		{
			UE_LOG(LogCrashTracker, Log, TEXT("Crashed outside of game thread; not drawing final frame"));
		}
		CleanupFinalFrame();
		SaveFinalCapture( CaptureVideoPath );
	}
	else
	{
		CleanupFinalFrame();
	}
	
	for (int32 i = 0; i < 2; ++i)
	{
		if (Buffer[i])
		{
			Buffer[i] = NULL;
		}
	}
	KeypressBuffer.Empty();
	for (int32 i = 0; i < CompressedFrames.Num(); ++i)
	{
		delete CompressedFrames[i];
	}
	CompressedFrames.Empty();

	bIsRunning = false;
}


void FCrashVideoCapture::CleanupFinalFrame()
{
	SyncWorkerThread();
	
	if (IsInGameThread() && IsRenderingThreadHealthy())
	{
		UE_LOG(LogCrashTracker, Log, TEXT("Attempting final rendering command flush..."));

		FlushRenderingCommands();
	
		UE_LOG(LogCrashTracker, Log, TEXT("Finished final rendering command flush"));
	}
	else
	{
		UE_LOG(LogCrashTracker, Log, TEXT("Crashed outside of game thread; not flushing render commands"));
	}
}


void FCrashVideoCapture::SetCrashTrackingEnabled(bool bEnabled)
{
	bIsRunning = bEnabled;
}


void FCrashVideoCapture::SaveFinalCapture( const FString& CapturePath )
{
	int32 FinalFrameIndex = CurrentFrameCaptureIndex == 0 ? CrashTrackerConstants::VideoFramesToCapture - 1 : CurrentFrameCaptureIndex - 1;

	FCompressedDataFrame* FinalFrame = NULL;
	TArray<FCompressedDataFrame*> OrderedDataFrames;
	for (int32 i = CurrentFrameCaptureIndex; i != FinalFrameIndex; i = (i + 1) % CrashTrackerConstants::VideoFramesToCapture)
	{
		FCompressedDataFrame* DataFrame = CompressedFrames[i];

		if (DataFrame->ActualSize > 0)
		{
			OrderedDataFrames.Add(DataFrame);
			FinalFrame = DataFrame;
		}
	}

	// duplicate the final frame at the end to fill 1 second so the end viewer
	// can see what happened exactly at the very end
	// this may not be useful with a crash on a non-game thread, because the final frame can't be drawn
	if (FinalFrame)
	{
		int32 ExtraFramesAdded = FMath::TruncToInt(1.f / CrashTrackerConstants::CaptureFrequency);
		for (int32 i = 0; i < ExtraFramesAdded; ++i)
		{
			OrderedDataFrames.Add(FinalFrame);
		}
	}

	DumpOutMJPEGAVI(OrderedDataFrames, CapturePath, Width, Height, FMath::TruncToInt(1.f / CrashTrackerConstants::CaptureFrequency));

	UE_LOG(LogCrashTracker, Log, TEXT("Finished dumping out crash capture video. %i Frames at %ix%i"), OrderedDataFrames.Num(), Width, Height);
}


void FCrashVideoCapture::SyncWorkerThread()
{
	FScopeLock SyncWorkerThreadLock(&SyncWorkerThreadSection);

	if (AsyncTask)
	{
		AsyncTask->EnsureCompletion(/*bDoWorkOnThisThreadIfNotStarted=*/true);
		delete AsyncTask;
		AsyncTask = NULL;
	}
}


void FCrashVideoCapture::Update(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_TotalTime);

	if (bIsRunning)
	{
		// Update the key press buffer times
		for (int32 i = 0; i < KeypressBuffer.Num(); ++i)
		{
			KeypressBuffer[i].Value -= DeltaSeconds;
		}
		
		// check to see if we need to render a new frame
		bool bShouldUpdate = false;
		CurrentAccumSeconds += DeltaSeconds;
		if (CurrentAccumSeconds > CrashTrackerConstants::CaptureFrequency)
		{
			CurrentAccumSeconds -= FMath::TruncToFloat(CurrentAccumSeconds / CrashTrackerConstants::CaptureFrequency) * CrashTrackerConstants::CaptureFrequency;
			bShouldUpdate = true;
		}

		if (bShouldUpdate)
		{
			CleanupKeyPressBuffer(DeltaSeconds);

			TArray<FString> StrippedKeyPressBuffer;
			for (int32 i = 0; i < KeypressBuffer.Num(); ++i)
			{
				StrippedKeyPressBuffer.Add(KeypressBuffer[i].Key);
			}
			
			if (Buffer[CurrentBufferIndex] && Width > 0 && Height > 0)
			{
				IImageWrapperPtr ImageWrapper;

				{
					SCOPE_CYCLE_COUNTER(STAT_SendFrameToCompressor);

					IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );
					ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::JPEG );
					ImageWrapper->SetRaw(Buffer[CurrentBufferIndex], Width * Height * sizeof(FColor), Width, Height, ERGBFormat::RGBA, 8);
				}

				{
					SCOPE_CYCLE_COUNTER(STAT_WaitForPreviousFrameToCompress);

					SyncWorkerThread();
					
					AsyncTask = new FAsyncTask<class FAsyncImageCompress>(ImageWrapper, CompressedFrames[CurrentFrameCaptureIndex]);
					AsyncTask->StartBackgroundTask();
				}
				
				CaptureSlateRenderer->UnmapVirtualScreenBuffer();
			}
			
			++CurrentFrameCaptureIndex;
			if (CurrentFrameCaptureIndex >= CrashTrackerConstants::VideoFramesToCapture)
			{
				CurrentFrameCaptureIndex = 0;
			}

			const bool bPrimaryWorkAreaOnly = false;	// We want all monitors captured for crash reporting
			const FIntRect VirtualScreen = CaptureSlateRenderer->SetupVirtualScreenBuffer(bPrimaryWorkAreaOnly, CrashTrackerConstants::ScreenScaling, nullptr);
			Width = VirtualScreen.Width();
			Height = VirtualScreen.Height();
			CaptureSlateRenderer->CopyWindowsToVirtualScreenBuffer(StrippedKeyPressBuffer);
			CaptureSlateRenderer->MapVirtualScreenBuffer(&Buffer[CurrentBufferIndex]);

			CurrentBufferIndex = (CurrentBufferIndex + 1) % 2;
		}
	}
}


bool FCrashVideoCapture::IsCapturing() const
{
	return bIsRunning != 0;
}


void FCrashVideoCapture::InvalidateFrame()
{
	Buffer[0] = Buffer[1] = NULL;
}


void FCrashVideoCapture::BufferKeyPress(const FString& KeyPress)
{
	KeypressBuffer.Add(TKeyValuePair<FString, float>(KeyPress, CrashTrackerConstants::SecondsToKeyPressFade));
}


void FCrashVideoCapture::CleanupKeyPressBuffer(float DeltaSeconds)
{
	TArray< TKeyValuePair<FString, float> > NewKeyPressBuffer;
	int32 StartKeyPressIndex = FMath::Max(0, KeypressBuffer.Num() - CrashTrackerConstants::MaxKeyPressesInBuffer);
	for (int32 i = StartKeyPressIndex; i < KeypressBuffer.Num(); ++i)
	{
		if (KeypressBuffer[i].Value > 0) {NewKeyPressBuffer.Add(KeypressBuffer[i]);}
	}
	KeypressBuffer = NewKeyPressBuffer;
}


bool FCrashVideoCapture::SaveCaptureNow( const FString& CapturePath )
{
	bool bSavedOk = false;
	UE_LOG(LogCrashTracker, Log, TEXT("Forcing save of crash video capture to %s.") , *CapturePath );

	if (bIsRunning)
	{
		SyncWorkerThread();

		SaveFinalCapture( CapturePath );

		bSavedOk = true;
	}
	return bSavedOk;
}




