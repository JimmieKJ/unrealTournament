// Fill out your copyright notice in the Description page of Project Settings.

#include "UnrealTournament.h"
#include "UTWeaponStateEquipping_Enforcer.h"
#include "UTWeaponStateUnequipping_Enforcer.h"
#include "UTWeap_Enforcer.h"

void UUTWeaponStateUnequipping_Enforcer::BeginState(const UUTWeaponState* PrevState)
{
	const UUTWeaponStateEquipping* PrevEquip = Cast<UUTWeaponStateEquipping>(PrevState);

	// if was previously equipping, pay same amount of time to take back down
	UnequipTime = (PrevEquip != NULL) ? FMath::Min(PrevEquip->PartialEquipTime, GetOuterAUTWeapon()->PutDownTime) : GetOuterAUTWeapon()->PutDownTime;
	UnequipTimeElapsed = 0.0f;
	if (UnequipTime <= 0.0f)
	{
		PutDownFinished();
	}
	else
	{
		AUTWeap_Enforcer* OuterWeapon = Cast<AUTWeap_Enforcer>(GetOuterAUTWeapon());

		GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateUnequipping_Enforcer::PutDownFinished, UnequipTime);
		if (OuterWeapon->PutDownAnim != NULL)
		{
			UAnimInstance* AnimInstance = OuterWeapon->Mesh->GetAnimInstance();
			if (AnimInstance != NULL)
			{
				AnimInstance->Montage_Play(OuterWeapon->PutDownAnim, OuterWeapon->PutDownAnim->SequenceLength / UnequipTime);
			}

			AnimInstance = OuterWeapon->LeftMesh->GetAnimInstance();
			if (AnimInstance != NULL && OuterWeapon->bDualEnforcerMode)
			{
				AnimInstance->Montage_Play(OuterWeapon->PutDownAnim, OuterWeapon->PutDownAnim->SequenceLength / UnequipTime);
			}
			
		}
	}
}

void UUTWeaponStateEquipping_Enforcer::StartEquip(float OverflowTime)
{

	EquipTime -= OverflowTime;
	if (EquipTime <= 0.0f)
	{
		BringUpFinished();
	}
	else
	{
		GetOuterAUTWeapon()->GetWorldTimerManager().SetTimer(this, &UUTWeaponStateEquipping_Enforcer::BringUpFinished, EquipTime);

		AUTWeap_Enforcer* OuterWeapon = Cast<AUTWeap_Enforcer>(GetOuterAUTWeapon());

		if (OuterWeapon->BringUpAnim != NULL)
		{
			UAnimInstance* AnimInstance = OuterWeapon->Mesh->GetAnimInstance();
			if (AnimInstance != NULL)
			{
				AnimInstance->Montage_Play(OuterWeapon->BringUpAnim, OuterWeapon->BringUpAnim->SequenceLength / EquipTime);
			}
		}

		if (OuterWeapon->LeftBringUpAnim != NULL && OuterWeapon->bDualEnforcerMode)
		{
			UAnimInstance* AnimInstance = OuterWeapon->LeftMesh->GetAnimInstance();
			if (AnimInstance != NULL)
			{
				AnimInstance->Montage_Play(OuterWeapon->LeftBringUpAnim, OuterWeapon->LeftBringUpAnim->SequenceLength / EquipTime);
			}
		}
	}
}
