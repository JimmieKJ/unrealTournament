// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LevelSequencePCH.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "MovieSceneSubSection.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSequenceInstance.h"
#include "MovieSceneTrack.h"
#include "LevelSequenceSpawnRegister.h"
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
	, LastCursorPosition(0.0f)
	, StartTime(0.f)
	, EndTime(0.f)
	, CurrentNumLoops(0)
	, bHasCleanedUpSequence(false)
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
		bIsPlaying = false;

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
		bIsPlaying = false;
		TimeCursorPosition = PlaybackSettings.PlayRate < 0.f ? GetLength() : 0.f;
		CurrentNumLoops = 0;

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

	TArray<UMovieSceneSequence*> AllSequences;
	GetDescendantSequences(LevelSequence, AllSequences);

	for (auto Sequence : AllSequences)
	{
		UMovieScene* MovieScene = Sequence->GetMovieScene();
		if (MovieScene == nullptr)
		{
			continue;
		}

		TArray<AActor*> ControlledActors;

		for (int32 PossessableCount = 0; PossessableCount < MovieScene->GetPossessableCount(); ++PossessableCount)
		{
			FMovieScenePossessable& Possessable = MovieScene->GetPossessable(PossessableCount);
				
			UObject* PossessableObject = Sequence->FindPossessableObject(Possessable.GetGuid(), this);
			if (PossessableObject != nullptr)
			{
				AActor* PossessableActor = Cast<AActor>(PossessableObject);

				if (PossessableActor != nullptr)
				{
					ControlledActors.Add(PossessableActor);
				}
			}
		}

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
		UpdateMovieSceneInstance(TimeCursorPosition, TimeCursorPosition);

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
	if (!RootMovieSceneInstance.IsValid())
	{
		RootMovieSceneInstance = MakeShareable(new FMovieSceneSequenceInstance(*LevelSequence));
		RootMovieSceneInstance->RefreshInstance(*this);
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
	UpdateMovieSceneInstance(TimeCursorPosition, LastCursorPosition);
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
			LastCursorPosition = TimeCursorPosition;

			SpawnRegister->ForgetExternallyOwnedSpawnedObjects(*this);

			return;
		}

		// stop playback
		Stop();
	}

	LastCursorPosition = TimeCursorPosition;
	TimeCursorPosition = NewPosition;
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

void ULevelSequencePlayer::GetRuntimeObjects(TSharedRef<FMovieSceneSequenceInstance> MovieSceneInstance, const FGuid& ObjectId, TArray<TWeakObjectPtr<UObject>>& OutObjects) const
{
	UObject* FoundObject = MovieSceneInstance->FindObject(ObjectId, *this);
	if (FoundObject)
	{
		OutObjects.Add(FoundObject);
	}
}


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

	if ((CameraObject == nullptr) && (UnlockIfCameraActor != ViewTarget))
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

void ULevelSequencePlayer::AddOrUpdateMovieSceneInstance(UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneSequenceInstance> InstanceToAdd)
{
}

void ULevelSequencePlayer::RemoveMovieSceneInstance(UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneSequenceInstance> InstanceToRemove)
{
}

TSharedRef<FMovieSceneSequenceInstance> ULevelSequencePlayer::GetRootMovieSceneSequenceInstance() const
{
	return RootMovieSceneInstance.ToSharedRef();
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
		UpdateMovieSceneInstance(TimeCursorPosition, LastCursorPosition);
	}
	else if (!bHasCleanedUpSequence && ShouldStopOrLoop(TimeCursorPosition))
	{
		UpdateMovieSceneInstance(TimeCursorPosition, LastCursorPosition);

		bHasCleanedUpSequence = true;
		
		SpawnRegister->ForgetExternallyOwnedSpawnedObjects(*this);
		SpawnRegister->CleanUp(*this);
	}
}

void ULevelSequencePlayer::UpdateMovieSceneInstance(float CurrentPosition, float PreviousPosition)
{
	if(RootMovieSceneInstance.IsValid())
	{
		float Position = CurrentPosition + StartTime;
		float LastPosition = PreviousPosition + StartTime;
		UMovieSceneSequence* MovieSceneSequence = RootMovieSceneInstance->GetSequence();
		if (MovieSceneSequence != nullptr && 
			MovieSceneSequence->GetMovieScene()->GetForceFixedFrameIntervalPlayback() &&
			MovieSceneSequence->GetMovieScene()->GetFixedFrameInterval() > 0 )
		{
			float FixedFrameInterval = MovieSceneSequence->GetMovieScene()->GetFixedFrameInterval();
			Position = UMovieScene::CalculateFixedFrameTime( Position, FixedFrameInterval );
			LastPosition = UMovieScene::CalculateFixedFrameTime( LastPosition, FixedFrameInterval );
		}
		EMovieSceneUpdateData UpdateData(Position, LastPosition);
		RootMovieSceneInstance->Update(UpdateData, *this);

#if WITH_EDITOR
		OnLevelSequencePlayerUpdate.Broadcast(*this, CurrentPosition, PreviousPosition);
#endif
	}
}

void ULevelSequencePlayer::TakeFrameSnapshot(FLevelSequencePlayerSnapshot& OutSnapshot) const
{
	const float CurrentTime = StartTime + TimeCursorPosition;

	OutSnapshot.Settings = SnapshotSettings;

	OutSnapshot.MasterTime = CurrentTime;
	OutSnapshot.MasterName = FText::FromString(LevelSequence->GetName());

	OutSnapshot.CurrentShotName = OutSnapshot.MasterName;
	OutSnapshot.CurrentShotLocalTime = CurrentTime;

	UMovieSceneCinematicShotTrack* ShotTrack = LevelSequence->GetMovieScene()->FindMasterTrack<UMovieSceneCinematicShotTrack>();
	if (ShotTrack)
	{
		int32 HighestRow = TNumericLimits<int32>::Max();
		UMovieSceneCinematicShotSection* ActiveShot = nullptr;
		for (UMovieSceneSection* Section : ShotTrack->GetAllSections())
		{
			if (Section->GetRange().Contains(CurrentTime) && (!ActiveShot || Section->GetRowIndex() < ActiveShot->GetRowIndex()))
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
			const float ShotOffset = ActiveShot->StartOffset + ShotLowerBound - ActiveShot->PrerollTime;
			const float ShotPosition = ShotOffset + (CurrentTime - (ActiveShot->GetStartTime() - ActiveShot->PrerollTime)) / ActiveShot->TimeScale;

			OutSnapshot.CurrentShotName = ActiveShot->GetShotDisplayName();
			OutSnapshot.CurrentShotLocalTime = ShotPosition;
		}
	}
}

IMovieSceneSpawnRegister& ULevelSequencePlayer::GetSpawnRegister()
{
	return *SpawnRegister;
}