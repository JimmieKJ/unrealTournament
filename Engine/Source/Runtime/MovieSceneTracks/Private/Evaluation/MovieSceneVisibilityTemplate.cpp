// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneVisibilityTemplate.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

DECLARE_CYCLE_STAT(TEXT("Visibility Track Evaluate"), MovieSceneEval_VisibilityTrack_Evaluate, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Visibility Track Token Execute"), MovieSceneEval_VisibilityTrack_TokenExecute, STATGROUP_MovieSceneEval);

/** A movie scene pre-animated token that stores a pre-animated actor's temporarily hidden in game */
struct FTemporarilyHiddenInGamePreAnimatedToken : IMovieScenePreAnimatedToken
{
	FTemporarilyHiddenInGamePreAnimatedToken(bool bInHidden, bool bInTemporarilyHiddenInGame)
		: bHidden(bInHidden)
		, bTemporarilyHiddenInGame(bInTemporarilyHiddenInGame)
	{}

	virtual void RestoreState(UObject& InObject, IMovieScenePlayer& Player) override
	{
		AActor* Actor = CastChecked<AActor>(&InObject);
		Actor->SetActorHiddenInGame(bHidden);

#if WITH_EDITOR
		Actor->SetIsTemporarilyHiddenInEditor(bTemporarilyHiddenInGame);
#endif // WITH_EDITOR
	}

	bool bHidden;
	bool bTemporarilyHiddenInGame;
};

struct FTemporarilyHiddenInGameTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	/** Cache the existing state of an object before moving it */
	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& InObject) const override
	{
		AActor* Actor = CastChecked<AActor>(&InObject);
		bool bInTemporarilyHiddenInGame = 
#if WITH_EDITOR
			Actor->IsTemporarilyHiddenInEditor();
#else
			false;
#endif
		return FTemporarilyHiddenInGamePreAnimatedToken(Actor->bHidden, bInTemporarilyHiddenInGame);
	}
};

/** A movie scene execution token that stores temporarily hidden in game */
struct FTemporarilyHiddenInGameExecutionToken
	: IMovieSceneExecutionToken
{
	static FMovieSceneAnimTypeID GetAnimTypeID()
	{
		return TMovieSceneAnimTypeID<FTemporarilyHiddenInGameExecutionToken>();
	}
	
	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_VisibilityTrack_TokenExecute);

		PropertyTemplate::TCachedSectionData<bool>& PropertyTrackData = PersistentData.GetSectionData<PropertyTemplate::TCachedSectionData<bool>>();

		for (PropertyTemplate::TCachedValue<bool>& ObjectAndValue : PropertyTrackData.ObjectsAndValues)
		{
			if (UObject* ObjectPtr = ObjectAndValue.WeakObject.Get())
			{
				AActor* Actor = Cast<AActor>(ObjectPtr);
				if (Actor)
				{
					Player.SavePreAnimatedState(*Actor, GetAnimTypeID(), FTemporarilyHiddenInGameTokenProducer());

					Actor->SetActorHiddenInGame(ObjectAndValue.Value);

#if WITH_EDITOR
					if (GIsEditor && Actor->GetWorld() != nullptr && !Actor->GetWorld()->IsPlayInEditor())
					{
						Actor->SetIsTemporarilyHiddenInEditor(ObjectAndValue.Value);
					}
#endif // WITH_EDITOR
				}
			}
		}
	}
};

FMovieSceneVisibilitySectionTemplate::FMovieSceneVisibilitySectionTemplate(const UMovieSceneBoolSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieSceneBoolPropertySectionTemplate(Section, Track)
{}

void FMovieSceneVisibilitySectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_VisibilityTrack_Evaluate);

	using namespace PropertyTemplate;

	for (TCachedValue<bool>& ObjectAndValue : PersistentData.GetSectionData<TCachedSectionData<bool>>().ObjectsAndValues)
	{
		ObjectAndValue.Value = !!BoolCurve.Evaluate(Context.GetTime(), ObjectAndValue.Value ? 1 : 0);

		// Invert this evaluation since the property is "bHiddenInGame" and we want the visualization to be the inverse of that. Green means visible.
		ObjectAndValue.Value = !ObjectAndValue.Value;
	}
	
	ExecutionTokens.Add(FTemporarilyHiddenInGameExecutionToken());
}

