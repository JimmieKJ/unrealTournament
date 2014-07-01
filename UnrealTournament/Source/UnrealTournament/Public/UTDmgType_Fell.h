// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTDmgType_Fell.generated.h"

UCLASS(CustomConstructor)
class UUTDmgType_Fell : public UUTDamageType
{
	GENERATED_UCLASS_BODY()

	UUTDmgType_Fell(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{
		bCausedByWorld = true;
	}
};