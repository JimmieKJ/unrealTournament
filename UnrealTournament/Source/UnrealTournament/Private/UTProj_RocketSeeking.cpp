// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProj_RocketSeeking.h"
#include "UnrealNetwork.h"

AUTProj_RocketSeeking::AUTProj_RocketSeeking(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	AdjustmentSpeed = 10000.0f;
}

void AUTProj_RocketSeeking::Tick(float DeltaTime)
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

void AUTProj_RocketSeeking::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTProj_RocketSeeking, TargetActor, COND_InitialOnly);
}


