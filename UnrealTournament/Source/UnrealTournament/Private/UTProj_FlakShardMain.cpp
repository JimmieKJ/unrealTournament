// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProjectileMovementComponent.h"
#include "UTProj_FlakShardMain.h"

AUTProj_FlakShardMain::AUTProj_FlakShardMain(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CenteredMomentumBonus = 180000.f;
	CenteredDamageBonus = 100.0f;
	MaxBonusTime = 0.2f;
	NumSatelliteShards = 3;
}

void AUTProj_FlakShardMain::DamageImpactedActor_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	Super::DamageImpactedActor_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);

	// check for camera shake
	if (Role == ROLE_Authority && ShortRangeKillShake != NULL && Instigator != NULL && GetLifeSpan() - InitialLifeSpan + MaxBonusTime > 0.0f)
	{
		AUTCharacter* UTC = Cast<AUTCharacter>(OtherActor);
		if (UTC != NULL && UTC != Instigator && UTC->IsDead())
		{
			APlayerController* PC = Cast<APlayerController>(Instigator->Controller);
			if (PC != NULL)
			{
				PC->ClientPlayCameraShake(ShortRangeKillShake);
			}
		}
	}
}

void AUTProj_FlakShardMain::OnBounce(const struct FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	Super::OnBounce(ImpactResult, ImpactVelocity);

	// no damage/momentum bonus after bounce
	MaxBonusTime = 0.f;
}

/**
* Increase damage to UTPawns based on how centered this shard is on target.  If it is within the time MaxBonusTime time period.
* e.g. point blank shot with the flak cannon you will do mega damage.  Once MaxBonusTime passes then this shard becomes a normal shard.
*/
FRadialDamageParams AUTProj_FlakShardMain::GetDamageParams_Implementation(AActor* OtherActor, const FVector& HitLocation, float& OutMomentum) const
{
	FRadialDamageParams CalculatedParams = Super::GetDamageParams_Implementation(OtherActor, HitLocation, OutMomentum);

	// When hitting a pawn within bonus point blank time
	AUTCharacter* OtherCharacter = Cast<AUTCharacter>(OtherActor);
	if (OtherCharacter != NULL)
	{
		const float BonusTime = GetLifeSpan() - InitialLifeSpan + MaxBonusTime;
		if (BonusTime > 0.0f)
		{
			// Apply bonus damage
			const float CharacterRadius = OtherCharacter->GetSimpleCollisionRadius();
			const float OffCenterDistance = FMath::PointDistToLine(OtherActor->GetActorLocation(), GetVelocity().GetSafeNormal(), HitLocation);
			const float OffCenterMultiplier = FMath::Max(0.f, 2.f * CharacterRadius - OffCenterDistance) / CharacterRadius;
			CalculatedParams.BaseDamage += CenteredDamageBonus * BonusTime * OffCenterMultiplier;
			OutMomentum += CenteredMomentumBonus * BonusTime;
		}
	}

	return CalculatedParams;
}
