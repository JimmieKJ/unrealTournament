// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject) override;
	virtual bool CanPossessObject(UObject& Object) const override;
	virtual UObject* FindObject(const FGuid& ObjectId) const override;
	virtual FGuid FindObjectId(UObject& Object) const override;
	virtual UMovieScene* GetMovieScene() const override;
	virtual UObject* GetParentObject(UObject* Object) const override;
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) override;

#if WITH_EDITOR
	virtual bool TryGetObjectDisplayName(const FGuid& ObjectId, FText& OutDisplayName) const override;
#endif

	/** Pointer to the movie scene that controls this sequence. */
	UPROPERTY()
	UMovieScene* MovieScene;
};
