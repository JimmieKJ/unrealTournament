// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Components/ActorComponent.h"
#include "NavRelevantInterface.h"
#include "NavRelevantComponent.generated.h"

UCLASS()
class ENGINE_API UNavRelevantComponent : public UActorComponent, public INavRelevantInterface
{
	GENERATED_UCLASS_BODY()

	// Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	// End UActorComponent Interface

	// Begin INavRelevantInterface Interface
	virtual FBox GetNavigationBounds() const override;
	virtual bool IsNavigationRelevant() const override;
	virtual void UpdateNavigationBounds() override;
	// End INavRelevantInterface Interface

	virtual void CalcBounds();

	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	void SetNavigationRelevancy(bool bRelevant);

	/** force refresh in navigation octree */
	void RefreshNavigationModifiers();

	/** bounds for navigation octree */
	FBox Bounds;

protected:

	UPROPERTY()
	uint32 bNavigationRelevant : 1;
};
