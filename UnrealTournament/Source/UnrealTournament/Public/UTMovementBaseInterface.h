// implement this interface to allow Actors to affect the path node graph UT builds on top of the navmesh
// simply implementing the interface makes it a seed point for building and you can optionally
// add functionality to generate special paths with unique properties or requirements (e.g. teleporters)
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTMovementBaseInterface.generated.h"

UINTERFACE(MinimalAPI)
class UUTMovementBaseInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class UNREALTOURNAMENT_API IUTMovementBaseInterface
{
	GENERATED_IINTERFACE_BODY()

	/** Called when UTChar becomes based on this actor.	*/
	UFUNCTION(BlueprintNativeEvent, Category = "MovementBase")
	void AddBasedCharacter(class AUTCharacter* BasedCharacter);

	/** Called when UTChar no longer based on this actor. */
	UFUNCTION(BlueprintNativeEvent, Category = "MovementBase")
	void RemoveBasedCharacter(class AUTCharacter* BasedCharacter);
};
