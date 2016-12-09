// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneExecutionTokens.h"

DECLARE_CYCLE_STAT(TEXT("Apply Execution Tokens"), MovieSceneEval_ApplyExecutionTokens, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Apply Execution Token"), MovieSceneEval_ApplyExecutionToken, STATGROUP_MovieSceneEval);

void FMovieSceneExecutionTokens::Apply(IMovieScenePlayer& Player)
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_ApplyExecutionTokens);

	FPersistentEvaluationData PersistentDataProxy(Player.State.PersistentEntityData, Player.State.PersistentSharedData);
	for (FMovieSceneExecutionTokens::FEntry& Entry : Tokens)
	{
		PersistentDataProxy.SetTrackKey(Entry.TrackKey);

		FMovieSceneEvaluationKey SectionKey(Entry.TrackKey.AsSection(Entry.SectionIdentifier));
		PersistentDataProxy.SetSectionKey(SectionKey);
		
		Player.PreAnimatedState.SetCaptureEntity(SectionKey, Entry.CompletionMode);

		{
			MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_ApplyExecutionToken);
			Entry.Token->Execute(Entry.Context, Entry.Operand, PersistentDataProxy, Player);
		}
	}

	Tokens.Reset();
}
