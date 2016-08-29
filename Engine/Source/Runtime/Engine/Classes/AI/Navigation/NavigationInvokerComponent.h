// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AI/Navigation/NavigationSystem.h"
#include "NavigationInvokerComponent.generated.h"

UCLASS(ClassGroup = (Navigation), meta = (BlueprintSpawnableComponent), hidecategories = (Activation))
class ENGINE_API UNavigationInvokerComponent : public UActorComponent
{
	GENERATED_BODY()

protected:

	UPROPERTY(EditAnywhere, Category = Navigation, meta = (ClampMin = "0.1", UIMin = "0.1"))
	float TileGenerationRadius;

	UPROPERTY(EditAnywhere, Category = Navigation, meta = (ClampMin = "0.1", UIMin = "0.1"))
	float TileRemovalRadius;

public:
	UNavigationInvokerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void RegisterWithNavigationSystem(UNavigationSystem& NavSys);

protected:
	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;
};