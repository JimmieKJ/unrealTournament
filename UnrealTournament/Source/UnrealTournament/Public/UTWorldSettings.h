// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWorldSettings.generated.h"

USTRUCT()
struct FTimedImpactEffect
{
	GENERATED_USTRUCT_BODY()

	/** the component */
	UPROPERTY()
	USceneComponent* EffectComp;
	/** time component was added */
	UPROPERTY()
	float CreationTime;

	FTimedImpactEffect()
	{}
	FTimedImpactEffect(USceneComponent* InComp, float InTime)
		: EffectComp(InComp), CreationTime(InTime)
	{}
};

UCLASS()
class AUTWorldSettings : public AWorldSettings
{
	GENERATED_UCLASS_BODY()

	/** maximum lifetime while onscreen of impact effects that don't end on their own such as decals
	 * set to zero for infinity
	 */
	UPROPERTY(globalconfig)
	float MaxImpactEffectVisibleLifetime;
	/** maximum lifetime while offscreen of impact effects that don't end on their own such as decals
	* set to zero for infinity
	*/
	UPROPERTY(globalconfig)
	float MaxImpactEffectInvisibleLifetime;

	/** array of impact effects that have a configurable maximum lifespan (like decals) */
	UPROPERTY()
	TArray<FTimedImpactEffect> TimedEffects;

	virtual void BeginPlay() override;
	
	/** add an impact effect that will be managed by the timing system */
	virtual void AddImpactEffect(class USceneComponent* NewEffect);

	/** checks lifetime on all active effects and culls as necessary */
	virtual void ExpireImpactEffects();
};