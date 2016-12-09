// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneSpawnTemplate.h"

#include "MovieSceneSequence.h"
#include "Sections/MovieSceneSpawnSection.h"

DECLARE_CYCLE_STAT(TEXT("Spawn Track Evaluate"), MovieSceneEval_SpawnTrack_Evaluate, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Spawn Track Token Execute"), MovieSceneEval_SpawnTrack_TokenExecute, STATGROUP_MovieSceneEval);

/** A movie scene pre-animated token that stores a pre-animated transform */
struct FSpawnTrackPreAnimatedTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const
	{
		struct FToken : IMovieScenePreAnimatedToken
		{
			virtual void RestoreState(UObject& InObject, IMovieScenePlayer& Player) override
			{
				Player.GetSpawnRegister().DestroyObjectDirectly(InObject);
			}
		};
		
		return FToken();
	}
};

struct FSpawnObjectToken : IMovieSceneExecutionToken
{
	FSpawnObjectToken(bool bInSpawned)
		: bSpawned(bInSpawned)
	{}

	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_SpawnTrack_TokenExecute)

		const bool bHasBoundObjects = Player.FindBoundObjects(Operand).Num() != 0;
		if (bSpawned)
		{
			// If it's not spawned, spawn it
			if (!bHasBoundObjects)
			{
				const UMovieSceneSequence* Sequence = Player.State.FindSequence(Operand.SequenceID);
				if (Sequence)
				{
					Player.GetSpawnRegister().SpawnObject(Operand.ObjectBindingID, *Sequence->GetMovieScene(), Operand.SequenceID, Player);
				}
			}

			// ensure that pre animated state is saved
			for (TWeakObjectPtr<> Object : Player.FindBoundObjects(Operand))
			{
				if (UObject* ObjectPtr = Object.Get())
				{
					Player.SavePreAnimatedState(*ObjectPtr, FMovieSceneSpawnSectionTemplate::GetAnimTypeID(), FSpawnTrackPreAnimatedTokenProducer());
				}
			}
		}
		else if (!bSpawned && bHasBoundObjects)
		{
			Player.GetSpawnRegister().DestroySpawnedObject(Operand.ObjectBindingID, Operand.SequenceID, Player);
		}
	}

	bool bSpawned;
};

FMovieSceneSpawnSectionTemplate::FMovieSceneSpawnSectionTemplate(const UMovieSceneSpawnSection& SpawnSection)
	: Curve(SpawnSection.GetCurve())
{
}

void FMovieSceneSpawnSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_SpawnTrack_Evaluate)

	const float Time = Context.GetTime();
	ExecutionTokens.Add(
		FSpawnObjectToken(Curve.Evaluate(Time) != 0)
		);
}

FMovieSceneAnimTypeID FMovieSceneSpawnSectionTemplate::GetAnimTypeID()
{
	return TMovieSceneAnimTypeID<FMovieSceneSpawnSectionTemplate>();
}
