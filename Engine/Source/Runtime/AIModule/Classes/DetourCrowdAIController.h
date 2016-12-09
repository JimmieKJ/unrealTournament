// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "AIController.h"
#include "DetourCrowdAIController.generated.h"

UCLASS()
class ADetourCrowdAIController : public AAIController
{
	GENERATED_BODY()
public:
	ADetourCrowdAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
