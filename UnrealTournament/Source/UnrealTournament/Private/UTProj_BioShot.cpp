// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProj_BioShot.h"
#include "UTProj_BioGlob.h"
#include "UnrealNetwork.h"
#include "UTImpactEffect.h"

static const float GOO_TIMER_TICK = 0.5f;

AUTProj_BioShot::AUTProj_BioShot(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	ProjectileMovement->InitialSpeed = 4000.0f;
	ProjectileMovement->MaxSpeed = 5000.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bInitialVelocityInLocalSpace = true;
	ProjectileMovement->ProjectileGravityScale = 0.9f;
	ProjectileMovement->bShouldBounce = false;

	DamageParams.BaseDamage = 21.0f;

	Momentum = 80000.0f;

	bLanded = false;
	SurfaceNormal = FVector(0.0f, 0.0f, 1.0f);
	SurfaceType = EHitType::HIT_None;
	SurfaceWallThreshold = 0.3f;
	FloorCollisionRadius = 40.0f;

	InitialLifeSpan = 10.0f;
	RestTime = 3.0f;
	bAlwaysShootable = true;
}

float AUTProj_BioShot::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser)
{
	if (Role == ROLE_Authority)
	{
		if (bLanded && !bExploded)
		{
			RemainingRestTime = -1.0;
			RemainingRestTimer();
		}
	}
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AUTProj_BioShot::OnRep_RemainingRestTime()
{
	if (RemainingRestTime <= 0.0 && !bExploded)
	{
		RemainingRestTimer();
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(this, &AUTProj_BioShot::RemainingRestTimer, RemainingRestTime, false);
	}
}

void AUTProj_BioShot::RemainingRestTimer()
{
	//Server ticks to update clients
	if (Role == ROLE_Authority)
	{
		RemainingRestTime -= GOO_TIMER_TICK;
	}

	if (RemainingRestTime <= 0.0 && !bExploded)
	{
		Explode(GetActorLocation(), SurfaceNormal);
	}
}

void AUTProj_BioShot::GrowCollision()
{
	CollisionComp->SetSphereRadius(FloorCollisionRadius, false);
}

void AUTProj_BioShot::Landed(UPrimitiveComponent* HitComp)
{
	if (!bLanded)
	{
		bLanded = true;
		bCanHitInstigator = true;

		//Change the collision so that weapons make it explode
		CollisionComp->SetCollisionProfileName("ProjectileShootable");
		GrowCollision();

		//Rotate away from the floor
		FRotator NewRotation = (SurfaceNormal).Rotation();
		NewRotation.Roll = FMath::FRand() * 360.0f;	//Random spin
		SetActorRotation(NewRotation);

		//Stop the projectile
		ProjectileMovement->ProjectileGravityScale = 0.0f;
		ProjectileMovement->Velocity = FVector::ZeroVector;

		//Start the explosion timer
		RemainingRestTime = RestTime;
		if (Role == ROLE_Authority)
		{
			GetWorld()->GetTimerManager().SetTimer(this, &AUTProj_BioShot::RemainingRestTimer, GOO_TIMER_TICK, true);
		}
		else
		{
			GetWorld()->GetTimerManager().SetTimer(this, &AUTProj_BioShot::RemainingRestTimer, RemainingRestTime, false);
		}

		//Stop any flight looping sounds
		TArray<USceneComponent*> Components;
		GetComponents<USceneComponent>(Components);
		for (int32 i = 0; i < Components.Num(); i++)
		{
			UAudioComponent* Audio = Cast<UAudioComponent>(Components[i]);
			if (Audio != NULL)
			{
				if (Audio->Sound != NULL && Audio->Sound->GetDuration() >= INDEFINITELY_LOOPING_DURATION)
				{
					Audio->Stop();
				}
			}
		}

		//Spawn Effects
		OnLanded();

		if (LandedEffects != NULL)
		{
			LandedEffects.GetDefaultObject()->SpawnEffect(GetWorld(), FTransform(SurfaceNormal.Rotation(), GetActorLocation()), HitComp, this, InstigatorController);
		}
	}
}

void AUTProj_BioShot::OnLanded_Implementation()
{
}

void AUTProj_BioShot::ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	if (Cast<AUTProj_BioGlob>(OtherActor) == NULL) // bio glob's ProcessHit() will absorb us
	{
		if (Cast<AUTCharacter>(OtherActor) != NULL || Cast<AUTProjectile>(OtherActor) != NULL)
		{
			Super::ProcessHit_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);
		}
		else if (!bLanded)
		{
			//Determine if we hit a Wall/Floor/Ceiling
			SurfaceNormal = HitNormal;
			if (FMath::Abs(SurfaceNormal.Z) > SurfaceWallThreshold) // A wall will have a low Z in the HitNormal since it's a unit vector
			{
				SurfaceType = (SurfaceNormal.Z >= 0) ? HIT_Floor : HIT_Ceiling;
			}
			else
			{
				SurfaceType = HIT_Wall;
			}

			SetActorLocation(HitLocation);

			Landed(OtherComp);
		}
	}
}
void AUTProj_BioShot::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTProj_BioShot, RemainingRestTime, COND_None);
}