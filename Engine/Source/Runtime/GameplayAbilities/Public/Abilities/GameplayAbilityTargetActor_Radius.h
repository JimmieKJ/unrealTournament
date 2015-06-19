// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayAbilityTargetActor.h"
#include "GameplayAbilityWorldReticle.h"
#include "GameplayAbilityTargetDataFilter.h"
#include "GameplayAbilityTargetActor_Radius.generated.h"

/** Selects everything within a given radius of the source actor. */
UCLASS(Blueprintable, notplaceable)
class GAMEPLAYABILITIES_API AGameplayAbilityTargetActor_Radius : public AGameplayAbilityTargetActor
{
	GENERATED_UCLASS_BODY()

public:

	virtual void StartTargeting(UGameplayAbility* Ability) override;
	
	virtual void ConfirmTargetingAndContinue() override;

	/** Radius of target acquisition around the ability's start location. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Radius)
	float Radius;

protected:

	TArray<TWeakObjectPtr<AActor> >	PerformOverlap(const FVector& Origin);

	FGameplayAbilityTargetDataHandle MakeTargetData(const TArray<TWeakObjectPtr<AActor> > Actors, const FVector& Origin) const;

};
