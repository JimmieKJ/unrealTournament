// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "Engine.h"
#include "UTMutator.h"
#include "UTWeapon.h"

#include "SampleMutator.generated.h"

UCLASS(Blueprintable, Meta = (ChildCanTick))
class ASampleMutator : public AUTMutator
{
	GENERATED_UCLASS_BODY()

	bool CheckRelevance_Implementation(AActor* Other) override;

	UPROPERTY()
	TSubclassOf<AUTWeapon> RocketLauncherClass;
};