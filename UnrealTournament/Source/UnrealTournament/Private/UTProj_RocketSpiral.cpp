// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProj_RocketSpiral.h"
#include "UTProjectileMovementComponent.h"
#include "UnrealNetwork.h"

AUTProj_RocketSpiral::AUTProj_RocketSpiral(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FlockRadius = 24.0f;
	FlockStiffness = -40.0f;
	FlockMaxForce = 1200.0f;
	FlockCurlForce = 900.0f;
}

void AUTProj_RocketSpiral::BeginPlay()
{
	Super::BeginPlay();

	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTProj_RocketSpiral::UpdateSpiral, 0.05f, true);
}

void AUTProj_RocketSpiral::UpdateSpiral()
{
	UUTProjectileMovementComponent* UTProjMovement = Cast<UUTProjectileMovementComponent>(ProjectileMovement);
	if (UTProjMovement != NULL)
	{
		// initialize Dir, if necessary
		if (InitialDir.IsZero())
		{
			InitialDir = UTProjMovement->Velocity.GetSafeNormal();
		}

		UTProjMovement->Velocity = UTProjMovement->InitialSpeed * (InitialDir * 0.5 * UTProjMovement->InitialSpeed + UTProjMovement->Velocity).GetSafeNormal();

		// Work out force between flock to add madness
		for (int32 i = ARRAY_COUNT(Flock) - 1; i >= 0; i--)
		{
			if (Flock[i] != NULL)
			{
				// Attract if distance between rockets is over 2*FlockRadius, repulse if below.
				FVector ForceDir = Flock[i]->GetActorLocation() - GetActorLocation();
				float ForceMag = FlockStiffness * ((2.0f * FlockRadius) - ForceDir.Size());
				UTProjMovement->Acceleration = ForceDir.GetSafeNormal() * FMath::Min(ForceMag, FlockMaxForce);

				// Vector 'curl'
				FVector CurlDir = FVector::CrossProduct(Flock[i]->ProjectileMovement->Velocity, ForceDir);
				if (bCurl == Flock[i]->bCurl)
				{
					UTProjMovement->Acceleration += CurlDir.GetSafeNormal() * FlockCurlForce;
				}
				else
				{
					UTProjMovement->Acceleration -= CurlDir.GetSafeNormal() * FlockCurlForce;
				}
				break;
			}
		}
	}
}

void AUTProj_RocketSpiral::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTProj_RocketSpiral, Flock, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTProj_RocketSpiral, bCurl, COND_InitialOnly);
}