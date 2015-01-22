// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AI/Navigation/NavigationGraph.h"
#include "NavigationGraphNodeComponent.generated.h"

UCLASS(config=Engine, MinimalAPI, HideCategories=(Mobility))
class UNavigationGraphNodeComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FNavGraphNode Node;

	UPROPERTY()
	UNavigationGraphNodeComponent* NextNodeComponent;

	UPROPERTY()
	UNavigationGraphNodeComponent* PrevNodeComponent;

	// Begin UObject interface.
	virtual void BeginDestroy() override;
};
