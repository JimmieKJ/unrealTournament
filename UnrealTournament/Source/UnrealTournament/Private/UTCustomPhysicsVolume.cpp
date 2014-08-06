// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCustomPhysicsVolume.h"

AUTCustomPhysicsVolume::AUTCustomPhysicsVolume(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	CustomGravityScaling = 1.f;
}

float AUTCustomPhysicsVolume::GetGravityZ() const
{
	return CustomGravityScaling * Super::GetGravityZ();
}