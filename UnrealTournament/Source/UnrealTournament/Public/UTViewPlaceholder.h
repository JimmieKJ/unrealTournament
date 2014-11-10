// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTViewPlaceholder.generated.h"

UCLASS(CustomConstructor)
class AUTViewPlaceholder : public AActor
{
	GENERATED_UCLASS_BODY()

	AUTViewPlaceholder(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{
		bReplicates = true;

		TSubobjectPtr<USceneComponent> SceneComponent = PCIP.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneComp"));
		RootComponent = SceneComponent;
		RootComponent->Mobility = EComponentMobility::Static;

		bHidden = true;
	}
};