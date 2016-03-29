// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSequence.h"
#include "NiagaraSequence.generated.h"


/**
 * Movie scene sequence used by Niagara.
 */
UCLASS(BlueprintType, MinimalAPI)
class UNiagaraSequence
	: public UMovieSceneSequence
{
	GENERATED_UCLASS_BODY()

public:

	// UMovieSceneSequence overrides

	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context) override;
	virtual bool CanPossessObject(UObject& Object) const override;
	virtual UObject* FindPossessableObject(const FGuid& ObjectId, UObject* Context) const override;
	virtual FGuid FindPossessableObjectId(UObject& Object) const override;
	virtual UMovieScene* GetMovieScene() const override;
	virtual UObject* GetParentObject(UObject* Object) const override;
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) override;

	/** Pointer to the movie scene that controls this sequence. */
	UPROPERTY()
	UMovieScene* MovieScene;
};
