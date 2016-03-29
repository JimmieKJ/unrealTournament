// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraPrivate.h"
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


bool UNiagaraSequence::CanPossessObject(UObject& Object) const
{
	return false;
}


UObject* UNiagaraSequence::FindPossessableObject(const FGuid& ObjectId, UObject* Context) const
{
	return nullptr;
}


FGuid UNiagaraSequence::FindPossessableObjectId(UObject& Object) const
{
	return FGuid();
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
