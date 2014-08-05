// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProjectileMovementComponent.h"
#include "UTProj_Redeemer.h"

AUTProj_Redeemer::AUTProj_Redeemer(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	// Movement
	ProjectileMovement->InitialSpeed = 2200.f;
	ProjectileMovement->MaxSpeed = 2200.f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;

	InitialLifeSpan = 20.0f;
}