// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Abilities/GameplayAbilityTargetDataFilter.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FGameplayTargetDataFilter
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

void FGameplayTargetDataFilter::InitializeFilterContext(AActor* FilterActor)
{
	SelfActor = FilterActor;
}
