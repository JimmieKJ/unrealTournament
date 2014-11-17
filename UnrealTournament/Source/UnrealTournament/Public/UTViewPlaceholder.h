// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTViewPlaceholder.generated.h"

UCLASS(CustomConstructor)
class AUTViewPlaceholder : public AActor
{
	GENERATED_UCLASS_BODY()

	AUTViewPlaceholder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		bReplicates = true;

		TSubobjectPtr<USceneComponent> SceneComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneComp"));
		RootComponent = SceneComponent;
		RootComponent->Mobility = EComponentMobility::Static;

		bHidden = true;
	}
};