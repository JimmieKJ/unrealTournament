// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWaterVolume.h"
#include "UTPainVolume.h"

AUTWaterVolume::AUTWaterVolume(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	bWaterVolume = true;
	FluidFriction = 1.1f;
}

AUTPainVolume::AUTPainVolume(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	bWaterVolume = true;
	FluidFriction = 1.5f;
}
