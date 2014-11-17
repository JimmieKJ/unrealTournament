// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTSpectatorPawn.generated.h"

UCLASS(CustomConstructor)
class AUTSpectatorPawn : public ASpectatorPawn
{
	GENERATED_UCLASS_BODY()

	AUTSpectatorPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		bAddDefaultMovementBindings = false;
	}
};