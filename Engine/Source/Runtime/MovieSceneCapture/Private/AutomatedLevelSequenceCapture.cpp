// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCapturePCH.h"

#include "LevelSequencePlayer.h"
#include "AutomatedLevelSequenceCapture.h"
#include "ErrorCodes.h"
#include "SceneViewport.h"
#include "ActiveMovieSceneCaptures.h"
#include "JsonObjectConverter.h"
#include "LevelSequenceBurnIn.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "MovieSceneCaptureHelpers.h"

UAutomatedLevelSequenceCapture::UAutomatedLevelSequenceCapture(const FObjectInitializer& Init)
	: Super(Init)
{
#if WITH_EDITORONLY_DATA == 0
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		checkf(false, TEXT("Automated level sequence captures can only be used in editor builds."));
	}
#else
	bUseCustomStartFrame = false;
	StartFrame = 0;
	bUseCustomEndFrame = false;
	EndFrame = 1;
	WarmUpFrameCount = 0;
	DelayBeforeWarmUp = 0.0f;
	bWriteEditDecisionList = true;

	RemainingDelaySeconds = 0.0f;
	RemainingWarmUpFrames = 0;

	BurnInOptions = Init.CreateDefaultSubobject<ULevelSequenceBurnInOptions>(this, "BurnInOptions");
#endif
}

#if WITH_EDITORONLY_DATA

void UAutomatedLevelSequenceCapture::SetLevelSequenceAsset(FString AssetPath)
{
	LevelSequenceAsset = MoveTemp(AssetPath);
}

void UAutomatedLevelSequenceCapture::AddFormatMappings(TMap<FString, FStringFormatArg>& OutFormatMappings, const FFrameMetrics& FrameMetrics) const
{
	OutFormatMappings.Add(TEXT("shot"), CachedState.CurrentShotName.ToString());

	const int32 FrameNumber = FMath::RoundToInt(CachedState.CurrentShotLocalTime * CachedState.Settings.FrameRate);
	OutFormatMappings.Add(TEXT("shot_frame"), FString::Printf(*FrameNumberFormat, FrameNumber));
}

void UAutomatedLevelSequenceCapture::Initialize(TSharedPtr<FSceneViewport> InViewport, int32 PIEInstance)
{
	// Apply command-line overrides from parent class first. This needs to be called before setting up the capture strategy with the desired frame rate.
	Super::Initialize(InViewport);

	// Apply command-line overrides
	{
		FString LevelSequenceAssetPath;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-LevelSequence=" ), LevelSequenceAssetPath ) )
		{
			LevelSequenceAsset.SetPath( LevelSequenceAssetPath );
		}

		int32 StartFrameOverride;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-MovieStartFrame=" ), StartFrameOverride ) )
		{
			bUseCustomStartFrame = true;
			StartFrame = StartFrameOverride;
		}

		int32 EndFrameOverride;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-MovieEndFrame=" ), EndFrameOverride ) )
		{
			bUseCustomEndFrame = true;
			EndFrame = EndFrameOverride;
		}

		int32 WarmUpFrameCountOverride;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-MovieWarmUpFrames=" ), WarmUpFrameCountOverride ) )
		{
			WarmUpFrameCount = WarmUpFrameCountOverride;
		}

		float DelayBeforeWarmUpOverride;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-MovieDelayBeforeWarmUp=" ), DelayBeforeWarmUpOverride ) )
		{
			DelayBeforeWarmUp = DelayBeforeWarmUpOverride;
		}
	}

	ALevelSequenceActor* Actor = LevelSequenceActor.Get();

	// If we don't have a valid actor, attempt to find a level sequence actor in the world that references this asset
	if( Actor == nullptr )
	{
		if( LevelSequenceAsset.IsValid() )
		{
			ULevelSequence* Asset = Cast<ULevelSequence>( LevelSequenceAsset.TryLoad() );
			if( Asset != nullptr )
			{
				for( auto It = TActorIterator<ALevelSequenceActor>( InViewport->GetClient()->GetWorld() ); It; ++It )
				{
					if( It->LevelSequence == LevelSequenceAsset )
					{
						// Found it!
						Actor = *It;
						this->LevelSequenceActor = Actor;

						break;
					}
				}
			}
		}
	}

	if (!Actor)
	{
		ULevelSequence* Asset = Cast<ULevelSequence>(LevelSequenceAsset.TryLoad());
		if (Asset)
		{
			// Spawn a new actor
			Actor = InViewport->GetClient()->GetWorld()->SpawnActor<ALevelSequenceActor>();
			Actor->SetSequence(Asset);
			// Ensure it doesn't loop (-1 is indefinite)
			Actor->PlaybackSettings.LoopCount = 0;
		
			LevelSequenceActor = Actor;
		}
		else
		{
			//FPlatformMisc::RequestExit(FMovieSceneCaptureExitCodes::AssetNotFound);
		}
	}

	if (Actor)
	{
		Actor->BurnInOptions = BurnInOptions;
		Actor->RefreshBurnIn();

		// Make sure we're not playing yet (in case AutoPlay was called from BeginPlay)
		if( Actor->SequencePlayer != nullptr && Actor->SequencePlayer->IsPlaying() )
		{
			Actor->SequencePlayer->Stop();
		}
		Actor->bAutoPlay = false;
	}

	CaptureState = ELevelSequenceCaptureState::Setup;
	RemainingDelaySeconds = FMath::Max( 0.0f, DelayBeforeWarmUp );
	CaptureStrategy = MakeShareable(new FFixedTimeStepCaptureStrategy(Settings.FrameRate));
}



void UAutomatedLevelSequenceCapture::SetupFrameRange()
{
	ALevelSequenceActor* Actor = LevelSequenceActor.Get();
	if( Actor )
	{
		ULevelSequence* LevelSequence = Cast<ULevelSequence>( Actor->LevelSequence.TryLoad() );
		if( LevelSequence != nullptr )
		{
			UMovieScene* MovieScene = LevelSequence->GetMovieScene();
			if( MovieScene != nullptr )
			{
				const int32 SequenceStartFrame = FMath::RoundToInt( MovieScene->GetPlaybackRange().GetLowerBoundValue() * Settings.FrameRate );
				const int32 SequenceEndFrame = FMath::Max( SequenceStartFrame, FMath::RoundToInt( MovieScene->GetPlaybackRange().GetUpperBoundValue() * Settings.FrameRate ) );

				// Default to playing back the sequence's stored playback range
				int32 PlaybackStartFrame = SequenceStartFrame;
				int32 PlaybackEndFrame = SequenceEndFrame;

				if( bUseCustomStartFrame )
				{
					PlaybackStartFrame = Settings.bUseRelativeFrameNumbers ? ( SequenceStartFrame + StartFrame ) : StartFrame;
				}

				if( !Settings.bUseRelativeFrameNumbers )
				{
					// NOTE: The frame number will be an offset from the first frame that we start capturing on, not the frame
					// that we start playback at (in the case of WarmUpFrameCount being non-zero).  So we'll cache out frame
					// number offset before adjusting for the warm up frames.
					this->FrameNumberOffset = PlaybackStartFrame;
				}

				if( bUseCustomEndFrame )
				{
					PlaybackEndFrame = FMath::Max( PlaybackStartFrame, Settings.bUseRelativeFrameNumbers ? ( SequenceEndFrame + EndFrame ) : EndFrame );

					// We always add 1 to the number of frames we want to capture, because we want to capture both the start and end frames (which if the play range is 0, would still yield a single frame)
					this->FrameCount = ( PlaybackEndFrame - PlaybackStartFrame ) + 1;
				}
				else
				{
					FrameCount = 0;
				}

				RemainingWarmUpFrames = FMath::Max( WarmUpFrameCount, 0 );
				if( RemainingWarmUpFrames > 0 )
				{
					// We were asked to playback additional frames before we start capturing
					PlaybackStartFrame -= RemainingWarmUpFrames;
				}

				// Override the movie scene's playback range
				Actor->SequencePlayer->SetPlaybackRange(
					(float)PlaybackStartFrame / (float)Settings.FrameRate,
					(float)PlaybackEndFrame / (float)Settings.FrameRate );
			}
		}
	}
}


void UAutomatedLevelSequenceCapture::Tick(float DeltaSeconds)
{
	ALevelSequenceActor* Actor = LevelSequenceActor.Get();

	if (!Actor || !Actor->SequencePlayer)
	{
		return;
	}

	// Setup the automated capture
	if (CaptureState == ELevelSequenceCaptureState::Setup)
	{
		SetupFrameRange();
		
		// Bind to the event so we know when to capture a frame
		OnPlayerUpdatedBinding = Actor->SequencePlayer->OnSequenceUpdated().AddUObject( this, &UAutomatedLevelSequenceCapture::SequenceUpdated );

		CaptureState = ELevelSequenceCaptureState::DelayBeforeWarmUp;
	}

	bool bShouldPlaySequence = false;

	// Then we'll just wait a little bit.  We'll delay the specified number of seconds before capturing to allow any
	// textures to stream in or post processing effects to settle.
	if( CaptureState == ELevelSequenceCaptureState::DelayBeforeWarmUp )
	{
		RemainingDelaySeconds -= DeltaSeconds;
		if( RemainingDelaySeconds <= 0.0f )
		{
			RemainingDelaySeconds = 0.0f;

			// Start warming up.  Even if we're not capturing yet, this will make sure we're rendering at a
			// fixed frame rate.
			StartWarmup();

			// Wait a frame to go by after we've set the fixed time step, so that the animation starts
			// playback at a consistent time
			CaptureState = ELevelSequenceCaptureState::ReadyToWarmUp;
		}
	}
	else if( CaptureState == ELevelSequenceCaptureState::ReadyToWarmUp )
	{
		Actor->SequencePlayer->SetSnapshotSettings(FLevelSequenceSnapshotSettings(Settings.ZeroPadFrameNumbers, Settings.FrameRate));
		Actor->SequencePlayer->StartPlayingNextTick();
		// Start warming up
		CaptureState = ELevelSequenceCaptureState::WarmingUp;
	}

	// Count down our warm up frames.
	// The post increment is important - it ensures we capture the very first frame if there are no warm up frames,
	// but correctly skip n frames if there are n warmup frames
	if( CaptureState == ELevelSequenceCaptureState::WarmingUp && RemainingWarmUpFrames-- == 0)
	{
		// Start capturing - this will capture the *next* update from sequencer
		CaptureState = ELevelSequenceCaptureState::FinishedWarmUp;
		UpdateFrameState();
		StartCapture();
	}

	if( bCapturing && !Actor->SequencePlayer->IsPlaying() )
	{
		Actor->SequencePlayer->OnSequenceUpdated().Remove( OnPlayerUpdatedBinding );
		ExportEDL();
		FinalizeWhenReady();
	}
}

void UAutomatedLevelSequenceCapture::SequenceUpdated(const ULevelSequencePlayer& Player, float CurrentTime, float PreviousTime)
{
	if (bCapturing)
	{
		UpdateFrameState();
		CaptureThisFrame(CurrentTime - PreviousTime);
	}
}

void UAutomatedLevelSequenceCapture::UpdateFrameState()
{
	ALevelSequenceActor* Actor = LevelSequenceActor.Get();

	if (Actor && Actor->SequencePlayer)
	{
		Actor->SequencePlayer->TakeFrameSnapshot(CachedState);
	}
}

void UAutomatedLevelSequenceCapture::LoadFromConfig()
{
	UMovieSceneCapture::LoadFromConfig();

	BurnInOptions->LoadConfig();
	BurnInOptions->ResetSettings();
	if (BurnInOptions->Settings)
	{
		BurnInOptions->Settings->LoadConfig();
	}
}

void UAutomatedLevelSequenceCapture::SaveToConfig()
{
	BurnInOptions->SaveConfig();
	if (BurnInOptions->Settings)
	{
		BurnInOptions->Settings->SaveConfig();
	}

	UMovieSceneCapture::SaveToConfig();
}

void UAutomatedLevelSequenceCapture::SerializeAdditionalJson(FJsonObject& Object)
{
	TSharedRef<FJsonObject> OptionsContainer = MakeShareable(new FJsonObject);
	if (FJsonObjectConverter::UStructToJsonObject(BurnInOptions->GetClass(), BurnInOptions, OptionsContainer, 0, 0))
	{
		Object.SetField(TEXT("BurnInOptions"), MakeShareable(new FJsonValueObject(OptionsContainer)));
	}

	if (BurnInOptions->Settings)
	{
		TSharedRef<FJsonObject> SettingsDataObject = MakeShareable(new FJsonObject);
		if (FJsonObjectConverter::UStructToJsonObject(BurnInOptions->Settings->GetClass(), BurnInOptions->Settings, SettingsDataObject, 0, 0))
		{
			Object.SetField(TEXT("BurnInOptionsInitSettings"), MakeShareable(new FJsonValueObject(SettingsDataObject)));
		}
	}
}

void UAutomatedLevelSequenceCapture::DeserializeAdditionalJson(const FJsonObject& Object)
{
	if (!BurnInOptions)
	{
		BurnInOptions = NewObject<ULevelSequenceBurnInOptions>(this, "BurnInOptions");
	}

	TSharedPtr<FJsonValue> OptionsContainer = Object.TryGetField(TEXT("BurnInOptions"));
	if (OptionsContainer.IsValid())
	{
		FJsonObjectConverter::JsonAttributesToUStruct(OptionsContainer->AsObject()->Values, BurnInOptions->GetClass(), BurnInOptions, 0, 0);
	}

	BurnInOptions->ResetSettings();
	if (BurnInOptions->Settings)
	{
		TSharedPtr<FJsonValue> SettingsDataObject = Object.TryGetField(TEXT("BurnInOptionsInitSettings"));
		if (SettingsDataObject.IsValid())
		{
			FJsonObjectConverter::JsonAttributesToUStruct(SettingsDataObject->AsObject()->Values, BurnInOptions->Settings->GetClass(), BurnInOptions->Settings, 0, 0);
		}
	}
}

void UAutomatedLevelSequenceCapture::ExportEDL()
{
	if (!bWriteEditDecisionList)
	{
		return;
	}
	
	ALevelSequenceActor* Actor = LevelSequenceActor.Get();
	ULevelSequencePlayer* Player = Actor ? Actor->SequencePlayer : nullptr;
	ULevelSequence* LevelSequence = Player ? Player->GetLevelSequence() : nullptr;
	UMovieScene* MovieScene = LevelSequence ? LevelSequence->GetMovieScene() : nullptr;

	if (!MovieScene)
	{
		return;
	}

	UMovieSceneCinematicShotTrack* ShotTrack = MovieScene->FindMasterTrack<UMovieSceneCinematicShotTrack>();
	if (!ShotTrack)
	{
		return;
	}

	MovieSceneCaptureHelpers::ExportEDL(MovieScene, Settings.FrameRate, Settings.OutputDirectory.Path);
}


#endif