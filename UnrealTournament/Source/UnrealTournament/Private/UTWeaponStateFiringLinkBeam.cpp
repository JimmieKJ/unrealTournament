// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
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

    if (LinkGun != NULL && LinkGun->LinkTarget == NULL)
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
    if (LinkGun->LinkTarget != nullptr)
    {
        LinkGun->LinkBreakTime = -1.f;
        LinkGun->LinkTarget = nullptr;
    }
    Super::EndState();
}

void UUTWeaponStateFiringLinkBeam::Tick(float DeltaTime)
{
    AUTWeap_LinkGun* LinkGun = Cast<AUTWeap_LinkGun>(GetOuterAUTWeapon());
    if (!LinkGun->FireShotOverride() && LinkGun->InstantHitInfo.IsValidIndex(LinkGun->GetCurrentFireMode()))
    {
        const FInstantHitDamageInfo& DamageInfo = LinkGun->InstantHitInfo[LinkGun->GetCurrentFireMode()]; //Get and store reference to DamageInfo, Damage = 34, Momentum = -100000, TraceRange = 1800
        FHitResult Hit;
        LinkGun->FireInstantHit(false, &Hit);

        // We are linked to something, then check to see if we need to break the link
        if (LinkGun->LinkTarget != nullptr)
        {
            // if we're linked to something, and our hit actor isn't our linked target
            //if (LinkGun->LinkTarget != &*Hit.Actor) // && Cast<AUTCharacter>(&*Hit.Actor)
            //{
            //    if (LinkGun->LinkBreakTime == -1)
            //    {
            //        LinkGun->LinkBreakTime = LinkGun->LinkBreakDelay;
            //    }
            //    LinkGun->LinkBreakTime -= DeltaTime;
            //    if (LinkGun->LinkBreakTime <= 0)
            //    {
            //        LinkGun->LinkBreakTime = -1;
            //        LinkGun->LinkTarget = NULL;
            //    }
            //}

            //Check to break Link
            if (!LinkGun->IsLinkValid())
            {
                //LinkGun->ClearLinksTo();

                LinkGun->LinkBreakTime = -1.f;
                LinkGun->LinkTarget = nullptr;
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
				else if (LinkGun->LinkTarget != NULL)
				{
					if (LinkGun->LinkBreakTime <= 0.f)
					{
						LinkGun->LinkBreakTime = -1.f;
						LinkGun->LinkTarget = NULL;
					}
					else
					{
						LinkGun->LinkBreakTime -= DeltaTime;
					}
				}
			}

            if (LinkGun->LinkTarget == NULL)
            {
                float LinkedDamage = float(DamageInfo.Damage);

                LinkedDamage += LinkedDamage * LinkGun->PerLinkDamageScalingSecondary * LinkGun->Links;
                Accumulator += LinkedDamage / LinkGun->GetRefireTime(LinkGun->GetCurrentFireMode()) * DeltaTime;

                if (Accumulator >= MinDamage)
                {
                    int32 AppliedDamage = FMath::TruncToInt(Accumulator);
                    Accumulator -= AppliedDamage;
                    FVector FireDir = (Hit.Location - Hit.TraceStart).SafeNormal();
					Hit.Actor->TakeDamage(AppliedDamage, FUTPointDamageEvent(AppliedDamage, Hit, FireDir, DamageInfo.DamageType, FireDir * (GetOuterAUTWeapon()->GetImpartedMomentumMag(Hit.Actor.Get()) * float(AppliedDamage) / float(DamageInfo.Damage))), GetOuterAUTWeapon()->GetUTOwner()->Controller, GetOuterAUTWeapon());
                }
            }
        }
        // beams show a clientside beam target
		if (LinkGun->Role < ROLE_Authority && LinkGun->GetUTOwner() != NULL) // might have lost owner due to TakeDamage() call above!
        {
            if (LinkGun->LinkTarget == NULL)
            {
                LinkGun->GetUTOwner()->SetFlashLocation(Hit.Location, LinkGun->GetCurrentFireMode());
            }
            else
            {
                LinkGun->GetUTOwner()->SetFlashLocation(LinkGun->LinkTarget->GetTargetLocation(), 10);
            }
        }
    }
}
