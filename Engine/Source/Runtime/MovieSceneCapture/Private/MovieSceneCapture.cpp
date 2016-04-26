// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCapturePCH.h"
#include "MovieSceneCapture.h"
#include "ActiveMovieSceneCaptures.h"
#include "HighResScreenshot.h"
#include "BufferVisualizationData.h"
#include "SceneViewExtension.h"

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
	FPaths::MakePlatformFilename( OutputDirectory.Path );

	bCreateTemporaryCopiesOfLevels = false;
	bUseRelativeFrameNumbers = false;
	GameModeOverride = nullptr;
	OutputFormat = TEXT("{world}");
	FrameRate = 24;
	bEnableTextureStreaming = false;
	bCinematicMode = true;
	bAllowMovement = false;
	bAllowTurning = false;
	bShowPlayer = false;
	bShowHUD = false;
}

UMovieSceneCapture::UMovieSceneCapture(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	TArray<FString> Tokens, Switches;
	FCommandLine::Parse( FCommandLine::Get(), Tokens, Switches );
	for (auto& Switch : Switches)
	{
		InheritedCommandLineArguments.AppendChar('-');
		InheritedCommandLineArguments.Append(Switch);
		InheritedCommandLineArguments.AppendChar(' ');
	}

	AdditionalCommandLineArguments += TEXT("-NoLoadingScreen -NOSCREENMESSAGES -ForceRes");

	Handle = FUniqueMovieSceneCaptureHandle();

	FrameCount = 0;
	bCapturing = false;
	FrameNumberOffset = 0;
	CaptureType = TEXT("Video");
}

void UMovieSceneCapture::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		if (ProtocolSettings)
		{
			ProtocolSettings->OnReleaseConfig(Settings);
		}
		ProtocolSettings = IMovieSceneCaptureModule::Get().GetProtocolRegistry().FactorySettingsType(CaptureType, this);
		if (ProtocolSettings)
		{
			ProtocolSettings->LoadConfig();
			ProtocolSettings->OnLoadConfig(Settings);
		}
	}

	Super::PostInitProperties();
}

void UMovieSceneCapture::Initialize(TSharedPtr<FSceneViewport> InSceneViewport, int32 PIEInstance)
{
	ensure(!bCapturing);

	// Apply command-line overrides
	{
		FString OutputPathOverride;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-MovieFolder=" ), OutputPathOverride ) )
		{
			Settings.OutputDirectory.Path = OutputPathOverride;
		}

		FString OutputNameOverride;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-MovieName=" ), OutputNameOverride ) )
		{
			Settings.OutputFormat = OutputNameOverride;
		}

		bool bOverrideOverwriteExisting;
		if( FParse::Bool( FCommandLine::Get(), TEXT( "-MovieOverwriteExisting=" ), bOverrideOverwriteExisting ) )
		{
			Settings.bOverwriteExisting = bOverrideOverwriteExisting;
		}

		bool bOverrideRelativeFrameNumbers;
		if( FParse::Bool( FCommandLine::Get(), TEXT( "-MovieRelativeFrames=" ), bOverrideRelativeFrameNumbers ) )
		{
			Settings.bUseRelativeFrameNumbers = bOverrideRelativeFrameNumbers;
		}

		bool bOverrideCinematicMode;
		if( FParse::Bool( FCommandLine::Get(), TEXT( "-MovieCinematicMode=" ), bOverrideCinematicMode ) )
		{
			Settings.bCinematicMode = bOverrideCinematicMode;
		}

		FString FormatOverride;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-MovieFormat=" ), FormatOverride ) )
		{
			CaptureType = *FormatOverride;
		}

		int32 FrameRateOverride;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-MovieFrameRate=" ), FrameRateOverride ) )
		{
			Settings.FrameRate = FrameRateOverride;
		}
	}

	bFinalizeWhenReady = false;

	InitSettings = FCaptureProtocolInitSettings::FromSlateViewport(InSceneViewport.ToSharedRef(), ProtocolSettings);

	CachedMetrics = FCachedMetrics();
	CachedMetrics.Width = InitSettings->DesiredSize.X;
	CachedMetrics.Height = InitSettings->DesiredSize.Y;

	FormatMappings.Reserve(10);
	FormatMappings.Add(TEXT("fps"), FString::Printf(TEXT("%d"), Settings.FrameRate));
	FormatMappings.Add(TEXT("width"), FString::Printf(TEXT("%d"), CachedMetrics.Width));
	FormatMappings.Add(TEXT("height"), FString::Printf(TEXT("%d"), CachedMetrics.Height));
	FormatMappings.Add(TEXT("world"), GWorld->GetName());

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		FActiveMovieSceneCaptures::Get().Add(this);
	}
}

void UMovieSceneCapture::StartWarmup()
{
	check( !bCapturing );
	if( !CaptureStrategy.IsValid() )
	{
		CaptureStrategy = MakeShareable( new FRealTimeCaptureStrategy( Settings.FrameRate ) );
	}
	CaptureStrategy->OnWarmup();
}

void UMovieSceneCapture::StartCapture()
{
	bFinalizeWhenReady = false;
	bCapturing = true;

	if (!CaptureStrategy.IsValid())
	{
		CaptureStrategy = MakeShareable(new FRealTimeCaptureStrategy(Settings.FrameRate));
	}

	CaptureStrategy->OnStart();

	CaptureProtocol = IMovieSceneCaptureModule::Get().GetProtocolRegistry().Factory(CaptureType);
	if (ensure(CaptureProtocol.IsValid()))
	{
		CaptureProtocol->Initialize(InitSettings.GetValue(), *this);
	}
}

void UMovieSceneCapture::CaptureThisFrame(float DeltaSeconds)
{
	if (!bCapturing || !CaptureStrategy.IsValid() || !CaptureProtocol.IsValid() || bFinalizeWhenReady)
	{
		return;
	}

	CachedMetrics.ElapsedSeconds += DeltaSeconds;
	if (CaptureStrategy->ShouldPresent(CachedMetrics.ElapsedSeconds, CachedMetrics.Frame))
	{
		uint32 NumDroppedFrames = CaptureStrategy->GetDroppedFrames(CachedMetrics.ElapsedSeconds, CachedMetrics.Frame);
		CachedMetrics.Frame += NumDroppedFrames;

		CaptureStrategy->OnPresent(CachedMetrics.ElapsedSeconds, CachedMetrics.Frame);

		const FFrameMetrics ThisFrameMetrics(
			CachedMetrics.ElapsedSeconds,
			DeltaSeconds,
			CachedMetrics.Frame,
			NumDroppedFrames
			);
		CaptureProtocol->CaptureFrame(ThisFrameMetrics, *this);

		++CachedMetrics.Frame;

		if (!bFinalizeWhenReady && (FrameCount != 0 && CachedMetrics.Frame >= FrameCount))
		{
			FinalizeWhenReady();
		}
	}
}

void UMovieSceneCapture::FinalizeWhenReady()
{
	bFinalizeWhenReady = true;
}

void UMovieSceneCapture::Finalize()
{
	FActiveMovieSceneCaptures::Get().Remove(this);

	if (bCapturing)
	{
		bCapturing = false;

		if (CaptureStrategy.IsValid())
		{
			CaptureStrategy->OnStop();
			CaptureStrategy = nullptr;
		}

		if (CaptureProtocol.IsValid())
		{
			CaptureProtocol->Finalize();
			CaptureProtocol = nullptr;
		}

		OnCaptureFinishedDelegate.Broadcast();
	}
}

FString UMovieSceneCapture::ResolveFileFormat(const FString& Format, const FFrameMetrics& FrameMetrics) const
{
	FormatMappings.Add(TEXT("frame"), FString::Printf(TEXT("%04d"), Settings.bUseRelativeFrameNumbers ? FrameMetrics.FrameNumber : FrameMetrics.FrameNumber + FrameNumberOffset));
	
	if (CaptureProtocol.IsValid())
	{
		CaptureProtocol->AddFormatMappings(FormatMappings);
	}
	return FString::Format(*Format, FormatMappings);
}

FString UMovieSceneCapture::GenerateFilename(const FFrameMetrics& FrameMetrics, const TCHAR* Extension) const
{
	FString OutputDirectory = ResolveFileFormat(Settings.OutputDirectory.Path, FrameMetrics);

	if (!IFileManager::Get().DirectoryExists(*OutputDirectory))
	{
		IFileManager::Get().MakeDirectory(*OutputDirectory);
	}

	const FString BaseFilename = OutputDirectory / ResolveFileFormat(Settings.OutputFormat, FrameMetrics);

	FString ThisTry = BaseFilename + Extension;

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

	int32 DuplicateIndex = 1;
	for (;;)
	{
		ThisTry = BaseFilename + FString::Printf(TEXT("_(%d)"), DuplicateIndex) + Extension;

		// If the file doesn't exist, we can use that, else, increment the index and try again
		if (IFileManager::Get().FileSize(*ThisTry) == -1)
		{
			return ThisTry;
		}

		++DuplicateIndex;
	}

	return ThisTry;
}

#if WITH_EDITOR
void UMovieSceneCapture::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.MemberProperty != NULL) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMovieSceneCapture, CaptureType))
	{
		if (ProtocolSettings)
		{
			ProtocolSettings->OnReleaseConfig(Settings);
		}

		ProtocolSettings = IMovieSceneCaptureModule::Get().GetProtocolRegistry().FactorySettingsType(CaptureType, this);
		if (ProtocolSettings)
		{
			ProtocolSettings->LoadConfig();
			ProtocolSettings->OnLoadConfig(Settings);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

FFixedTimeStepCaptureStrategy::FFixedTimeStepCaptureStrategy(uint32 InTargetFPS)
	: TargetFPS(InTargetFPS)
{
}

void FFixedTimeStepCaptureStrategy::OnWarmup()
{
	FApp::SetFixedDeltaTime(1.0 / TargetFPS);
	FApp::SetUseFixedTimeStep(true);
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

void FRealTimeCaptureStrategy::OnWarmup()
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