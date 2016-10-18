// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Particles/ParticleSystemComponent.h"
#include "UTCharacter.h"
#include "UTGib.h"

#include "UTDmgType_AttachParticles.generated.h"

UCLASS(Abstract, HideDropDown, CustomConstructor)
class UNREALTOURNAMENT_API UUTDmgType_AttachParticles : public UUTDamageType
{
	GENERATED_UCLASS_BODY()

	UUTDmgType_AttachParticles(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		DamageThreshold = 35;
		HealthThreshold = 100000;
		EffectLifeSpan = 2.5f;
		AttachToGibChance = 0.4f;
	}

	/** the effect to spawn */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = Effects)
	UParticleSystem* HitEffect;
	/** lifespan of the effect */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = Effects)
	float EffectLifeSpan;
	/** chance to attach the effect to gibs */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = Effects)
	float AttachToGibChance;
	/** only hits of this damage or higher spawn the effect (0 to ignore) */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = Effects)
	int32 DamageThreshold;
	/** victim must be at this health or less to spawn the effect (high value to ignore) */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = Effects)
	int32 HealthThreshold;
	/** if set, any threshold must pass to spawn, otherwise all must pass */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = Effects)
	bool bSpawnOnAnyPass;

	virtual void PlayHitEffects_Implementation(AUTCharacter* HitPawn, bool bPlayedArmorEffect) const override
	{
		if (HitEffect != NULL && HitPawn->LastTakeHitInfo.Damage > 0)
		{
			bool bDamagePassed = (HitPawn->LastTakeHitInfo.Damage >= DamageThreshold);
			bool bHealthPassed = (HitPawn->Health <= HealthThreshold);
			if (bSpawnOnAnyPass ? (bDamagePassed || bHealthPassed) : (bDamagePassed && bHealthPassed))
			{
				UParticleSystemComponent* PSC = NewObject<UParticleSystemComponent>(HitPawn);
				PSC->bAutoActivate = true;
				PSC->bAutoDestroy = true;
				PSC->SetTemplate(HitEffect);
				FTimerHandle TempHandle;
				HitPawn->GetWorldTimerManager().SetTimer(TempHandle, PSC, &UParticleSystemComponent::DeactivateSystem, EffectLifeSpan, false);
				PSC->RegisterComponent();
				PSC->AttachToComponent(HitPawn->GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, HitPawn->GetMesh()->FindClosestBone(HitPawn->GetActorLocation() + HitPawn->LastTakeHitInfo.RelHitLocation));
			}
		}
		Super::PlayHitEffects_Implementation(HitPawn, bPlayedArmorEffect);
	}

	virtual void PlayGibEffects_Implementation(AUTGib* Gib) const override
	{
		if (HitEffect != NULL && FMath::FRand() < AttachToGibChance)
		{
			UParticleSystemComponent* PSC = NewObject<UParticleSystemComponent>(Gib);
			PSC->bAutoActivate = true;
			PSC->bAutoDestroy = true;
			PSC->SetTemplate(HitEffect);
			FTimerHandle TempHandle;
			Gib->GetWorldTimerManager().SetTimer(TempHandle, PSC, &UParticleSystemComponent::DeactivateSystem, EffectLifeSpan, false);
			PSC->RegisterComponent();
			PSC->AttachToComponent(Gib->GetRootComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);
		}
	}
};