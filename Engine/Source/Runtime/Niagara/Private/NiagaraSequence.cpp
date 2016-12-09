// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraSequence.h"


/* UNiagaraSequence structors
 *****************************************************************************/

UNiagaraSequence::UNiagaraSequence(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MovieScene(nullptr)
{ }


/* UMovieSceneAnimation overrides
 *****************************************************************************/


void UNiagaraSequence::BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context)
{
}


bool UNiagaraSequence::CanPossessObject(UObject& Object, UObject* InPlaybackContext) const
{
	return false;
}

UMovieScene* UNiagaraSequence::GetMovieScene() const
{
	return MovieScene;
}


UObject* UNiagaraSequence::GetParentObject(UObject* Object) const
{
	return nullptr;
}


void UNiagaraSequence::UnbindPossessableObjects(const FGuid& ObjectId)
{
}
