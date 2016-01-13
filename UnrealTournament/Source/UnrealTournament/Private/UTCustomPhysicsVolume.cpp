// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCustomPhysicsVolume.h"

AUTCustomPhysicsVolume::AUTCustomPhysicsVolume(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	CustomGravityScaling = 1.f;
}

float AUTCustomPhysicsVolume::GetGravityZ() const
{
	return CustomGravityScaling * Super::GetGravityZ();
}