// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProjectileMovementComponent.h"
#include "UTProj_FlakShell.h"
#include "UTProj_FlakShard.h"

AUTProj_FlakShell::AUTProj_FlakShell(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Movement
	ProjectileMovement->InitialSpeed = 2300.f;
	ProjectileMovement->MaxSpeed = 2400.f;
	ProjectileMovement->ProjectileGravityScale = 1.0f;

	CollisionComp->InitSphereRadius(10);

	TossZ = 430.0f;

	DamageParams.BaseDamage = 90.0f;
	DamageParams.OuterRadius = 300.0f;
	DamageParams.MinimumDamage = 20.f;

	Momentum = 110000.0f;

	InitialLifeSpan = 6.0f;

	// Flak shell
	ShardSpawnCount = 3;
	ShardSpawnAngle = 20.0f;
}

void AUTProj_FlakShell::Explode_Implementation(const FVector& HitLocation, const FVector& HitNormal, UPrimitiveComponent* HitComp)
{
	if (!bExploded)
	{
		Super::Explode_Implementation(HitLocation, HitNormal, HitComp);

		// On explosion spawn additional flak shards
		if (Role == ROLE_Authority && ShardClass != NULL && ShardSpawnCount > 0)
		{
			if (Cast<APawn>(ImpactedActor))
			{
				AUTProjectile* DefaultShard = Cast<AUTProjectile>(ShardClass->GetDefaultObject());
				if (DefaultShard)
				{
					// no shards, just give the damage directly to the pawn
					FUTPointDamageEvent Event;
					float AdjustedMomentum = DefaultShard->Momentum;
					Event.Damage = DefaultShard->DamageParams.BaseDamage;
					Event.DamageTypeClass = DefaultShard->MyDamageType;
					Event.HitInfo = FHitResult(ImpactedActor, HitComp, HitLocation, HitNormal);
					Event.ShotDirection = GetVelocity().GetSafeNormal();
					Event.Momentum = Event.ShotDirection * AdjustedMomentum;
					ImpactedActor->TakeDamage(Event.Damage, Event, InstigatorController, this);
				}
			}
			else
			{
				// push spawn location out slightly, but be careful not to push it into world geometry
				FVector SpawnPos = HitLocation + 10.0f * HitNormal;
				FCollisionQueryParams TraceParams;
				TraceParams.AddIgnoredActor(this);
				TraceParams.AddIgnoredActor(ImpactedActor);
				FHitResult Hit;
				if (GetWorld()->LineTraceSingleByChannel(Hit, SpawnPos, HitLocation, ECC_Visibility, TraceParams))
				{
					SpawnPos = Hit.Location;
				}

				// Setup spawn parameters
				FActorSpawnParameters Params;
				Params.Instigator = Instigator;
				Params.Owner = Instigator;

				for (int32 i = 0; i < ShardSpawnCount; ++i)
				{
					FVector MovementDir = GetActorRotation().Vector();
					FVector ShardDir = (MovementDir - 2.0f * HitNormal * (MovementDir | HitNormal));
					// Randomize spawn direction along bounce direction
					const FRotator SpawnRotation = FMath::VRandCone(ShardDir, FMath::DegreesToRadians(ShardSpawnAngle)).Rotation();

					// Spawn shard
					AUTProjectile* Proj = GetWorld()->SpawnActor<AUTProjectile>(ShardClass, SpawnPos, SpawnRotation, Params);
					// propagate InstigatorController, since Instigator->Controller might have been disconnected by now
					if (Proj != NULL && InstigatorController != NULL)
					{
						Proj->InstigatorController = InstigatorController;
					}
				}
			}
		}
	}
}