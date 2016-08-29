// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieScene3DConstraintSection.h"
#include "MovieScene3DAttachSection.generated.h"


class AActor;
class USceneComponent;


/**
 * A 3D Attach section
 */
UCLASS(MinimalAPI)
class UMovieScene3DAttachSection
	: public UMovieScene3DConstraintSection
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Evaluates the attach track
	 *
	 * @param Time The position in time within the movie scene
	 */
	void Eval(USceneComponent* SceneComponent, float Time, AActor* Actor, FVector& OutTranslation, FRotator& OutRotation) const;

	/** 
	 * Adds an attach to the section
	 *
	 * @param Time	The location in time where the attach should be added
	 * @param SequenceEndTime   The time at the end of the sequence, by default the attach is set to end at this time
	 * @param InAttachId The id to the path
	 */
	void AddAttach(float Time, float SequenceEndTime, const FGuid& InAttachId);
	
public:

	UPROPERTY(EditAnywhere, Category="Attach")
	FName AttachSocketName;

	UPROPERTY(EditAnywhere, Category="Attach")
	FName AttachComponentName;

	UPROPERTY(EditAnywhere, Category="Attach")
	uint32 bConstrainTx:1;

	UPROPERTY(EditAnywhere, Category="Attach")
	uint32 bConstrainTy:1;

	UPROPERTY(EditAnywhere, Category="Attach")
	uint32 bConstrainTz:1;

	UPROPERTY(EditAnywhere, Category="Attach")
	uint32 bConstrainRx:1;

	UPROPERTY(EditAnywhere, Category="Attach")
	uint32 bConstrainRy:1;

	UPROPERTY(EditAnywhere, Category="Attach")
	uint32 bConstrainRz:1;
};
