// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Matinee/MatineeActor.h"
#include "MatineeActorCameraAnim.generated.h"

class UCameraAnim;

/**
 * Actor used to control temporary matinees for camera anims that only exist in the editor
 */
UCLASS(notplaceable, MinimalAPI, NotBlueprintable)
class AMatineeActorCameraAnim : public AMatineeActor
{
	GENERATED_UCLASS_BODY()

	/** The camera anim we are editing */
	UPROPERTY(Transient)
	UCameraAnim* CameraAnim;

	//~ Begin UObject Interface
	virtual bool NeedsLoadForClient() const override
	{ 
		return false; 
	}
	//~ End UObject Interface
};