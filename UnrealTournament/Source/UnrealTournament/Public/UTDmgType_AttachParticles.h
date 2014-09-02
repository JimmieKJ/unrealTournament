// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Particles/ParticleSystemComponent.h"
#include "UTCharacter.h"

#include "UTDmgType_AttachParticles.generated.h"

UCLASS(Abstract, HideDropDown, CustomConstructor)
class UUTDmgType_AttachParticles : public UUTDamageType
{
	GENERATED_UCLASS_BODY()

	UUTDmgType_AttachParticles(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{
		DamageThreshold = 35;
		HealthThreshold = 100000;
		EffectLifeSpan = 3.0f;
	}

	/** the effect to spawn */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = Effects)
	UParticleSystem* HitEffect;
	/** lifespan of the effect */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = Effects)
	float EffectLifeSpan;
	/** only hits of this damage or higher spawn the effect (0 to ignore) */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = Effects)
	int32 DamageThreshold;
	/** victim must be at this health or less to spawn the effect (high value to ignore) */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = Effects)
	int32 HealthThreshold;
	/** if set, any threshold must pass to spawn, otherwise all must pass */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = Effects)
	bool bSpawnOnAnyPass;

	virtual void PlayHitEffects_Implementation(AUTCharacter* HitPawn) const override
	{
		if (HitEffect != NULL)
		{
			bool bDamagePassed = (HitPawn->LastTakeHitInfo.Damage >= DamageThreshold);
			bool bHealthPassed = (HitPawn->Health <= HealthThreshold);
			if (bSpawnOnAnyPass ? (bDamagePassed || bHealthPassed) : (bDamagePassed && bHealthPassed))
			{
				UParticleSystemComponent* PSC = NewObject<UParticleSystemComponent>(HitPawn);
				PSC->bAutoActivate = true;
				PSC->bAutoDestroy = true;
				PSC->SetTemplate(HitEffect);
				HitPawn->GetWorldTimerManager().SetTimer(PSC, &UParticleSystemComponent::DeactivateSystem, EffectLifeSpan, false);
				PSC->RegisterComponent();
				PSC->AttachTo(HitPawn->Mesh, HitPawn->Mesh->FindClosestBone(HitPawn->GetActorLocation() + HitPawn->LastTakeHitInfo.RelHitLocation), EAttachLocation::SnapToTarget);
			}
		}
	}
};