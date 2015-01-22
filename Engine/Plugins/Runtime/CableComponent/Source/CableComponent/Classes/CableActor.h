// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CableActor.generated.h"

/** An actor that renders a simulated cable */
UCLASS(hidecategories=(Input,Collision,Replication), showcategories=("Input|MouseInput", "Input|TouchInput"))
class ACableActor : public AActor
{
	GENERATED_UCLASS_BODY()

	/** Cable component that performs simulation and rendering */
	UPROPERTY(Category=Cable, VisibleAnywhere, BlueprintReadOnly)
	class UCableComponent* CableComponent;
};
