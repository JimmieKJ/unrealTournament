// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeaponAttachment.h"

#include "UTWeapAttachment_ImpactHammer.generated.h"

UCLASS(Blueprintable, NotPlaceable, Abstract, CustomConstructor)
class UNREALTOURNAMENT_API AUTWeapAttachment_ImpactHammer : public AUTWeaponAttachment
{
	GENERATED_UCLASS_BODY()

	AUTWeapAttachment_ImpactHammer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	virtual void PlayFiringEffects() override
	{
		if (UTOwner->FireMode != 0)
		{
			Super::PlayFiringEffects();
		}
		else
		{
			// for primary, FlashCount > 0 is charging, FlashCount == 0 and FlashLocation set is release
			if (UTOwner->FlashCount > 0)
			{
				StopFiringEffects(true);
				// muzzle flash
				if (MuzzleFlash.IsValidIndex(UTOwner->FireMode) && MuzzleFlash[UTOwner->FireMode] != NULL && MuzzleFlash[UTOwner->FireMode]->Template != NULL)
				{
					// if we detect a looping particle system, then don't reactivate it
					if (!MuzzleFlash[UTOwner->FireMode]->bIsActive || MuzzleFlash[UTOwner->FireMode]->bSuppressSpawning || !IsLoopingParticleSystem(MuzzleFlash[UTOwner->FireMode]->Template))
					{
						MuzzleFlash[UTOwner->FireMode]->ActivateSystem();
					}
				}
			}
			else
			{
				StopFiringEffects(false);

				// fire effects
				static FName NAME_HitLocation(TEXT("HitLocation"));
				static FName NAME_LocalHitLocation(TEXT("LocalHitLocation"));
				const FVector SpawnLocation = (MuzzleFlash.IsValidIndex(UTOwner->FireMode) && MuzzleFlash[UTOwner->FireMode] != NULL) ? MuzzleFlash[UTOwner->FireMode]->GetComponentLocation() : UTOwner->GetActorLocation() + UTOwner->GetActorRotation().RotateVector(FVector(UTOwner->GetSimpleCollisionCylinderExtent().X, 0.0f, 0.0f));
				if (FireEffect.IsValidIndex(UTOwner->FireMode) && FireEffect[UTOwner->FireMode] != NULL)
				{
					UParticleSystemComponent* PSC = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), FireEffect[UTOwner->FireMode], SpawnLocation, (UTOwner->FlashLocation - SpawnLocation).Rotation(), true);
					PSC->SetVectorParameter(NAME_HitLocation, UTOwner->FlashLocation);
					PSC->SetVectorParameter(NAME_LocalHitLocation, PSC->ComponentToWorld.InverseTransformPosition(UTOwner->FlashLocation));
				}

				if ((UTOwner->FlashLocation - LastImpactEffectLocation).Size() >= ImpactEffectSkipDistance || GetWorld()->TimeSeconds - LastImpactEffectTime >= MaxImpactEffectSkipTime)
				{
					if (ImpactEffect.IsValidIndex(UTOwner->FireMode) && ImpactEffect[UTOwner->FireMode] != NULL)
					{
						FHitResult ImpactHit = AUTWeapon::GetImpactEffectHit(UTOwner, SpawnLocation, UTOwner->FlashLocation);
						ImpactEffect[UTOwner->FireMode].GetDefaultObject()->SpawnEffect(GetWorld(), FTransform(ImpactHit.Normal.Rotation(), ImpactHit.Location), ImpactHit.Component.Get(), NULL, UTOwner->Controller);
					}
					LastImpactEffectLocation = UTOwner->FlashLocation;
					LastImpactEffectTime = GetWorld()->TimeSeconds;
				}
			}
		}
	}
};