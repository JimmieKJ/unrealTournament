// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ExponentialHeightFog.generated.h"


/**
 * Implements an Actor for exponential height fog.
 */
UCLASS(showcategories=(Movement, Rendering, "Utilities|Transformation"), ClassGroup=Fog, MinimalAPI)
class AExponentialHeightFog
	: public AInfo
{
	GENERATED_UCLASS_BODY()

private_subobject:
	/** @todo document */
	DEPRECATED_FORGAME(4.6, "Component should not be accessed directly, please use GetComponent() function instead. Component will soon be private and your code will not compile.")
	UPROPERTY(Category = ExponentialHeightFog, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UExponentialHeightFogComponent* Component;

public:

	/** replicated copy of ExponentialHeightFogComponent's bEnabled property */
	UPROPERTY(replicatedUsing=OnRep_bEnabled)
	uint32 bEnabled:1;

	/** Replication Notification Callbacks */
	UFUNCTION()
	virtual void OnRep_bEnabled();


	//Begin AActor Interface
	virtual void PostInitializeComponents() override;
	//End AActor Interface

	/** Returns Component subobject **/
	ENGINE_API class UExponentialHeightFogComponent* GetComponent() const;
};
