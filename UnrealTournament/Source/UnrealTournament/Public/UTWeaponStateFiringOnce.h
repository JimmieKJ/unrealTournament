

#pragma once

#include "UTWeaponStateFiring.h"
#include "UTWeaponStateFiringOnce.generated.h"

/**
 * 
 */
UCLASS(CustomConstructor)
class UUTWeaponStateFiringOnce : public UUTWeaponStateFiring
{
	GENERATED_UCLASS_BODY()

	bool bFinishedCooldown;

UUTWeaponStateFiringOnce(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bFinishedCooldown = false;
}

	virtual void BeginState(const UUTWeaponState* PrevState)
	{
		static int bla = 0;
		bla++;

		Super::BeginState(PrevState);
		bFinishedCooldown = false;
	}

	virtual void RefireCheckTimer()
	{
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
