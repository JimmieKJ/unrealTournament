// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTDmgType_KillZ.h"

#include "UTWorldSettings.generated.h"

UCLASS(CustomConstructor)
class AUTWorldSettings : public AWorldSettings
{
	GENERATED_UCLASS_BODY()

	AUTWorldSettings(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{
		KillZDamageType = UUTDmgType_KillZ::StaticClass();
	}
};