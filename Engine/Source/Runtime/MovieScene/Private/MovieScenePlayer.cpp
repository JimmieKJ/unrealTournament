// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "MovieSceneSequenceInstance.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"

TSharedRef<FMovieSceneSequenceInstance> IMovieScenePlayer::GetRootMovieSceneSequenceInstance()
{
	return GetEvaluationTemplate().GetInstance(MovieSceneSequenceID::Root)->LegacySequenceInstance.ToSharedRef();
}

void IMovieScenePlayer::GetRuntimeObjects(TSharedRef<FMovieSceneSequenceInstance> MovieSceneInstance, const FGuid& Guid, TArray<TWeakObjectPtr<>>& OutRuntimeObjects)
{
	for (TWeakObjectPtr<> Obj : FindBoundObjects(Guid, MovieSceneInstance->GetSequenceID()))
	{
		OutRuntimeObjects.Add(Obj);
	}
}
