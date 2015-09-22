

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

		GetUTOwner()->SetPendingFire(GetOuterAUTWeapon()->GetCurrentFireMode(), false);
		GetOuterAUTWeapon()->GotoActiveState();
		bFinishedCooldown = true;
	}
};
