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
	bStageSequence = false;
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
	// Ensure the LevelSequence is up to date with the LevelSequenceActorId (Level sequence is only there for a nice UI)
	LevelSequenceActor = FUniqueObjectGuid(LevelSequenceActorId);

	// Attempt to find the level sequence actor
	ALevelSequenceActor* Actor = LevelSequenceActor.Get();
	if (Actor)
	{
		// Ensure that the sequence cannot play by itself. This is safe because this code is called before BeginPlay()
		Actor->bAutoPlay = false;
	}
	
	PrerollTime = 0.f;
	CaptureStrategy = MakeShareable(new FFixedTimeStepCaptureStrategy(Settings.FrameRate));

	Super::Initialize(InViewport);
}

void UAutomatedLevelSequenceCapture::Tick(float DeltaSeconds)
{
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
		// First off, check if we need to stage the sequence
		if (bStageSequence)
		{
			Actor->SequencePlayer->Play();
			Actor->SequencePlayer->Pause();
			bStageSequence = false;
		}

		// Then handle any preroll
		if (PrerollTime.IsSet())
		{
			float& Value = PrerollTime.GetValue();
			Value += DeltaSeconds;
			if (Value >= PrerollAmount)
			{
				if (!OnPlayerUpdatedBinding.IsValid())
				{
					// Bind to the event so we know when to capture a frame
					OnPlayerUpdatedBinding = Actor->SequencePlayer->OnSequenceUpdated().AddUObject(this, &UAutomatedLevelSequenceCapture::SequenceUpdated);
				}

				StartCapture();
				Actor->SequencePlayer->Play();

				PrerollTime.Reset();
			}
		}

		// Otherwise we can quit if we've processed all the frames
		else if (OutstandingFrameCount == 0)
		{
			StopCapture();
		}
	}
}

void UAutomatedLevelSequenceCapture::OnCaptureStopped()
{
	Close();
	FPlatformMisc::RequestExit(0);
}

void UAutomatedLevelSequenceCapture::SequenceUpdated(const ULevelSequencePlayer& Player, float CurrentTime, float PreviousTime)
{
	// Grab this frame
	PrepareForScreenshot();
	if (!Player.IsPlaying())
	{
		Player.OnSequenceUpdated().Remove(OnPlayerUpdatedBinding);
	}
}

void UAutomatedLevelSequenceCapture::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	LevelSequenceActorId = LevelSequenceActor.GetUniqueID().GetGuid();
}

#endif