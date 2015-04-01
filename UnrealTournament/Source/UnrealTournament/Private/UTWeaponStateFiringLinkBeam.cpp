// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_LinkGun.h"
#include "UTWeaponStateFiringLinkBeam.h"

UUTWeaponStateFiringLinkBeam::UUTWeaponStateFiringLinkBeam(const FObjectInitializer& OI)
: Super(OI)
{
}

void UUTWeaponStateFiringLinkBeam::FireShot()
{
    // [possibly] consume ammo but don't fire from here
    AUTWeap_LinkGun* LinkGun = Cast<AUTWeap_LinkGun>(GetOuterAUTWeapon());

    LinkGun->PlayFiringEffects();

	if (LinkGun != NULL && LinkGun->GetLinkTarget() == NULL)
    {
        LinkGun->ConsumeAmmo(LinkGun->GetCurrentFireMode());
    }
    
	if (GetUTOwner() != NULL)
    {
        static FName NAME_FiredWeapon(TEXT("FiredWeapon"));
        GetUTOwner()->InventoryEvent(NAME_FiredWeapon);
    }
}

void UUTWeaponStateFiringLinkBeam::EndState()
{
    AUTWeap_LinkGun* LinkGun = Cast<AUTWeap_LinkGun>(GetOuterAUTWeapon());
	if (LinkGun->GetLinkTarget() != nullptr)
    {
        LinkGun->LinkBreakTime = -1.f;
		LinkGun->ClearLinksTo();
    }
    Super::EndState();
}

void UUTWeaponStateFiringLinkBeam::Tick(float DeltaTime)
{
	HandleDelayedShot();
	
	AUTWeap_LinkGun* LinkGun = Cast<AUTWeap_LinkGun>(GetOuterAUTWeapon());
    if (!LinkGun->FireShotOverride() && LinkGun->InstantHitInfo.IsValidIndex(LinkGun->GetCurrentFireMode()))
    {
        const FInstantHitDamageInfo& DamageInfo = LinkGun->InstantHitInfo[LinkGun->GetCurrentFireMode()]; //Get and store reference to DamageInfo, Damage = 34, Momentum = -100000, TraceRange = 1800
        FHitResult Hit;
        LinkGun->FireInstantHit(false, &Hit);

        // We are linked to something, then check to see if we need to break the link
		if (LinkGun->GetLinkTarget() != nullptr)
        {
            //Check to break Link
            if (!LinkGun->IsLinkValid())
            {
                LinkGun->LinkBreakTime = -1.f;
				LinkGun->ClearLinksTo();
            }
        }

		if (Hit.Actor != NULL && Hit.Actor->bCanBeDamaged && Hit.Actor != LinkGun->GetUTOwner())
        {   
            // Check to see if our HitActor is linkable, if so, link to it
			if (LinkGun->Role == ROLE_Authority)
			{
				if (LinkGun->IsLinkable(Hit.Actor.Get()))
				{
					LinkGun->SetLinkTo(Hit.Actor.Get());
				}
				else if (LinkGun->GetLinkTarget() != NULL)
				{
					if (LinkGun->LinkBreakTime <= 0.f)
					{
						LinkGun->LinkBreakTime = -1.f;
						LinkGun->ClearLinksTo();
					}
					else
					{
						LinkGun->LinkBreakTime -= DeltaTime;
					}
				}
			}

			if (LinkGun->GetLinkTarget() == NULL)
            {
                float LinkedDamage = float(DamageInfo.Damage);

                LinkedDamage += LinkedDamage * LinkGun->PerLinkDamageScalingSecondary * LinkGun->GetNumLinks();
                Accumulator += LinkedDamage / LinkGun->GetRefireTime(LinkGun->GetCurrentFireMode()) * DeltaTime;

                if (Accumulator >= MinDamage)
                {
                    int32 AppliedDamage = FMath::TruncToInt(Accumulator);
                    Accumulator -= AppliedDamage;
                    FVector FireDir = (Hit.Location - Hit.TraceStart).GetSafeNormal();
					Hit.Actor->TakeDamage(AppliedDamage, FUTPointDamageEvent(AppliedDamage, Hit, FireDir, DamageInfo.DamageType, FireDir * (GetOuterAUTWeapon()->GetImpartedMomentumMag(Hit.Actor.Get()) * float(AppliedDamage) / float(DamageInfo.Damage))), GetOuterAUTWeapon()->GetUTOwner()->Controller, GetOuterAUTWeapon());
                }
            }
        }
        // beams show a clientside beam target
		if (LinkGun->Role < ROLE_Authority && LinkGun->GetUTOwner() != NULL) // might have lost owner due to TakeDamage() call above!
        {
			if (LinkGun->GetLinkTarget() == NULL)
            {
                LinkGun->GetUTOwner()->SetFlashLocation(Hit.Location, LinkGun->GetCurrentFireMode());
            }
            else
            {
				LinkGun->GetUTOwner()->SetFlashLocation(LinkGun->GetLinkTarget()->GetTargetLocation(), 10);
            }
        }
    }
}
