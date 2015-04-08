// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProj_Rocket.h"
#include "UnrealNetwork.h"

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

	PrimaryActorTick.bCanEverTick = true;
	AdjustmentSpeed = 10000.0f;
}

void AUTProj_Rocket::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (TargetActor != NULL)
	{
		FVector WantedDir = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();

		ProjectileMovement->Velocity += WantedDir * AdjustmentSpeed * DeltaTime;
		ProjectileMovement->Velocity = ProjectileMovement->Velocity.GetSafeNormal() * ProjectileMovement->MaxSpeed;

		//If the rocket has passed the target stop following
		if (FVector::DotProduct(WantedDir, ProjectileMovement->Velocity) < 0.0f)
		{
			TargetActor = NULL;
		}
	}
}

void AUTProj_Rocket::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTProj_Rocket, TargetActor, COND_InitialOnly);
}