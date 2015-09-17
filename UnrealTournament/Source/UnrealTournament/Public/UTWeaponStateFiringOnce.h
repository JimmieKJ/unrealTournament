

#pragma once

#include "UTWeaponStateFiring.h"
#include "UTWeaponStateFiringOnce.generated.h"

/**
 * 
 */
UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTWeaponStateFiringOnce : public UUTWeaponStateFiring
{
	GENERATED_UCLASS_BODY()

	bool bFinishedCooldown;

UUTWeaponStateFiringOnce(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bFinishedCooldown = false;
}

	virtual void BeginState(const UUTWeaponState* PrevState)
	{
		Super::BeginState(PrevState);
		bFinishedCooldown = false;
	}

	virtual void RefireCheckTimer()
	{
		// tell bot we're done
		AUTBot* B = Cast<AUTBot>(GetUTOwner()->Controller);
		if (B != NULL)
		{
			GetUTOwner()->StopFiring();
		}

		bFinishedCooldown = true;
		EndFiringSequence(GetOuterAUTWeapon()->GetCurrentFireMode());
	}

	virtual void EndFiringSequence(uint8 FireModeNum)
	{
		if (bFinishedCooldown && GetUTOwner() && GetOuterAUTWeapon())
		{
			GetUTOwner()->ClearPendingFire();
			GetOuterAUTWeapon()->GotoActiveState();
		}
	}

	
	
};
