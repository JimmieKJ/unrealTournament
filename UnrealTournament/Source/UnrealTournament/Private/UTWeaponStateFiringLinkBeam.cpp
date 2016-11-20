// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_LinkGun.h"
#include "UTWeaponStateFiringLinkBeam.h"
#include "Animation/AnimInstance.h"

UUTWeaponStateFiringLinkBeam::UUTWeaponStateFiringLinkBeam(const FObjectInitializer& OI)
: Super(OI)
{
	AccumulatedFiringTime = 0.f;
}

void UUTWeaponStateFiringLinkBeam::BeginState(const UUTWeaponState* PrevState)
{
	AUTWeap_LinkGun* LinkGun = Cast<AUTWeap_LinkGun>(GetOuterAUTWeapon());
	LinkGun->CurrentLinkedTarget = nullptr;
	LinkGun->LinkStartTime = -100.f;
	Super::BeginState(PrevState);
}

void UUTWeaponStateFiringLinkBeam::FireShot()
{
    // [possibly] consume ammo but don't fire from here
    AUTWeap_LinkGun* LinkGun = Cast<AUTWeap_LinkGun>(GetOuterAUTWeapon());

	if (LinkGun != NULL)
    {
		LinkGun->PlayFiringEffects();
		LinkGun->ConsumeAmmo(LinkGun->GetCurrentFireMode());

		//Special case for hidden weapons since we really need the MuzzleFlash to play for the link beam
		if (LinkGun->ShouldPlay1PVisuals() && LinkGun->GetWeaponHand() == EWeaponHand::HAND_Hidden)
		{
			if (LinkGun->MuzzleFlash.IsValidIndex(LinkGun->GetCurrentFireMode()) && LinkGun->MuzzleFlash[LinkGun->GetCurrentFireMode()] != NULL && LinkGun->MuzzleFlash[LinkGun->GetCurrentFireMode()]->Template != NULL)
			{
				// if we detect a looping particle system, then don't reactivate it
				if (!LinkGun->MuzzleFlash[LinkGun->GetCurrentFireMode()]->bIsActive || LinkGun->MuzzleFlash[LinkGun->GetCurrentFireMode()]->bSuppressSpawning || !IsLoopingParticleSystem(LinkGun->MuzzleFlash[LinkGun->GetCurrentFireMode()]->Template))
				{
					LinkGun->MuzzleFlash[LinkGun->GetCurrentFireMode()]->ActivateSystem();
				}
			}
		}
    }

	if (FiringLoopAnim != NULL)
	{
		UAnimInstance* AnimInstance = GetOuterAUTWeapon()->GetMesh()->GetAnimInstance();
		if (AnimInstance != NULL && !AnimInstance->Montage_IsPlaying(FiringLoopAnim))
		{
			GetOuterAUTWeapon()->PlayWeaponAnim(FiringLoopAnim, FiringLoopAnimHands, 1.0f);
		}
	}
    
	if (GetUTOwner() != NULL)
    {
		GetUTOwner()->TargetEyeOffset.Y = LinkGun->FiringBeamKickbackY;
		GetUTOwner()->InventoryEvent(InventoryEventName::FiredWeapon);
    }
}

void UUTWeaponStateFiringLinkBeam::EndFiringSequence(uint8 FireModeNum)
{
	if (FireModeNum == GetFireMode())
	{
		AUTWeap_LinkGun* LinkGun = Cast<AUTWeap_LinkGun>(GetOuterAUTWeapon());
		if (!LinkGun || (!LinkGun->bReadyToPull && !LinkGun->IsLinkPulsing()))
		{
			Super::EndFiringSequence(FireModeNum);
		if (FireModeNum == GetOuterAUTWeapon()->GetCurrentFireMode())
			{
				GetOuterAUTWeapon()->GotoActiveState();
			}
		}
		else
		{
			bPendingEndFire = true;
		}
	}
}


void UUTWeaponStateFiringLinkBeam::RefireCheckTimer()
{
	AUTWeap_LinkGun* LinkGun = Cast<AUTWeap_LinkGun>(GetOuterAUTWeapon());
	if (!LinkGun || !LinkGun->IsLinkPulsing())
	{
		Super::RefireCheckTimer();
	}
}

void UUTWeaponStateFiringLinkBeam::EndState()
{
	bPendingEndFire = false;
    Super::EndState();
}

void UUTWeaponStateFiringLinkBeam::Tick(float DeltaTime)
{
	AUTWeap_LinkGun* LinkGun = Cast<AUTWeap_LinkGun>(GetOuterAUTWeapon());
	if (LinkGun && (LinkGun->Role == ROLE_Authority))
	{
		LinkGun->bLinkCausingDamage = false;
	}
	if (bPendingEndFire)
	{
		if (LinkGun && LinkGun->IsLinkPulsing())
		{
			LinkGun->GetUTOwner()->SetFlashLocation(LinkGun->PulseLoc, LinkGun->GetCurrentFireMode());
			return;
		}
		else if (LinkGun && LinkGun->bReadyToPull && LinkGun->CurrentLinkedTarget)
		{
			LinkGun->StartLinkPull();
			return;
		}
		EndFiringSequence(1);
		return;
	}
	HandleDelayedShot();
	
    if (LinkGun && !LinkGun->FireShotOverride() && LinkGun->InstantHitInfo.IsValidIndex(LinkGun->GetCurrentFireMode()))
    {
		const FInstantHitDamageInfo& DamageInfo = LinkGun->InstantHitInfo[LinkGun->GetCurrentFireMode()]; //Get and store reference to DamageInfo, Damage = 34, Momentum = -100000, TraceRange = 1800
		FHitResult Hit; 
		FName RealShotsStatsName = LinkGun->ShotsStatsName;
		LinkGun->ShotsStatsName = NAME_None;
		FName RealHitsStatsName = LinkGun->HitsStatsName;
		LinkGun->HitsStatsName = NAME_None;
		LinkGun->FireInstantHit(false, &Hit);
		LinkGun->ShotsStatsName = RealShotsStatsName;
		LinkGun->HitsStatsName = RealHitsStatsName;

		AccumulatedFiringTime += DeltaTime;
		float RefireTime = LinkGun->GetRefireTime(LinkGun->GetCurrentFireMode());
		AUTPlayerState* PS = (LinkGun->Role == ROLE_Authority) && LinkGun->GetUTOwner() && LinkGun->GetUTOwner()->Controller ? Cast<AUTPlayerState>(LinkGun->GetUTOwner()->Controller->PlayerState) : NULL;
		LinkGun->bLinkBeamImpacting = (Hit.Time < 1.f);
		AActor* OldLinkedTarget = LinkGun->CurrentLinkedTarget;
		LinkGun->CurrentLinkedTarget = nullptr;
		if (Hit.Actor != NULL && Hit.Actor->bCanBeDamaged && Hit.Actor != LinkGun->GetUTOwner())
        {   
			LinkGun->CurrentLinkedTarget = Hit.Actor.Get();
			if (LinkGun->Role == ROLE_Authority)
			{
				LinkGun->bLinkCausingDamage = true;
			}

            float LinkedDamage = float(DamageInfo.Damage);
 			Accumulator += LinkedDamage / RefireTime * DeltaTime;
			if (PS && (LinkGun->ShotsStatsName != NAME_None) && (AccumulatedFiringTime > RefireTime))
			{
				AccumulatedFiringTime -= RefireTime;
				PS->ModifyStatsValue(LinkGun->ShotsStatsName, 1);
			}

			if (Accumulator >= MinDamage)
            {
                int32 AppliedDamage = FMath::TruncToInt(Accumulator);
                Accumulator -= AppliedDamage;
                FVector FireDir = (Hit.Location - Hit.TraceStart).GetSafeNormal();
				AController* LinkDamageInstigator = LinkGun->GetUTOwner() ? LinkGun->GetUTOwner()->Controller : nullptr;
				Hit.Actor->TakeDamage(AppliedDamage, FUTPointDamageEvent(AppliedDamage, Hit, FireDir, DamageInfo.DamageType, FireDir * (LinkGun->GetImpartedMomentumMag(Hit.Actor.Get()) * float(AppliedDamage) / float(DamageInfo.Damage))), LinkDamageInstigator, LinkGun);
				if (PS && (LinkGun->HitsStatsName != NAME_None))
				{
					PS->ModifyStatsValue(LinkGun->HitsStatsName, AppliedDamage/FMath::Max(LinkedDamage, 1.f));
				}
			}
        }
		else
		{
			if (PS && (LinkGun->ShotsStatsName != NAME_None) && (AccumulatedFiringTime > RefireTime))
			{
				AccumulatedFiringTime -= RefireTime;
				PS->ModifyStatsValue(LinkGun->ShotsStatsName, 1);
			}
		}
		if (OldLinkedTarget != LinkGun->CurrentLinkedTarget)
		{
			LinkGun->LinkStartTime = GetWorld()->GetTimeSeconds();
			LinkGun->bReadyToPull = false;
		}
		else if (LinkGun->CurrentLinkedTarget && !LinkGun->IsLinkPulsing())
		{
			LinkGun->bReadyToPull = (GetWorld()->GetTimeSeconds() - LinkGun->LinkStartTime > LinkGun->PullWarmupTime);
		}
        // beams show a clientside beam target
		if (LinkGun->Role < ROLE_Authority && LinkGun->GetUTOwner() != NULL) // might have lost owner due to TakeDamage() call above!
        {
			LinkGun->GetUTOwner()->SetFlashLocation(Hit.Location, LinkGun->GetCurrentFireMode());
        }
    }
}

