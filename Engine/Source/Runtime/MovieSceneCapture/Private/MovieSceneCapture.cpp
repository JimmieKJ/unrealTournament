// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCapturePCH.h"
#include "MovieSceneCapture.h"
#include "ActiveMovieSceneCaptures.h"

#if WITH_EDITOR
#include "ImageWrapper.h"
#endif

#include "MovieSceneCaptureModule.h"

#define LOCTEXT_NAMESPACE "MovieSceneCapture"

struct FUniqueMovieSceneCaptureHandle : FMovieSceneCaptureHandle
{
	FUniqueMovieSceneCaptureHandle()
	{
		/// Start IDs at index 1 since 0 is deemed invalid
		static uint32 Unique = 1;
		ID = Unique++;
	}
};

FMovieSceneCaptureSettings::FMovieSceneCaptureSettings()
	: Resolution(1280, 720)
{
	OutputDirectory.Path = FPaths::VideoCaptureDir();
	OutputFormat = NSLOCTEXT("MovieCapture", "DefaultFormat", "{world}_{width}x{height}").ToString();
	FrameRate = 24;
	FrameCount = 0;
	CaptureType = EMovieCaptureType::AVI;
	bUseCompression = true;
	CompressionQuality = 1.f;
	bEnableTextureStreaming = true;
	bCinematicMode = true;
	bAllowMovement = true;
	bAllowTurning = true;
	bShowPlayer = true;
	bShowHUD = true;
}

UMovieSceneCapture::UMovieSceneCapture(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	bBufferVisualizationDumpFrames = false;

	TArray<FString> Tokens, Switches;
	FCommandLine::Parse( FCommandLine::Get(), Tokens, Switches );
	for (auto& Switch : Switches)
	{
		InheritedCommandLineArguments.AppendChar('-');
		InheritedCommandLineArguments.Append(Switch);
		InheritedCommandLineArguments.AppendChar(' ');
	}

	// the PIEVIACONSOLE parameter tells UGameEngine to add the auto-save dir to the paths array and repopulate the package file cache
	// this is needed in order to support streaming levels as the streaming level packages will be loaded only when needed (thus
	// their package names need to be findable by the package file caching system)
	// (we add to EditorCommandLine because the URL is ignored by WindowsTools)
	AdditionalCommandLineArguments += TEXT("-PIEVIACONSOLE -NoLoadingScreen ");

	Handle = FUniqueMovieSceneCaptureHandle();

	OutstandingFrameCount = 0;
	LastFrameDelta = 0.f;
	bCapturing = false;
}

void UMovieSceneCapture::Initialize(TWeakPtr<FSceneViewport> InSceneViewport)
{
	SceneViewport = InSceneViewport;

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		Ticker.Reset(new FTicker(this));
	}
}

void UMovieSceneCapture::PrepareForScreenshot()
{
	auto Viewport = SceneViewport.Pin();
	if (!Viewport.IsValid())
	{
		return;
	}

	auto ViewportWidget = Viewport->GetViewportWidget().Pin();
	if (!ViewportWidget.IsValid())
	{
		return;
	}

	auto& Application = FSlateApplication::Get();

	// We can't screenshot the widget unless there's a valid window handle to draw it in.
	TSharedPtr<SWindow> WidgetWindow = Application.FindWidgetWindow(ViewportWidget.ToSharedRef());
	if (!WidgetWindow.IsValid())
	{
		return;
	}

	++OutstandingFrameCount;

	FWidgetPath WidgetPath;
	FSlateApplication::Get().GeneratePathToWidgetChecked(ViewportWidget.ToSharedRef(), WidgetPath);

	FArrangedWidget ArrangedWidget = WidgetPath.FindArrangedWidget(ViewportWidget.ToSharedRef()).Get(FArrangedWidget::NullWidget);

	FVector2D Position = ArrangedWidget.Geometry.AbsolutePosition;
	FVector2D WindowPosition = WidgetWindow->GetPositionInScreen();

	const int32 RelativePositionX = Position.X - WindowPosition.X;
	const int32 RelativePositionY = Position.Y - WindowPosition.Y;

	FVector2D Size = ArrangedWidget.Geometry.GetDrawSize();

	FIntRect ScreenshotRect(
		RelativePositionX,
		RelativePositionY,
		RelativePositionX + Size.X,
		RelativePositionY + Size.Y);

	Application.GetRenderer()->PrepareToTakeScreenshot(ScreenshotRect, &ScratchBuffer);
}

void UMovieSceneCapture::Tick(float DeltaSeconds)
{
	if (OutstandingFrameCount > 0)
	{
		CaptureFrame(LastFrameDelta);
		--OutstandingFrameCount;
	}
	LastFrameDelta = DeltaSeconds;
}

void UMovieSceneCapture::StartCapture()
{
	auto Viewport = SceneViewport.Pin();
	if (!Viewport.IsValid())
	{
		return;
	}

	bCapturing = true;

	if (!CaptureStrategy.IsValid())
	{
		CaptureStrategy = MakeShareable(new FRealTimeCaptureStrategy(Settings.FrameRate));
	}

	CaptureStrategy->OnStart();

	CachedMetrics.ElapsedSeconds = 0;

	if (bBufferVisualizationDumpFrames)
	{
		static IConsoleVariable* CVarDumpFrames = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BufferVisualizationDumpFrames"));
		if (CVarDumpFrames)
		{
			CVarDumpFrames->Set(1, ECVF_SetByCommandline);
		}
	}

	CachedMetrics.Width = Viewport->GetSize().X;
	CachedMetrics.Height = Viewport->GetSize().Y;

	if (Settings.CaptureType == EMovieCaptureType::AVI)
	{
		FAVIWriterOptions Options;
		Options.OutputFilename = ResolveUniqueFilename();
		Options.CaptureFPS = Settings.FrameRate;
		Options.CodecName = Settings.Codec;
		Options.bSynchronizeFrames = CaptureStrategy->ShouldSynchronizeFrames();

		if (Settings.bUseCompression)
		{
			Options.CompressionQuality = Settings.CompressionQuality;
		}

		AVIWriter.Reset(FAVIWriter::CreateInstance(Options));
		AVIWriter->StartCapture(Viewport);
	}
}

void UMovieSceneCapture::CaptureFrame(float DeltaSeconds)
{
	auto Viewport = SceneViewport.Pin();
	if (!CaptureStrategy.IsValid() || !Viewport.IsValid() || !ScratchBuffer.Num())
	{
		return;
	}

	CachedMetrics.ElapsedSeconds += DeltaSeconds;

	// By this point, the slate application has already populated our scratch buffer
	if (CaptureStrategy->ShouldPresent(CachedMetrics.ElapsedSeconds, CachedMetrics.Frame))
	{
		TArray<FColor> ThisFrameBuffer;
		Swap(ThisFrameBuffer, ScratchBuffer);

		uint32 NumDroppedFrames = CaptureStrategy->GetDroppedFrames(CachedMetrics.ElapsedSeconds, CachedMetrics.Frame);
		CachedMetrics.Frame += NumDroppedFrames;

		CaptureStrategy->OnPresent(CachedMetrics.ElapsedSeconds, CachedMetrics.Frame);
		++CachedMetrics.Frame;
		
		if (AVIWriter)
		{
			AVIWriter->DropFrames(NumDroppedFrames);
			AVIWriter->Update(CachedMetrics.ElapsedSeconds, MoveTemp(ThisFrameBuffer));
		}
#if WITH_EDITOR
		else
		{
			SaveFrameToFile(MoveTemp(ThisFrameBuffer));
		}
#endif
		if (Settings.FrameCount != 0 && CachedMetrics.Frame >= Settings.FrameCount)
		{
			StopCapture();
		}
	}
}

void UMovieSceneCapture::StopCapture()
{
	Ticker.Reset();

	if (bCapturing)
	{
		OutstandingFrameCount = 0;
		bCapturing = false;

		CaptureStrategy->OnStop();
		CaptureStrategy = nullptr;

		if (AVIWriter)
		{
			AVIWriter->StopCapture();
			AVIWriter.Reset();
		}

		OnCaptureStopped();
	}
}

void UMovieSceneCapture::Close()
{
	StopCapture();
	FActiveMovieSceneCaptures::Get().Remove(this);
}

#if WITH_EDITOR
void UMovieSceneCapture::SaveFrameToFile(TArray<FColor> Colors)
{
	if (Colors.Num() == 0)
	{
		return;
	}

	FString Filename = ResolveUniqueFilename();
	if (Filename.IsEmpty())
	{
		return;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );

	// clamp to 1-100 range
	const int32 ImageCompressionQuality = FMath::Clamp<int32>(Settings.CompressionQuality * 100, 1, 100);

	switch (Settings.CaptureType)
	{
	case EMovieCaptureType::BMP:
		FFileHelper::CreateBitmap(*Filename, CachedMetrics.Width, CachedMetrics.Height, Colors.GetData());
		break;

	case EMovieCaptureType::PNG:
		{
			IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
			for (FColor& Color : Colors)
			{
				Color.A = 255;
			}

			if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(&Colors[0], Colors.Num() * sizeof(FColor), CachedMetrics.Width, CachedMetrics.Height, ERGBFormat::BGRA, 8))
			{
				FFileHelper::SaveArrayToFile(ImageWrapper->GetCompressed(ImageCompressionQuality), *Filename);
			}
		}
		break;

	case EMovieCaptureType::JPEG:
		{
			IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
			if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(&Colors[0], Colors.Num() * sizeof(FColor), CachedMetrics.Width, CachedMetrics.Height, ERGBFormat::BGRA, 8))
			{
				FFileHelper::SaveArrayToFile(ImageWrapper->GetCompressed(ImageCompressionQuality), *Filename);
			}
		}
		break;

	default:
		break;
	}
}
#endif

const TCHAR* UMovieSceneCapture::GetDefaultFileExtension() const
{
	switch (Settings.CaptureType)
	{
		case EMovieCaptureType::BMP:	return TEXT(".bmp");
		case EMovieCaptureType::PNG:	return TEXT(".png");
		case EMovieCaptureType::JPEG:	return TEXT(".jpeg");
#if PLATFORM_MAC
		default:						return TEXT(".mov");
#else
		default:						return TEXT(".avi");
#endif
	}
}

FString UMovieSceneCapture::ResolveFileFormat(const FString& Folder, const FString& Format) const
{
	TMap<FString, FStringFormatArg> Mappings;
	Mappings.Add(TEXT("fps"), FString::Printf(TEXT("%d"), Settings.FrameRate));
	Mappings.Add(TEXT("frame"), FString::Printf(TEXT("%04d"), CachedMetrics.Frame));
	Mappings.Add(TEXT("width"), FString::Printf(TEXT("%d"), CachedMetrics.Width));
	Mappings.Add(TEXT("height"), FString::Printf(TEXT("%d"), CachedMetrics.Height));
	Mappings.Add(TEXT("world"), GWorld->GetName());

	if (Settings.bUseCompression)
	{
		Mappings.Add(TEXT("quality"), FString::Printf(TEXT("%.2f"), Settings.CompressionQuality));
	}
	else
	{
		Mappings.Add(TEXT("quality"), NSLOCTEXT("MovieCapture", "Uncompressed", "Uncompressed").ToString());
	}

	return Folder / FString::Format(*Format, Mappings);
}

FString UMovieSceneCapture::ResolveUniqueFilename()
{
	FString BaseFilename = ResolveFileFormat(Settings.OutputDirectory.Path, Settings.OutputFormat);
	FString ThisTry = BaseFilename + GetDefaultFileExtension();

	if (!IFileManager::Get().DirectoryExists(*Settings.OutputDirectory.Path))
	{
		IFileManager::Get().MakeDirectory(*Settings.OutputDirectory.Path);
	}

	if (Settings.bOverwriteExisting)
	{
		// Try and delete it first
		while (IFileManager::Get().FileSize(*ThisTry) != -1 && !FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*ThisTry))
		{
			// popup a message box
			FText MessageText = FText::Format(LOCTEXT("UnableToRemoveFile_Format", "The destination file '{0}' could not be deleted because it's in use by another application.\n\nPlease close this application before continuing."), FText::FromString(ThisTry));
			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *MessageText.ToString(), *LOCTEXT("UnableToRemoveFile", "Unable to remove file").ToString());
		}
		return ThisTry;
	}

	if (IFileManager::Get().FileSize(*ThisTry) == -1)
	{
		return ThisTry;
	}

	uint32 Index = 2;
	for (;;)
	{
		ThisTry = BaseFilename + FString::Printf(TEXT(" %02d"), Index) + GetDefaultFileExtension();

		// If the file doesn't exist, we can use that, else, increment the index and try again
		if (IFileManager::Get().FileSize(*ThisTry) == -1)
		{
			return ThisTry;
		}

		++Index;
	}

	return ThisTry;
}

FFixedTimeStepCaptureStrategy::FFixedTimeStepCaptureStrategy(uint32 InTargetFPS)
	: TargetFPS(InTargetFPS)
{
}

void FFixedTimeStepCaptureStrategy::OnStart()
{
	FApp::SetFixedDeltaTime(1.0 / TargetFPS);
	FApp::SetUseFixedTimeStep(true);
}

void FFixedTimeStepCaptureStrategy::OnStop()
{
	FApp::SetUseFixedTimeStep(false);
}

void FFixedTimeStepCaptureStrategy::OnPresent(double CurrentTimeSeconds, uint32 FrameIndex)
{
}

bool FFixedTimeStepCaptureStrategy::ShouldPresent(double CurrentTimeSeconds, uint32 FrameIndex) const
{
	return true;
}

int32 FFixedTimeStepCaptureStrategy::GetDroppedFrames(double CurrentTimeSeconds, uint32 FrameIndex) const
{
	return 0;
}

FRealTimeCaptureStrategy::FRealTimeCaptureStrategy(uint32 InTargetFPS)
	: NextPresentTimeS(0), FrameLength(1.0 / InTargetFPS)
{
}

void FRealTimeCaptureStrategy::OnStart()
{
}

void FRealTimeCaptureStrategy::OnStop()
{
}

void FRealTimeCaptureStrategy::OnPresent(double CurrentTimeSeconds, uint32 FrameIndex)
{
}

bool FRealTimeCaptureStrategy::ShouldPresent(double CurrentTimeSeconds, uint32 FrameIndex) const
{
	return CurrentTimeSeconds >= FrameIndex * FrameLength;
}

int32 FRealTimeCaptureStrategy::GetDroppedFrames(double CurrentTimeSeconds, uint32 FrameIndex) const
{
	uint32 ThisFrame = FMath::FloorToInt(CurrentTimeSeconds / FrameLength);
	if (ThisFrame > FrameIndex)
	{
		return ThisFrame - FrameIndex;
	}
	return 0;
}

#undef LOCTEXT_NAMESPACE