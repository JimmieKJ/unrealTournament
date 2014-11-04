// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeap_LinkGun.generated.h"

UCLASS(Abstract)
class AUTWeap_LinkGun : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintReadOnly, Category = Bio)
	class AUTProj_BioShot* LinkedBio;

	virtual void PlayImpactEffects(const FVector& TargetLoc, uint8 FireMode, const FVector& SpawnLocation, const FRotator& SpawnRotation) override;

	virtual void StopFire(uint8 FireModeNum) override;

	virtual void ServerStopFire_Implementation(uint8 FireModeNum) override;

};