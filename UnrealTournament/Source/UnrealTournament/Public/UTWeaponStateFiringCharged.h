// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateFiring.h"

#include "UTWeaponStateFiringCharged.generated.h"

UCLASS(CustomConstructor)
class UUTWeaponStateFiringCharged : public UUTWeaponStateFiring
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateFiringCharged(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	/** looping animation played while charging */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	UAnimMontage* ChargingLoopAnim;

	bool bCharging;
	float ChargeTime;

	virtual void BeginState(const UUTWeaponState* PrevState) override
	{
		ToggleLoopingEffects(true);
		GetOuterAUTWeapon()->OnStartedFiring();
		GetOuterAUTWeapon()->DeactivateSpawnProtection();
		bCharging = true;
	}
	virtual void EndState() override
	{
		if (bCharging)
		{
			EndFiringSequence(GetFireMode());
		}
		Super::EndState();
	}
	virtual void UpdateTiming() override
	{
		if (!bCharging)
		{
			Super::UpdateTiming();
		}
		else
		{
			// update anim speed
			if (ChargingLoopAnim != NULL)
			{
				UAnimInstance* AnimInstance = GetOuterAUTWeapon()->Mesh->GetAnimInstance();
				if (AnimInstance != NULL && AnimInstance->GetCurrentActiveMontage() == ChargingLoopAnim)
				{
					AnimInstance->Montage_SetPlayRate(ChargingLoopAnim, GetUTOwner()->GetFireRateMultiplier());
				}
			}
		}
	}
	virtual void RefireCheckTimer() override
	{
		// query bot to consider whether to still fire, switch modes, etc
		AUTBot* B = Cast<AUTBot>(GetUTOwner()->Controller);
		if (B != NULL)
		{
			B->CheckWeaponFiring();
		}

		if (GetOuterAUTWeapon()->GetUTOwner()->GetPendingWeapon() != NULL || !GetOuterAUTWeapon()->GetUTOwner()->IsPendingFire(GetOuterAUTWeapon()->GetCurrentFireMode()) || !GetOuterAUTWeapon()->HasAmmo(GetOuterAUTWeapon()->GetCurrentFireMode()))
		{
			GetOuterAUTWeapon()->GotoActiveState();
		}
		else
		{
			ToggleLoopingEffects(true);
			GetOuterAUTWeapon()->OnContinuedFiring();
			bCharging = true;
		}
	}
	virtual void EndFiringSequence(uint8 FireModeNum)
	{
		if (bCharging && FireModeNum == GetFireMode())
		{
			ToggleLoopingEffects(false);
			FireShot();
			bCharging = false;
			ChargeTime = 0.0f;
			if (GetOuterAUTWeapon()->GetCurrentState() == this)
			{
				GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateFiring::RefireCheckTimer, GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()), false);
			}
		}
	}
	virtual void Tick(float DeltaTime) override
	{
		if (bCharging)
		{
			ChargeTime += DeltaTime * GetOuterAUTWeapon()->GetUTOwner()->GetFireRateMultiplier();
		}
	}

	virtual void ToggleLoopingEffects(bool bNowOn) override
	{
		Super::ToggleLoopingEffects(bNowOn);

		if (ChargingLoopAnim != NULL)
		{
			UAnimInstance* AnimInstance = GetOuterAUTWeapon()->Mesh->GetAnimInstance();
			if (AnimInstance != NULL)
			{
				if (bNowOn)
				{
					AnimInstance->Montage_Play(ChargingLoopAnim, GetUTOwner()->GetFireRateMultiplier());
				}
				else if (AnimInstance->GetCurrentActiveMontage() == ChargingLoopAnim)
				{
					AnimInstance->Montage_Stop(0.1f);
				}
			}
		}
	}

	virtual void PutDown() override
	{
		if (!bCharging)
		{
			Super::PutDown();
		}
	}
};