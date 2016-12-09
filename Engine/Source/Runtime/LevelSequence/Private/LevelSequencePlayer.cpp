// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LevelSequencePlayer.h"
#include "GameFramework/Actor.h"
#include "MovieScene.h"
#include "Misc/CoreDelegates.h"
#include "EngineGlobals.h"
#include "Camera/PlayerCameraManager.h"
#include "UObject/Package.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Tickable.h"
#include "Engine/LevelScriptActor.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieSceneSubSection.h"
#include "LevelSequenceSpawnRegister.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreaming.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Sections/MovieSceneCinematicShotSection.h"

struct FTickAnimationPlayers : public FTickableGameObject
{
	TArray<TWeakObjectPtr<ULevelSequencePlayer>> ActiveInstances;
	
	virtual bool IsTickable() const override
	{
		return ActiveInstances.Num() != 0;
	}

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FTickAnimationPlayers, STATGROUP_Tickables);
	}
	
	virtual void Tick(float DeltaSeconds) override
	{
		for (int32 Index = 0; Index < ActiveInstances.Num();)
		{
			if (auto* Player = ActiveInstances[Index].Get())
			{
				Player->Update(DeltaSeconds);
				++Index;
			}
			else
			{
				ActiveInstances.RemoveAt(Index, 1, false);
			}
		}
	}
};

struct FAutoDestroyAnimationTicker
{
	FAutoDestroyAnimationTicker()
	{
		FCoreDelegates::OnPreExit.AddLambda([&]{
			Impl.Reset();
		});
	}
	
	void Add(ULevelSequencePlayer* Player)
	{
		if (!Impl.IsValid())
		{
			Impl.Reset(new FTickAnimationPlayers);
		}
		Impl->ActiveInstances.Add(Player);
	}

	TUniquePtr<FTickAnimationPlayers> Impl;
} GAnimationPlayerTicker;

/* ULevelSequencePlayer structors
 *****************************************************************************/

ULevelSequencePlayer::ULevelSequencePlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LevelSequence(nullptr)
	, bIsPlaying(false)
	, bReversePlayback(false)
	, TimeCursorPosition(0.0f)
	, StartTime(0.f)
	, EndTime(0.f)
	, CurrentNumLoops(0)
	, bHasCleanedUpSequence(false)
	, bIsEvaluating(false)
{
	SpawnRegister = MakeShareable(new FLevelSequenceSpawnRegister);
}


/* ULevelSequencePlayer interface
 *****************************************************************************/

ULevelSequencePlayer* ULevelSequencePlayer::CreateLevelSequencePlayer(UObject* WorldContextObject, ULevelSequence* InLevelSequence, FLevelSequencePlaybackSettings Settings)
{
	if (InLevelSequence == nullptr)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	check(World != nullptr);

	ULevelSequencePlayer* NewPlayer = NewObject<ULevelSequencePlayer>(GetTransientPackage(), NAME_None, RF_Transient);
	check(NewPlayer != nullptr);

	NewPlayer->Initialize(InLevelSequence, World, Settings);

	// Automatically tick this player
	GAnimationPlayerTicker.Add(NewPlayer);

	return NewPlayer;
}


bool ULevelSequencePlayer::IsPlaying() const
{
	return bIsPlaying;
}


void ULevelSequencePlayer::Pause()
{
	if (bIsPlaying)
	{
		if (bIsEvaluating)
		{
			LatentActions.Add(ELatentAction::Pause);
			return;
		}

		bIsPlaying = false;

		// Evaluate the sequence at its current time, with a status of 'stopped' to ensure that animated state pauses correctly
		{
			TGuardValue<bool> Guard(bIsEvaluating, true);

			UMovieSceneSequence* MovieSceneSequence = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root);
			TOptional<float> FixedFrameInterval = MovieSceneSequence->GetMovieScene() ? MovieSceneSequence->GetMovieScene()->GetOptionalFixedFrameInterval() : TOptional<float>();

			FMovieSceneEvaluationRange Range = PlayPosition.JumpTo(GetSequencePosition(), FixedFrameInterval);

			const FMovieSceneContext Context(Range, EMovieScenePlayerStatus::Stopped);
			RootTemplateInstance.Evaluate(Context, *this);
		}

		ApplyLatentActions();

		if (OnPause.IsBound())
		{
			OnPause.Broadcast();
		}
	}
}

void ULevelSequencePlayer::Stop()
{
	if (bIsPlaying)
	{
		if (bIsEvaluating)
		{
			LatentActions.Add(ELatentAction::Stop);
			return;
		}

		bIsPlaying = false;
		TimeCursorPosition = PlaybackSettings.PlayRate < 0.f ? GetLength() : 0.f;
		CurrentNumLoops = 0;

		RootTemplateInstance.Finish(*this);

		SetTickPrerequisites(false);

		if (OnStop.IsBound())
		{
			OnStop.Broadcast();
		}
	}
}

void GetDescendantSequences(UMovieSceneSequence* InSequence, TArray<UMovieSceneSequence*> & InSequences)
{
	if (InSequence == nullptr)
	{
		return;
	}

	InSequences.Add(InSequence);
	
	UMovieScene* MovieScene = InSequence->GetMovieScene();
	if (MovieScene == nullptr)
	{
		return;
	}

	for (auto MasterTrack : MovieScene->GetMasterTracks())
	{
		for (auto Section : MasterTrack->GetAllSections())
		{
			UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
			if (SubSection != nullptr)
			{
				UMovieSceneSequence* SubSequence = SubSection->GetSequence();
				if (SubSequence != nullptr)
				{
					GetDescendantSequences(SubSequence, InSequences);
				}
			}
		}
	}
}

void ULevelSequencePlayer::SetTickPrerequisites(bool bAddTickPrerequisites)
{
	AActor* LevelSequenceActor = Cast<AActor>(GetOuter());
	if (LevelSequenceActor == nullptr)
	{
		return;
	}

	// @todo: this should happen procedurally, rather than up front. The current approach will not scale well.
	for (auto& Pair : RootTemplateInstance.GetHierarchy().AllSubSequenceData())
	{
		UMovieScene* MovieScene = Pair.Value.Sequence ? Pair.Value.Sequence->GetMovieScene() : nullptr;
		if (!MovieScene)
		{
			continue;
		}

		TArray<AActor*> ControlledActors;
		FMovieSceneSequenceID SequenceID = Pair.Key;

		for (int32 PossessableCount = 0; PossessableCount < MovieScene->GetPossessableCount(); ++PossessableCount)
		{
			FMovieScenePossessable& Possessable = MovieScene->GetPossessable(PossessableCount);

			for (TWeakObjectPtr<> PossessableObject : FindBoundObjects(Possessable.GetGuid(), SequenceID))
			{
				AActor* PossessableActor = Cast<AActor>(PossessableObject.Get());
				if (PossessableActor != nullptr)
				{
					ControlledActors.Add(PossessableActor);
				}
			}
		}

		// @todo: should this happen on spawn, not here?
		// @todo: does setting tick prerequisites on spawnable *templates* actually propagate to spawned instances?
		for (int32 SpawnableCount = 0; SpawnableCount < MovieScene->GetSpawnableCount(); ++ SpawnableCount)
		{
			FMovieSceneSpawnable& Spawnable = MovieScene->GetSpawnable(SpawnableCount);

			UObject* SpawnableObject = Spawnable.GetObjectTemplate();
			if (SpawnableObject != nullptr)
			{
				AActor* SpawnableActor = Cast<AActor>(SpawnableObject);

				if (SpawnableActor != nullptr)
				{
					ControlledActors.Add(SpawnableActor);
				}
			}
		}


		for (AActor* ControlledActor : ControlledActors)
		{
			for( UActorComponent* Component : ControlledActor->GetComponents() )
			{
				if (bAddTickPrerequisites)
				{
					Component->PrimaryComponentTick.AddPrerequisite(LevelSequenceActor, LevelSequenceActor->PrimaryActorTick);
				}
				else
				{
					Component->PrimaryComponentTick.RemovePrerequisite(LevelSequenceActor, LevelSequenceActor->PrimaryActorTick);
				}
			}

			if (bAddTickPrerequisites)
			{
				ControlledActor->PrimaryActorTick.AddPrerequisite(LevelSequenceActor, LevelSequenceActor->PrimaryActorTick);
			}
			else
			{
				ControlledActor->PrimaryActorTick.RemovePrerequisite(LevelSequenceActor, LevelSequenceActor->PrimaryActorTick);
			}
		}
	}
}

void ULevelSequencePlayer::Play()
{
	bReversePlayback = false;

	PlayInternal();
}

void ULevelSequencePlayer::PlayReverse() 
{
	bReversePlayback = true;

	PlayInternal();
}

void ULevelSequencePlayer::ChangePlaybackDirection()
{
	bReversePlayback = !bReversePlayback;

	PlayInternal();
}

void ULevelSequencePlayer::PlayInternal()
{
	if (!bIsPlaying)
	{
		// Start playing
		StartPlayingNextTick();

		// Update now
		bPendingFirstUpdate = false;

		UMovieSceneSequence* MovieSceneSequence = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root);
		TOptional<float> FixedFrameInterval = MovieSceneSequence->GetMovieScene() ? MovieSceneSequence->GetMovieScene()->GetOptionalFixedFrameInterval() : TOptional<float>();

		FMovieSceneEvaluationRange Range = PlayPosition.JumpTo(GetSequencePosition(), FixedFrameInterval);
		UpdateMovieSceneInstance(Range);

		if (OnPlay.IsBound())
		{
			OnPlay.Broadcast();
		}
	}
}

void ULevelSequencePlayer::PlayLooping(int32 NumLoops)
{
	PlaybackSettings.LoopCount = NumLoops;
	PlayInternal();
}

void ULevelSequencePlayer::StartPlayingNextTick()
{
	if ((LevelSequence == nullptr) || !World.IsValid())
	{
		return;
	}

	// @todo Sequencer playback: Should we recreate the instance every time?
	// We must not recreate the instance since it holds stateful information (such as which objects it has spawned). Recreating the instance would break any 
	// @todo: Is this still the case now that eval state is stored (correctly) in the player?
	if (!RootTemplateInstance.IsValid())
	{
		RootTemplateInstance.Initialize(*LevelSequence, *this);
	}

	SetTickPrerequisites(true);

	bPendingFirstUpdate = true;
	bIsPlaying = true;
	bHasCleanedUpSequence = false;
}

float ULevelSequencePlayer::GetPlaybackPosition() const
{
	return TimeCursorPosition;
}

void ULevelSequencePlayer::SetPlaybackPosition(float NewPlaybackPosition)
{
	UpdateTimeCursorPosition(NewPlaybackPosition);
}

float ULevelSequencePlayer::GetLength() const
{
	return EndTime - StartTime;
}

float ULevelSequencePlayer::GetPlayRate() const
{
	return PlaybackSettings.PlayRate;
}

void ULevelSequencePlayer::SetPlayRate(float PlayRate)
{
	PlaybackSettings.PlayRate = PlayRate;
}

void ULevelSequencePlayer::SetPlaybackRange( const float NewStartTime, const float NewEndTime )
{
	StartTime = NewStartTime;
	EndTime = FMath::Max(NewEndTime, StartTime);

	TimeCursorPosition = FMath::Clamp(TimeCursorPosition, 0.f, GetLength());
}

void ULevelSequencePlayer::UpdateTimeCursorPosition(float NewPosition)
{
	float Length = GetLength();

	UMovieSceneSequence* MovieSceneSequence = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root);
	TOptional<float> FixedFrameInterval = MovieSceneSequence->GetMovieScene() ? MovieSceneSequence->GetMovieScene()->GetOptionalFixedFrameInterval() : TOptional<float>();

	if (bPendingFirstUpdate)
	{
		NewPosition = TimeCursorPosition;
		bPendingFirstUpdate = false;
	}

	if (ShouldStopOrLoop(NewPosition))
	{
		// loop playback
		if (PlaybackSettings.LoopCount < 0 || CurrentNumLoops < PlaybackSettings.LoopCount)
		{
			++CurrentNumLoops;
			const float Overplay = FMath::Fmod(NewPosition, Length);
			TimeCursorPosition = Overplay < 0 ? Length + Overplay : Overplay;
			
			PlayPosition.Reset(Overplay < 0 ? Length : 0.f);
			FMovieSceneEvaluationRange Range = PlayPosition.PlayTo(GetSequencePosition(), FixedFrameInterval);

			SpawnRegister->ForgetExternallyOwnedSpawnedObjects(State, *this);

			UpdateMovieSceneInstance(Range);
			return;
		}

		// stop playback
		Stop();
		return;
	}

	TimeCursorPosition = NewPosition;

	FMovieSceneEvaluationRange Range = PlayPosition.PlayTo(NewPosition + StartTime, FixedFrameInterval);
	UpdateMovieSceneInstance(Range);
}

bool ULevelSequencePlayer::ShouldStopOrLoop(float NewPosition)
{
	bool bShouldStopOrLoop = false;
	if (!bReversePlayback)
	{
		bShouldStopOrLoop = NewPosition >= GetLength();
	}
	else
	{
		bShouldStopOrLoop = NewPosition < 0.f;
	}

	return bShouldStopOrLoop;
}

/* ULevelSequencePlayer implementation
 *****************************************************************************/

void ULevelSequencePlayer::Initialize(ULevelSequence* InLevelSequence, UWorld* InWorld, const FLevelSequencePlaybackSettings& Settings)
{
	CurrentPlayer = this;
	LevelSequence = InLevelSequence;

	RootTemplateInstance.Initialize(*LevelSequence, *this);

	World = InWorld;
	PlaybackSettings = Settings;

	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		TRange<float> PlaybackRange = MovieScene->GetPlaybackRange();
		SetPlaybackRange(PlaybackRange.GetLowerBoundValue(), PlaybackRange.GetUpperBoundValue());
	}

	// Ensure everything is set up, ready for playback
	Stop();
}


/* IMovieScenePlayer interface
 *****************************************************************************/


void ULevelSequencePlayer::UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut)
{
	// skip missing player controller
	APlayerController* PC = World->GetGameInstance()->GetFirstLocalPlayerController();

	if (PC == nullptr)
	{
		return;
	}

	// skip same view target
	AActor* ViewTarget = PC->GetViewTarget();

	// save the last view target so that it can be restored when the camera object is null
	if (!LastViewTarget.IsValid())
	{
		LastViewTarget = ViewTarget;
	}

	UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromRuntimeObject(CameraObject);
	
	CachedCameraComponent = CameraComponent;

	if (CameraObject == ViewTarget)
	{
		if ( bJumpCut )
		{
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->bGameCameraCutThisFrame = true;
			}

			if (CameraComponent)
			{
				CameraComponent->NotifyCameraCut();
			}
		}
		return;
	}

	// skip unlocking if the current view target differs
	AActor* UnlockIfCameraActor = Cast<AActor>(UnlockIfCameraObject);

	// if unlockIfCameraActor is valid, release lock if currently locked to object
	if (CameraObject == nullptr && UnlockIfCameraActor != nullptr && UnlockIfCameraActor != ViewTarget)
	{
		return;
	}

	// override the player controller's view target
	AActor* CameraActor = Cast<AActor>(CameraObject);

	// if the camera object is null, use the last view target so that it is restored to the state before the sequence takes control
	if (CameraActor == nullptr)
	{
		CameraActor = LastViewTarget.Get();
	}

	FViewTargetTransitionParams TransitionParams;
	PC->SetViewTarget(CameraActor, TransitionParams);

	if (CameraComponent)
	{
		CameraComponent->NotifyCameraCut();
	}

	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->bClientSimulatingViewTarget = (CameraActor != nullptr);
		PC->PlayerCameraManager->bGameCameraCutThisFrame = true;
	}
}

void ULevelSequencePlayer::SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap)
{
}

void ULevelSequencePlayer::GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const
{
}

EMovieScenePlayerStatus::Type ULevelSequencePlayer::GetPlaybackStatus() const
{
	return bIsPlaying ? EMovieScenePlayerStatus::Playing : EMovieScenePlayerStatus::Stopped;
}

void ULevelSequencePlayer::SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus)
{
}

UObject* ULevelSequencePlayer::GetPlaybackContext() const
{
	return World.Get();
}

TArray<UObject*> ULevelSequencePlayer::GetEventContexts() const
{
	TArray<UObject*> EventContexts;
	if (World.IsValid())
	{
		GetEventContexts(*World, EventContexts);
	}
	return EventContexts;
}

void ULevelSequencePlayer::GetEventContexts(UWorld& InWorld, TArray<UObject*>& OutContexts)
{
	if (InWorld.GetLevelScriptActor())
	{
		OutContexts.Add(InWorld.GetLevelScriptActor());
	}

	for (ULevelStreaming* StreamingLevel : InWorld.StreamingLevels)
	{
		if (StreamingLevel->GetLevelScriptActor())
		{
			OutContexts.Add(StreamingLevel->GetLevelScriptActor());
		}
	}
}

void ULevelSequencePlayer::Update(const float DeltaSeconds)
{
	if (bIsPlaying)
	{
		float PlayRate = bReversePlayback ? -PlaybackSettings.PlayRate : PlaybackSettings.PlayRate;
		UpdateTimeCursorPosition(TimeCursorPosition + DeltaSeconds * PlayRate);
	}
	else if (!bHasCleanedUpSequence && ShouldStopOrLoop(TimeCursorPosition))
	{
		// Update the sequence one last time
		UMovieSceneSequence* MovieSceneSequence = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root);
		TOptional<float> FixedFrameInterval = MovieSceneSequence->GetMovieScene() ? MovieSceneSequence->GetMovieScene()->GetOptionalFixedFrameInterval() : TOptional<float>();
		
		FMovieSceneEvaluationRange Range = PlayPosition.PlayTo(GetSequencePosition(), FixedFrameInterval);
		UpdateMovieSceneInstance(Range);

		bHasCleanedUpSequence = true;
		
		SpawnRegister->ForgetExternallyOwnedSpawnedObjects(State, *this);
		SpawnRegister->CleanUp(*this);
	}
}

void ULevelSequencePlayer::UpdateMovieSceneInstance(FMovieSceneEvaluationRange InRange)
{
	{
		TGuardValue<bool> Guard(bIsEvaluating, true);

		const FMovieSceneContext Context(InRange, GetPlaybackStatus());
		RootTemplateInstance.Evaluate(Context, *this);

	#if WITH_EDITOR
		OnLevelSequencePlayerUpdate.Broadcast(*this, Context.GetTime(), Context.GetPreviousTime());
	#endif
	}

	ApplyLatentActions();
}

void ULevelSequencePlayer::ApplyLatentActions()
{
	// Swap to a stack array to ensure no reentrancy if we evaluate during a pause, for instance
	TArray<ELatentAction> TheseActions;
	Swap(TheseActions, LatentActions);

	for (ELatentAction LatentAction : TheseActions)
	{
		switch(LatentAction)
		{
		case ELatentAction::Stop:	Stop(); break;
		case ELatentAction::Pause:	Pause(); break;
		}
	}
}

void ULevelSequencePlayer::TakeFrameSnapshot(FLevelSequencePlayerSnapshot& OutSnapshot) const
{
	if (!ensure(LevelSequence))
	{
		return;
	}
	
	const float CurrentTime = StartTime + TimeCursorPosition;

	OutSnapshot.Settings = SnapshotSettings;

	OutSnapshot.MasterTime = CurrentTime;
	OutSnapshot.MasterName = FText::FromString(LevelSequence->GetName());

	OutSnapshot.CurrentShotName = OutSnapshot.MasterName;
	OutSnapshot.CurrentShotLocalTime = CurrentTime;
	OutSnapshot.CameraComponent = CachedCameraComponent.IsValid() ? CachedCameraComponent.Get() : nullptr;

	UMovieSceneCinematicShotTrack* ShotTrack = LevelSequence->GetMovieScene()->FindMasterTrack<UMovieSceneCinematicShotTrack>();
	if (ShotTrack)
	{
		int32 HighestRow = TNumericLimits<int32>::Max();
		UMovieSceneCinematicShotSection* ActiveShot = nullptr;
		for (UMovieSceneSection* Section : ShotTrack->GetAllSections())
		{
			if (!ensure(Section))
			{
				continue;
			}

			if (Section->IsActive() && Section->GetRange().Contains(CurrentTime) && (!ActiveShot || Section->GetRowIndex() < ActiveShot->GetRowIndex()))
			{
				ActiveShot = Cast<UMovieSceneCinematicShotSection>(Section);
			}
		}

		if (ActiveShot)
		{
			// Assume that shots with no sequence start at 0.
			const float ShotLowerBound = ActiveShot->GetSequence() 
				? ActiveShot->GetSequence()->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue() 
				: 0;
			const float ShotOffset = ActiveShot->Parameters.StartOffset + ShotLowerBound - ActiveShot->Parameters.PrerollTime;
			const float ShotPosition = ShotOffset + (CurrentTime - (ActiveShot->GetStartTime() - ActiveShot->Parameters.PrerollTime)) / ActiveShot->Parameters.TimeScale;

			OutSnapshot.CurrentShotName = ActiveShot->GetShotDisplayName();
			OutSnapshot.CurrentShotLocalTime = ShotPosition;
		}
	}
}

FMovieSceneSpawnRegister& ULevelSequencePlayer::GetSpawnRegister()
{
	return *SpawnRegister;
}
