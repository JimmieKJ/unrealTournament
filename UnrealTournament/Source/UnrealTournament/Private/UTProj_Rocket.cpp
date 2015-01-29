// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProj_Rocket.h"

AUTProj_Rocket::AUTProj_Rocket(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DamageParams.BaseDamage = 100;
	DamageParams.OuterRadius = 430.f;
	Momentum = 150000.0f;
	InitialLifeSpan = 10.f;
	ProjectileMovement->InitialSpeed = 2900.f;
	ProjectileMovement->MaxSpeed = 2900.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
}