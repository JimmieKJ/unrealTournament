// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProj_Sniper.generated.h"

UCLASS(CustomConstructor, Abstract)
class AUTProj_Sniper : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

	AUTProj_Sniper(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{
		HeadshotDamageMult = 2.0f;
		HeadScaling = 1.0f;
	}

	/** scaling for target head area */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	float HeadScaling;
	/** damage multiplier for headshot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	float HeadshotDamageMult;
	/** damage type for headshot (if NULL use standard damage type) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Damage)
	TSubclassOf<UDamageType> HeadshotDamageType;

	virtual void DamageImpactedActor_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal) OVERRIDE
	{
		if (HeadshotDamageMult <= 1.0f)
		{
			Super::DamageImpactedActor_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);
		}
		else
		{
			float SavedDamage = DamageParams.BaseDamage;
			TSubclassOf<UDamageType> SavedDamageType = MyDamageType;

			AUTCharacter* UTC = Cast<AUTCharacter>(OtherActor);
			if (UTC != NULL && UTC->IsHeadShot(HitLocation, GetVelocity().SafeNormal(), HeadScaling, true))
			{
				DamageParams.BaseDamage *= HeadshotDamageMult;
				MyDamageType = (HeadshotDamageType != NULL) ? HeadshotDamageType : MyDamageType;
			}
			
			Super::DamageImpactedActor_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);

			DamageParams.BaseDamage = SavedDamage;
			MyDamageType = SavedDamageType;
		}
	}
};