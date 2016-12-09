// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieScenePreAnimatedState.h"

DECLARE_CYCLE_STAT(TEXT("Save Pre Animated State"), MovieSceneEval_SavePreAnimatedState, STATGROUP_MovieSceneEval);

void FMovieSceneSavedObjectTokens::Add(ECapturePreAnimatedState CaptureState, FMovieSceneAnimTypeID InAnimTypeID, FMovieSceneEvaluationKey AssociatedKey, const IMovieScenePreAnimatedTokenProducer& Producer, UObject& InObject, IMovieScenePreAnimatedState& Parent)
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_SavePreAnimatedState)

	switch(CaptureState)
	{
	case ECapturePreAnimatedState::Entity:
		{
			FMovieSceneEntityAndAnimTypeID EntityAndTypeID{AssociatedKey, InAnimTypeID};
			if (AnimatedEntityTypeIDs.Contains(EntityAndTypeID))
			{
				return;
			}
			else
			{
				AnimatedEntityTypeIDs.Add(EntityAndTypeID);

				FRefCountedPreAnimatedObjectToken* Token = EntityTokens.FindByPredicate([=](const FRefCountedPreAnimatedObjectToken& InToken){ return InToken.AnimTypeID == InAnimTypeID; });
				if (!Token)
				{
					EntityTokens.Add(FRefCountedPreAnimatedObjectToken(Producer.CacheExistingState(InObject), InAnimTypeID));
				}
				else
				{
					++Token->RefCount;
				}

				Parent.EntityHasAnimatedObject(AssociatedKey, FObjectKey(&InObject));
			}
		}
		// fallthrough

	case ECapturePreAnimatedState::Global:
		if (!AnimatedGlobalTypeIDs.Contains(InAnimTypeID))
		{
			AnimatedGlobalTypeIDs.Add(InAnimTypeID);
			GlobalStateTokens.Add(Producer.CacheExistingState(InObject));
		}
		break;

	default:
		break;
	}
}

void FMovieSceneSavedObjectTokens::Restore(IMovieScenePlayer& Player)
{
	UObject* ObjectPtr = Object.Get();
	if (!ObjectPtr)
	{
		return;
	}

	// Restore in reverse
	for (int32 Index = GlobalStateTokens.Num() - 1; Index >= 0; --Index)
	{
		GlobalStateTokens[Index]->RestoreState(*ObjectPtr, Player);
	}

	Reset();
}

void FMovieSceneSavedObjectTokens::Restore(IMovieScenePlayer& Player, TFunctionRef<bool(FMovieSceneAnimTypeID)> InFilter)
{
	UObject* ObjectPtr = Object.Get();
	if (!ObjectPtr)
	{
		return;
	}

	TArray<FMovieSceneAnimTypeID> TypesToRemove;

	// Restore in reverse
	for (int32 Index = AnimatedGlobalTypeIDs.Num() - 1; Index >= 0; --Index)
	{
		if (InFilter(AnimatedGlobalTypeIDs[Index]))
		{
			TypesToRemove.Add(AnimatedGlobalTypeIDs[Index]);

			GlobalStateTokens[Index]->RestoreState(*ObjectPtr, Player);

			GlobalStateTokens.RemoveAtSwap(Index);
			AnimatedGlobalTypeIDs.RemoveAtSwap(Index);
		}
	}

	EntityTokens.RemoveAll(
		[&](const FRefCountedPreAnimatedObjectToken& In){
			return TypesToRemove.Contains(In.AnimTypeID);
		}
	);

	AnimatedEntityTypeIDs.RemoveAll(
		[&](const FMovieSceneEntityAndAnimTypeID& In){
			return TypesToRemove.Contains(In.AnimTypeID);
		}
	);
}

void FMovieSceneSavedMasterTokens::Add(ECapturePreAnimatedState CaptureState, FMovieSceneAnimTypeID InAnimTypeID, FMovieSceneEvaluationKey AssociatedKey, const IMovieScenePreAnimatedGlobalTokenProducer& Producer, IMovieScenePreAnimatedState& Parent)
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_SavePreAnimatedState)

	switch(CaptureState)
	{
	case ECapturePreAnimatedState::Entity:
		{
			FMovieSceneEntityAndAnimTypeID EntityAndTypeID{AssociatedKey, InAnimTypeID};
			if (AnimatedEntityTypeIDs.Contains(EntityAndTypeID))
			{
				return;
			}
			else
			{
				AnimatedEntityTypeIDs.Add(EntityAndTypeID);

				FRefCountedPreAnimatedGlobalToken* Token = EntityTokens.FindByPredicate([=](const FRefCountedPreAnimatedGlobalToken& InToken){ return InToken.AnimTypeID == InAnimTypeID; });
				if (!Token)
				{
					EntityTokens.Add(FRefCountedPreAnimatedGlobalToken(Producer.CacheExistingState(), InAnimTypeID));
				}
				else
				{
					++Token->RefCount;
				}

				Parent.EntityHasAnimatedMaster(AssociatedKey);
			}
		}
		// fallthrough

	case ECapturePreAnimatedState::Global:
		if (!AnimatedGlobalTypeIDs.Contains(InAnimTypeID))
		{
			AnimatedGlobalTypeIDs.Add(InAnimTypeID);
			GlobalStateTokens.Add(Producer.CacheExistingState());
		}
		break;

	default:
		break;
	}
}

void FMovieSceneSavedMasterTokens::Restore(IMovieScenePlayer& Player)
{
	// Restore in reverse
	for (int32 Index = GlobalStateTokens.Num() - 1; Index >= 0; --Index)
	{
		GlobalStateTokens[Index]->RestoreState(Player);
	}

	Reset();
}

void FMovieScenePreAnimatedState::RestorePreAnimatedState(IMovieScenePlayer& Player)
{
	for (auto& Pair : ObjectTokens)
	{
		Pair.Value.Restore(Player);
	}

	MasterTokens.Restore(Player);

	EntityToAnimatedObjects.Reset();
}

void FMovieScenePreAnimatedState::RestorePreAnimatedState(IMovieScenePlayer& Player, UObject& Object)
{
	FObjectKey ObjectKey(&Object);

	FMovieSceneSavedObjectTokens* FoundObjectTokens = ObjectTokens.Find(ObjectKey);
	if (FoundObjectTokens)
	{
		FoundObjectTokens->Restore(Player);
	}

	for (auto& Pair : EntityToAnimatedObjects)
	{
		Pair.Value.Remove(ObjectKey);
	}
}

void FMovieScenePreAnimatedState::RestorePreAnimatedState(IMovieScenePlayer& Player, UObject& Object, TFunctionRef<bool(FMovieSceneAnimTypeID)> InFilter)
{
	FMovieSceneSavedObjectTokens* FoundObjectTokens = ObjectTokens.Find(&Object);
	if (FoundObjectTokens)
	{
		FoundObjectTokens->Restore(Player, InFilter);
	}
}
