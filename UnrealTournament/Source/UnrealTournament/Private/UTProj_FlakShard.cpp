// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProjectileMovementComponent.h"
#include "UTProj_FlakShard.h"

AUTProj_FlakShard::AUTProj_FlakShard(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	ProjectileMovement->InitialSpeed = 6500.0f;
	ProjectileMovement->MaxSpeed = 6500.0f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = 0.0f;

	// Damage
	DamageParams.BaseDamage = 18.0f;
	DamageParams.MinimumDamage = 5.0f;
	Momentum = 28000.f;

	DamageAttenuation = 5.0f;
	DamageAttenuationDelay = 0.75f;
	MinDamageSpeed = 800.f;

	InitialLifeSpan = 2.f;
	BounceFinalLifeSpanIncrement = 0.5f;
	BouncesRemaining = 2;
}

void AUTProj_FlakShard::ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	if (GetVelocity().Size() < MinDamageSpeed)
	{
		ShutDown();
	}
	else
	{
		Super::ProcessHit_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);
	}
}

void AUTProj_FlakShard::OnBounce(const struct FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	Super::OnBounce(ImpactResult, ImpactVelocity);

	// manually handle bounce velocity to match UT3 for now
	ProjectileMovement->Velocity = 0.6f * (ImpactVelocity - 2.0f * ImpactResult.Normal * (ImpactVelocity | ImpactResult.Normal));

	// Set gravity on bounce
	ProjectileMovement->ProjectileGravityScale = 1.f;

	// Limit number of bounces
	BouncesRemaining--;
	if (BouncesRemaining == 0)
	{
		SetLifeSpan(GetLifeSpan() + BounceFinalLifeSpanIncrement);
		ProjectileMovement->bShouldBounce = false;
	}
}

FRadialDamageParams AUTProj_FlakShard::GetDamageParams_Implementation(AActor* OtherActor, const FVector& HitLocation, float& OutMomentum) const
{
	FRadialDamageParams Result = Super::GetDamageParams_Implementation(OtherActor, HitLocation, OutMomentum);
	Result.BaseDamage = FMath::Max<float>(Result.MinimumDamage, Result.BaseDamage - DamageAttenuation * FMath::Max<float>(0.0f, GetClass()->GetDefaultObject<AUTProj_FlakShard>()->InitialLifeSpan - GetLifeSpan() - DamageAttenuationDelay));
	return Result;
}

