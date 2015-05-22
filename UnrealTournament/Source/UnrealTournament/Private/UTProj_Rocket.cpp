// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProj_Rocket.h"
#include "UnrealNetwork.h"
#include "UTRewardMessage.h"
#include "StatNames.h"

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
	AdjustmentSpeed = 5000.0f;
	bLeadTarget = true;
}

void AUTProj_Rocket::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (TargetActor != NULL)
	{
		FVector WantedDir = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		if (bLeadTarget)
		{
			WantedDir += TargetActor->GetVelocity() * WantedDir.Size() / ProjectileMovement->MaxSpeed;
		}

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
	DOREPLIFETIME(AUTProj_Rocket, TargetActor);
}

void AUTProj_Rocket::DamageImpactedActor_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	AUTCharacter* HitCharacter = Cast<AUTCharacter>(OtherActor);
	bool bPossibleAirRocket = (HitCharacter && AirRocketRewardClass && (HitCharacter->Health > 0) && HitCharacter->GetCharacterMovement() != NULL && (HitCharacter->GetCharacterMovement()->MovementMode == MOVE_Falling) && (GetWorld()->GetTimeSeconds() - HitCharacter->FallingStartTime > 0.2f));

	Super::DamageImpactedActor_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);
	if (bPossibleAirRocket && HitCharacter && (HitCharacter->Health <= 0))
	{
		// Air Rocket reward
		AUTPlayerController* PC = Cast<AUTPlayerController>(InstigatorController);
		if (PC != NULL)
		{
			PC->SendPersonalMessage(AirRocketRewardClass);
			AUTPlayerState* PS = Cast<AUTPlayerState>(PC->PlayerState);
			if (PS)
			{
				PS->ModifyStatsValue(NAME_AirRox, 1);
			}
		}
	}
}

void AUTProj_Rocket::Explode_Implementation(const FVector& HitLocation, const FVector& HitNormal, UPrimitiveComponent* HitComp)
{
	AUTCharacter* HitCharacter = Cast<AUTCharacter>(ImpactedActor);
	bool bFollowersTrack = (!bExploded && (Role == ROLE_Authority) && (FollowerRockets.Num() > 0) && HitCharacter);

	Super::Explode_Implementation(HitLocation, HitNormal, HitComp);
	if (bFollowersTrack && HitCharacter && (HitCharacter->Health > 0))
	{
		for (int32 i = 0; i < FollowerRockets.Num(); i++)
		{
			if (FollowerRockets[i] && !FollowerRockets[i]->IsPendingKillPending())
			{
				FollowerRockets[i]->TargetActor = HitCharacter;
				AdjustmentSpeed = 24000.f;
				bLeadTarget = true;
			}
		}
	}
}
