// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTDmgType_KillZ.generated.h"

UCLASS(CustomConstructor)
class UUTDmgType_KillZ : public UUTDamageType
{
	GENERATED_UCLASS_BODY()

	UUTDmgType_KillZ(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{
		bCausedByWorld = true;
	}
};