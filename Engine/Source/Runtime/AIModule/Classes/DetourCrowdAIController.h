// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AIController.h"
#include "DetourCrowdAIController.generated.h"

UCLASS()
class ADetourCrowdAIController : public AAIController
{
	GENERATED_BODY()
public:
	ADetourCrowdAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};