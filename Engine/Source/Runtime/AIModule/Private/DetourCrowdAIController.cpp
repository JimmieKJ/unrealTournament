// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DetourCrowdAIController.h"
#include "Navigation/CrowdFollowingComponent.h"

ADetourCrowdAIController::ADetourCrowdAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{

}
