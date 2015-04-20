

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

		if (GetOuterAUTWeapon()->GetUTOwner()->GetPendingWeapon() != NULL || !GetOuterAUTWeapon()->GetUTOwner()->IsPendingFire(GetOuterAUTWeapon()->GetCurrentFireMode()) || !GetOuterAUTWeapon()->HasAmmo(GetOuterAUTWeapon()->GetCurrentFireMode()))
		{
			GetOuterAUTWeapon()->GotoActiveState();
		}
		bFinishedCooldown = true;
	}

	virtual void EndFiringSequence(uint8 FireModeNum)
	{
		if (bFinishedCooldown)
		{
			GetOuterAUTWeapon()->GotoActiveState();
		}
	}

	
	
};
