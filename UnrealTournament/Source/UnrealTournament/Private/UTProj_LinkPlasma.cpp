// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProj_LinkPlasma.h"
#include "UTProjectileMovementComponent.h"
#include "UnrealNetwork.h"

AUTProj_LinkPlasma::AUTProj_LinkPlasma(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bNetTemporary = true;
	bLowPriorityLight = true;
	OverlapSphereGrowthRate = 40.0f;
	MaxOverlapSphereSize = 10.0f;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AUTProj_LinkPlasma::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (PawnOverlapSphere != nullptr && PawnOverlapSphere->GetUnscaledSphereRadius() < MaxOverlapSphereSize)
	{
		PawnOverlapSphere->SetSphereRadius(FMath::Min<float>(MaxOverlapSphereSize, PawnOverlapSphere->GetUnscaledSphereRadius() + OverlapSphereGrowthRate * DeltaTime));
	}
}

