// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AIController.h"
#include "UTGhostController.generated.h"

/**
 * 
 */
UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API AUTGhostController : public AAIController
{
	GENERATED_BODY()
	
		AUTGhostController(const FObjectInitializer& OI)
	: Super(OI)
	{}
	
	//Overriden to prevent the ai controller setting the pitch to 0
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn) override {}
	
};
