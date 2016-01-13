// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTEyewear.h"


AUTEyewear::AUTEyewear(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void AUTEyewear::OnWearerDeath_Implementation(TSubclassOf<UDamageType> DamageType)
{
	DetachRootComponentFromParent(true);
	SetBodiesToSimulatePhysics();
}