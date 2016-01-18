// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTViewPlaceholder.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API AUTViewPlaceholder : public AActor
{
	GENERATED_UCLASS_BODY()

	AUTViewPlaceholder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		bReplicates = true;

		USceneComponent* SceneComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("SceneComp"));
		RootComponent = SceneComponent;
		RootComponent->Mobility = EComponentMobility::Static;

		bHidden = true;
	}
};