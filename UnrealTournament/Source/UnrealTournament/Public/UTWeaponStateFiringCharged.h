// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeaponStateFiring.h"
#include "Animation/AnimInstance.h"

#include "UTWeaponStateFiringCharged.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTWeaponStateFiringCharged : public UUTWeaponStateFiring
{
	GENERATED_UCLASS_BODY()

	UUTWeaponStateFiringCharged(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	/** looping animation played while charging */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	UAnimMontage* ChargingLoopAnim;

	/** if set, increment flash count when charging starts (causes third person effects to be played) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Effects)
	bool bChargeFlashCount;

	bool bCharging;
	float ChargeTime;

	virtual void BeginState(const UUTWeaponState* PrevState) override
	{
		ToggleLoopingEffects(true);
		GetOuterAUTWeapon()->OnStartedFiring();
		if (GetUTOwner() == NULL || GetOuterAUTWeapon()->GetCurrentState() != this)
		{
			return;
		}
		GetOuterAUTWeapon()->DeactivateSpawnProtection();
		bCharging = true;
		if (bChargeFlashCount)
		{
			GetUTOwner()->IncrementFlashCount(GetOuterAUTWeapon()->GetCurrentFireMode());
		}
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

		if (GetOuterAUTWeapon()->HandleContinuedFiring())
		{
			ToggleLoopingEffects(true);
			bCharging = true;
			if (bChargeFlashCount)
			{
				GetUTOwner()->IncrementFlashCount(GetOuterAUTWeapon()->GetCurrentFireMode());
			}
		}
	}
	virtual void EndFiringSequence(uint8 FireModeNum)
	{
		if (bCharging && FireModeNum == GetFireMode())
		{
			ToggleLoopingEffects(false);
			AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
			if (!GameState  || (!GameState->HasMatchEnded() && !GameState->IsMatchIntermission()))
			{
				FireShot();
			}
			bCharging = false;
			ChargeTime = 0.0f;
			if (GetOuterAUTWeapon()->GetCurrentState() == this)
			{
				GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(RefireCheckHandle, this, &UUTWeaponStateFiring::RefireCheckTimer, GetOuterAUTWeapon()->GetRefireTime(GetOuterAUTWeapon()->GetCurrentFireMode()), false);
			}
			// weapon switch now if out of ammo
			if (!GetOuterAUTWeapon()->HasAnyAmmo() && GetUTOwner() != nullptr && GetUTOwner()->IsLocallyControlled() && GetUTOwner()->GetPendingWeapon() == nullptr)
			{
				GetOuterAUTWeapon()->AUTWeapon::OnRep_Ammo();
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
	virtual bool WillSpawnShot(float DeltaTime) override
	{
		AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetOuterAUTWeapon()->GetUTOwner()->GetController());
		return UTPC && UTPC->HasDeferredFireInputs();
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
		if (!bCharging || !GetOuterAUTWeapon()->HasAnyAmmo())
		{
			Super::PutDown();
		}
	}
};