// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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


void UNiagaraSequence::BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject)
{
}


bool UNiagaraSequence::CanPossessObject(UObject& Object) const
{
	return false;
}


UObject* UNiagaraSequence::FindObject(const FGuid& ObjectId) const
{
	return nullptr;
}


FGuid UNiagaraSequence::FindObjectId(UObject& Object) const
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


#if WITH_EDITOR
bool UNiagaraSequence::TryGetObjectDisplayName(const FGuid& ObjectId, FText& OutDisplayName) const
{
	return false;
}
#endif


void UNiagaraSequence::UnbindPossessableObjects(const FGuid& ObjectId)
{
}
