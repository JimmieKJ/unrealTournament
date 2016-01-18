// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTSpectatorPawn.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API AUTSpectatorPawn : public ASpectatorPawn
{
	GENERATED_UCLASS_BODY()

	AUTSpectatorPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		bAddDefaultMovementBindings = false;
	}
};