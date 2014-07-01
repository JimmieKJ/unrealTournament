// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateFiring.h"

#include "UTWeaponStateFiringBeam.generated.h"

UCLASS()
class UUTWeaponStateFiringBeam : public UUTWeaponStateFiring
{
	GENERATED_UCLASS_BODY()

		/** beam damage accumulator must reach this value before the damage is applied to the target */
	UPROPERTY(EditDefaultsOnly, Category = Damage)
	int32 MinDamage;

	/** current damage fraction (<= MinDamage) */
	UPROPERTY(BlueprintReadWrite, Category = Damage)
	float Accumulator;

	virtual void FireShot() OVERRIDE;
	virtual void EndState() OVERRIDE
	{
		Accumulator = 0.0f;
		Super::EndState();
	}
	virtual void EndFiringSequence(uint8 FireModeNum)
	{
		Super::EndFiringSequence(FireModeNum);
		if (FireModeNum == GetOuterAUTWeapon()->GetCurrentFireMode())
		{
			GetOuterAUTWeapon()->GotoActiveState();
		}
	}
	virtual void Tick(float DeltaTime) OVERRIDE;
};