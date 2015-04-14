// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeap_Minigun.generated.h"

UCLASS(Abstract)
class UNREALTOURNAMENT_API AUTWeap_Minigun : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	virtual float GetAISelectRating_Implementation() override;
	virtual bool CanAttack_Implementation(AActor* Target, const FVector& TargetLoc, bool bDirectOnly, bool bPreferCurrentMode, uint8& BestFireMode, FVector& OptimalTargetLoc) override;

	/** Allow firing shard even if not enough ammo left */
	virtual bool HasAmmo(uint8 FireModeNum) override;

};