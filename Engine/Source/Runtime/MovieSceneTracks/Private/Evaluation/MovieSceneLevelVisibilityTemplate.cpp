// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneLevelVisibilityTemplate.h"

#include "Engine/LevelStreaming.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"

bool GetLevelVisibility(const ULevelStreaming& Level)
{
#if WITH_EDITOR
	if (GIsEditor && !Level.GetWorld()->IsPlayInEditor())
	{
		return Level.bShouldBeVisibleInEditor;
	}
	else
#endif
	{
		return Level.bShouldBeVisible;
	}
}

void SetLevelVisibility(ULevelStreaming& Level, bool bVisible)
{
#if WITH_EDITOR
	if (GIsEditor && !Level.GetWorld()->IsPlayInEditor())
	{
		Level.bShouldBeVisibleInEditor = bVisible;
		Level.GetWorld()->FlushLevelStreaming();

		// Iterate over the level's actors
		TArray<AActor*>& Actors = Level.GetLoadedLevel()->Actors;
		for ( int32 ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex )
		{
			AActor* Actor = Actors[ActorIndex];
			if ( Actor )
			{
				if (Actor->bHiddenEdLevel == bVisible )
				{
					Actor->bHiddenEdLevel = !bVisible;
					if ( bVisible )
					{
						Actor->ReregisterAllComponents();
					}
					else
					{
						Actor->UnregisterAllComponents();
					}
				}
			}
		}
	}
	else
#endif
	{
		Level.bShouldBeVisible = bVisible;
	}
}

// TODO: This was copied from LevelStreaming.cpp, it should be in a set of shared utilities somewhere.
FString MakeSafeLevelName(const FName& InLevelName, UWorld& World)
{
	// Special case for PIE, the PackageName gets mangled.
	if (!World.StreamingLevelsPrefix.IsEmpty())
	{
		FString PackageName = World.StreamingLevelsPrefix + FPackageName::GetShortName(InLevelName);
		if (!FPackageName::IsShortPackageName(InLevelName))
		{
			PackageName = FPackageName::GetLongPackagePath(InLevelName.ToString()) + TEXT( "/" ) + PackageName;
		}

		return PackageName;
	}

	return InLevelName.ToString();
}

ULevelStreaming* GetStreamingLevel(FName LevelName, UWorld& World)
{
	if (LevelName != NAME_None)
	{
		FString SafeLevelName = MakeSafeLevelName(LevelName, World);
		if (FPackageName::IsShortPackageName(SafeLevelName))
		{
			// Make sure MyMap1 and Map1 names do not resolve to a same streaming level
			SafeLevelName = TEXT("/") + SafeLevelName;
		}

		for (ULevelStreaming* LevelStreaming : World.StreamingLevels)
		{
			if (LevelStreaming && LevelStreaming->GetWorldAssetPackageName().EndsWith(SafeLevelName, ESearchCase::IgnoreCase))
			{
				return LevelStreaming;
			}
		}
	}

	return nullptr;
}

struct FLevelStreamingPreAnimatedToken : IMovieScenePreAnimatedToken
{
	FLevelStreamingPreAnimatedToken(bool bInIsVisible)
		: bVisible(bInIsVisible)
	{
	}

	virtual void RestoreState(UObject& Object, IMovieScenePlayer& Player) override
	{
		ULevelStreaming* LevelStreaming = CastChecked<ULevelStreaming>(&Object);
		SetLevelVisibility(*LevelStreaming, bVisible);
	}

	bool bVisible;
};

struct FLevelStreamingPreAnimatedTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const override
	{
		ULevelStreaming* LevelStreaming = CastChecked<ULevelStreaming>(&Object);
		return FLevelStreamingPreAnimatedToken(GetLevelVisibility(*LevelStreaming));
	}
};

struct FLevelStreamingSharedTrackData : IPersistentEvaluationData
{
	bool HasAnythingToDo() const
	{
		return VisibilityMap.Num() != 0;
	}

	void AssignLevelVisibilityOverrides(TArrayView<const FName> LevelNames, ELevelVisibility Visibility)
	{
		const int32 Inc = Visibility == ELevelVisibility::Visible ? 1 : -1;
		for (FName Name : LevelNames)
		{
			VisibilityMap.FindOrAdd(Name).VisibilityRequestCount += Inc;
		}
	}
	
	void UnassignLevelVisibilityOverrides(TArrayView<const FName> LevelNames, ELevelVisibility Visibility)
	{
		const int32 Dec = Visibility == ELevelVisibility::Visible ? 1 : -1;
		for (FName Name : LevelNames)
		{
			VisibilityMap.FindOrAdd(Name).VisibilityRequestCount -= Dec;
		}
	}

	void TearDown(IMovieScenePlayer& Player)
	{
		for (auto& Pair : VisibilityMap)
		{
			Pair.Value.VisibilityRequestCount = 0;
		}

		ApplyLevelVisibility(Player);
	}

	void ApplyLevelVisibility(IMovieScenePlayer& Player)
	{
		UWorld* World = Player.GetPlaybackContext()->GetWorld();
		if (!World)
		{
			return;
		}

		FLevelStreamingPreAnimatedTokenProducer TokenProducer;

		TArray<FName, TInlineAllocator<8>> LevelsToRestore;

		bool bAnythingChanged = false;
		for (auto& Pair : VisibilityMap)
		{
			FName SafeLevelName(*MakeSafeLevelName(Pair.Key, *World));

			ULevelStreaming* Level = GetLevel(SafeLevelName, *World);
			if (!Level)
			{
				continue;
			}

			if (Pair.Value.VisibilityRequestCount == 0)
			{
				LevelsToRestore.Add(Pair.Key);

				// Restore the state from before our evaluation
				if (Pair.Value.bPreviousState.IsSet())
				{
					SetLevelVisibility(*Level, Pair.Value.bPreviousState.GetValue());
					bAnythingChanged = true;
				}
			}
			else
			{
				const bool bShouldBeVisible = Pair.Value.VisibilityRequestCount > 0;
				if (GetLevelVisibility(*Level) != bShouldBeVisible)
				{
					// Save preanimated state for this track, and globally
					if (!Pair.Value.bPreviousState.IsSet())
					{
						Pair.Value.bPreviousState = GetLevelVisibility(*Level);
					}

					Player.SavePreAnimatedState(*Level, TMovieSceneAnimTypeID<FLevelStreamingSharedTrackData>(), TokenProducer);

					SetLevelVisibility(*Level, bShouldBeVisible);
					bAnythingChanged = true;
				}
			}
		}

		for (FName Level : LevelsToRestore)
		{
			VisibilityMap.Remove(Level);
		}

		if (bAnythingChanged)
		{
			World->FlushLevelStreaming( EFlushLevelStreamingType::Visibility );
		}
	}

private:

	ULevelStreaming* GetLevel(FName SafeLevelName, UWorld& World)
	{
		if (TWeakObjectPtr<ULevelStreaming>* FoundStreamingLevel = NameToLevelMap.Find(SafeLevelName))
		{
			if (ULevelStreaming* Level = FoundStreamingLevel->Get())
			{
				return Level;
			}

			NameToLevelMap.Remove(SafeLevelName);
		}

		ULevelStreaming* Level = GetStreamingLevel(SafeLevelName, World);
		if (Level)
		{
			NameToLevelMap.Add(SafeLevelName, Level);
		}

		return Level;
	}
	
	struct FVisibilityData
	{
		FVisibilityData() : VisibilityRequestCount(0) {}

		/** Ref count of things asking for this level to be (in)visible. > 0 signifies visible, < 0 signifies invisible */
		int32 VisibilityRequestCount;
		TOptional<bool> bPreviousState;
	};
	TMap<FName, FVisibilityData> VisibilityMap;

	TMap<FName, TWeakObjectPtr<ULevelStreaming>> NameToLevelMap;
};

FMovieSceneLevelVisibilitySectionTemplate::FMovieSceneLevelVisibilitySectionTemplate(const UMovieSceneLevelVisibilitySection& Section)
	: Visibility(Section.GetVisibility())
	, LevelNames(Section.GetLevelNames())
{
}

void FMovieSceneLevelVisibilitySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	// Evaluate doesn't actually do anything
}

void FMovieSceneLevelVisibilitySectionTemplate::Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	FLevelStreamingSharedTrackData& TrackData = PersistentData.GetOrAdd<FLevelStreamingSharedTrackData>(FMovieSceneLevelVisibilitySharedTrack::GetSharedDataKey());
	TrackData.AssignLevelVisibilityOverrides(LevelNames, Visibility);
}

void FMovieSceneLevelVisibilitySectionTemplate::TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	// Track data may have already disappeared if the FMovieSceneLevelVisibilitySharedTrack is no longer evaluated either
	FLevelStreamingSharedTrackData* TrackData = PersistentData.Find<FLevelStreamingSharedTrackData>(FMovieSceneLevelVisibilitySharedTrack::GetSharedDataKey());
	if (TrackData)
	{
		TrackData->UnassignLevelVisibilityOverrides(LevelNames, Visibility);
	}
}

FSharedPersistentDataKey FMovieSceneLevelVisibilitySharedTrack::GetSharedDataKey()
{
	static FMovieSceneSharedDataId DataId(FMovieSceneSharedDataId::Allocate());
	return FSharedPersistentDataKey(DataId, FMovieSceneEvaluationOperand());
}

void FMovieSceneLevelVisibilitySharedTrack::TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	FLevelStreamingSharedTrackData* TrackData = PersistentData.Find<FLevelStreamingSharedTrackData>(GetSharedDataKey());
	if (TrackData && TrackData->HasAnythingToDo())
	{
		TrackData->TearDown(Player);

		PersistentData.Reset(GetSharedDataKey());
	}
}

struct FLevelVisibilityExecutionToken : IMovieSceneExecutionToken
{
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		FLevelStreamingSharedTrackData* TrackData = PersistentData.Find<FLevelStreamingSharedTrackData>(FMovieSceneLevelVisibilitySharedTrack::GetSharedDataKey());
		if (TrackData)
		{
			TrackData->ApplyLevelVisibility(Player);
		}
	}
};

void FMovieSceneLevelVisibilitySharedTrack::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	const FLevelStreamingSharedTrackData* TrackData = PersistentData.Find<FLevelStreamingSharedTrackData>(GetSharedDataKey());
	if (TrackData && TrackData->HasAnythingToDo())
	{
		ExecutionTokens.Add(FLevelVisibilityExecutionToken());
	}
}
