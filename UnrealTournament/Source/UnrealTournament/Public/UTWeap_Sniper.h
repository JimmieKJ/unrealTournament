// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeap_Sniper.generated.h"

UCLASS(Abstract)
class AUTWeap_Sniper : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	/** target head area size bonus when moving slowly (crouch speed or less) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	float SlowHeadshotScale;
	/** target head area size bonus when moving quickly */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	float RunningHeadshotScale;

	virtual AUTProjectile* FireProjectile();
};