// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCosmetic.h"
#include "UTEyewear.generated.h"

UCLASS(Blueprintable, Abstract)
class UNREALTOURNAMENT_API AUTEyewear : public AUTCosmetic
{
	GENERATED_UCLASS_BODY()

	void OnWearerDeath_Implementation(TSubclassOf<UDamageType> DamageType) override;
};
