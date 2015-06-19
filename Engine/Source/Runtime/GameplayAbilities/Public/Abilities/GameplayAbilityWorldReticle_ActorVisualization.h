// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GameplayAbilityWorldReticle.h"

#include "GameplayAbilityWorldReticle_ActorVisualization.generated.h"

class AGameplayAbilityTargetActor;

/** This is a dummy reticle for internal use by visualization placement tasks. It builds a custom visual model of the visualization being placed. */
UCLASS(notplaceable)
class AGameplayAbilityWorldReticle_ActorVisualization : public AGameplayAbilityWorldReticle
{
	GENERATED_UCLASS_BODY()

public:
	void InitializeReticleVisualizationInformation(AGameplayAbilityTargetActor* InTargetingActor, AActor* VisualizationActor, UMaterialInterface *VisualizationMaterial);

private:
	/** Hardcoded collision component, so other objects don't think they can collide with the visualization actor */
	DEPRECATED_FORGAME(4.6, "CollisionComponent should not be accessed directly, please use GetCollisionComponent() function instead. CollisionComponent will soon be private and your code will not compile.")
	UPROPERTY()
	class UCapsuleComponent* CollisionComponent;
public:

	UPROPERTY()
	TArray<UActorComponent*> VisualizationComponents;

	/** Overridable function called whenever this actor is being removed from a level */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Returns CollisionComponent subobject **/
	GAMEPLAYABILITIES_API class UCapsuleComponent* GetCollisionComponent();
};
