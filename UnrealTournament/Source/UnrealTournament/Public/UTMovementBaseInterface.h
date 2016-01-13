// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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
