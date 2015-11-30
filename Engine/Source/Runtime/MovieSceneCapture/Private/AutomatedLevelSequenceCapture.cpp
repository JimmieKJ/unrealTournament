// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCapturePCH.h"

#include "LevelSequencePlayer.h"
#include "AutomatedLevelSequenceCapture.h"
#include "ErrorCodes.h"
#include "ActiveMovieSceneCaptures.h"

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

	RemainingDelaySeconds = 0.0f;
	RemainingWarmUpFrames = 0;
#endif
}

#if WITH_EDITORONLY_DATA

void UAutomatedLevelSequenceCapture::SetLevelSequenceAsset(FString AssetPath)
{
	LevelSequenceAsset = MoveTemp(AssetPath);
}

void UAutomatedLevelSequenceCapture::SetLevelSequenceActor(ALevelSequenceActor* InActor)
{
	LevelSequenceActor = InActor;
	LevelSequenceActorId = LevelSequenceActor.GetUniqueID().GetGuid();
}

void UAutomatedLevelSequenceCapture::Initialize(TWeakPtr<FSceneViewport> InViewport)
{
	// Apply command-line overrides from parent class first. This needs to be called before setting up the capture strategy with the desired frame rate.
	Super::Initialize(InViewport);

	// Apply command-line overrides
	{
		FString LevelSequenceAssetPath;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-LevelSequence=" ), LevelSequenceAssetPath ) )
		{
			LevelSequenceAsset.SetPath( LevelSequenceAssetPath );
			LevelSequenceActorId = FGuid();
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

	// Ensure the LevelSequence is up to date with the LevelSequenceActorId (Level sequence is only there for a nice UI)
	LevelSequenceActor = FUniqueObjectGuid(LevelSequenceActorId);

	ALevelSequenceActor* Actor = LevelSequenceActor.Get();

	// If we don't have a valid actor, attempt to find a level sequence actor in the world that references this asset
	if( Actor == nullptr )
	{
		if( LevelSequenceAsset.IsValid() )
		{
			ULevelSequence* Asset = Cast<ULevelSequence>( LevelSequenceAsset.TryLoad() );
			if( Asset != nullptr )
			{
				for( auto It = TActorIterator<ALevelSequenceActor>( InViewport.Pin()->GetClient()->GetWorld() ); It; ++It )
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

	if (Actor)
	{
		// Make sure we're not playing yet (in case AutoPlay was called from BeginPlay)
		if( Actor->SequencePlayer != nullptr && Actor->SequencePlayer->IsPlaying() )
		{
			Actor->SequencePlayer->Stop();
		}
		Actor->bAutoPlay = false;

		// Bind to the event so we know when to capture a frame
		if( Actor->SequencePlayer != nullptr )
		{
			OnPlayerUpdatedBinding = Actor->SequencePlayer->OnSequenceUpdated().AddUObject( this, &UAutomatedLevelSequenceCapture::SequenceUpdated );
		}
	}

	CaptureState = ELevelSequenceCaptureState::DelayBeforeWarmUp;
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
				}

				// We always add 1 to the number of frames we want to capture, because we want to capture both the start and end frames (which if the play range is 0, would still yield a single frame)
				this->FrameCount = ( PlaybackEndFrame - PlaybackStartFrame ) + 1;

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
	const bool bAnyFramesToCapture = OutstandingFrameCount > 0;
	Super::Tick(DeltaSeconds);

	ALevelSequenceActor* Actor = LevelSequenceActor.Get();

	if (!Actor)
	{
		ULevelSequence* Asset = Cast<ULevelSequence>(LevelSequenceAsset.TryLoad());
		if (Asset)
		{
			// Spawn a new actor
			Actor = GWorld->SpawnActor<ALevelSequenceActor>();
			Actor->SetSequence(Asset);
			// Ensure it doesn't loop (-1 is indefinite)
			Actor->PlaybackSettings.LoopCount = 0;
			
			LevelSequenceActor = Actor;
		}
		else
		{
			FPlatformMisc::RequestExit(FMovieSceneCaptureExitCodes::AssetNotFound);
		}
	}

	if (Actor && Actor->SequencePlayer)
	{
		// First off we'll stage the sequence.  This just means we'll play the first frame of the animation
		// and then pause it immediately
		if( CaptureState == ELevelSequenceCaptureState::Staging )
		{
			Actor->SequencePlayer->Play();
			Actor->SequencePlayer->Pause();

			CaptureState = ELevelSequenceCaptureState::DelayBeforeWarmUp;
		}

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
			Actor->SequencePlayer->Play();
			CaptureState = ELevelSequenceCaptureState::WarmingUp;
		}


		if( CaptureState == ELevelSequenceCaptureState::WarmingUp )
		{
			// Count down our warm up frames
			if( RemainingWarmUpFrames == 0 )
			{
				CaptureState = ELevelSequenceCaptureState::FinishedWarmUp;

				// It's time to start capturing!
				StartCapture();
			}
			else
			{
				// Not ready to capture just yet
				--RemainingWarmUpFrames;
			}
		}


		if( bAnyFramesToCapture && OutstandingFrameCount == 0 )
		{
			// If we hit this, then we've rendered out the last frame and can stop!
			StopCapture();
		}
	}
}

void UAutomatedLevelSequenceCapture::OnCaptureStopped()
{
	ALevelSequenceActor* Actor = LevelSequenceActor.Get();
	if( Actor != nullptr )
	{
		if( Actor->SequencePlayer != nullptr )
		{
			Actor->SequencePlayer->OnSequenceUpdated().Remove( OnPlayerUpdatedBinding );
		}
	}

	Close();
	FPlatformMisc::RequestExit(0);
}

void UAutomatedLevelSequenceCapture::SequenceUpdated(const ULevelSequencePlayer& Player, float CurrentTime, float PreviousTime)
{
	if( bCapturing )
	{
		// Save the previous rendered frame
		CaptureFrame( LastFrameDelta );

		// Get ready to capture the next rendered frame
		PrepareForScreenshot();
	}
}

void UAutomatedLevelSequenceCapture::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	LevelSequenceActorId = LevelSequenceActor.GetUniqueID().GetGuid();
}

#endif