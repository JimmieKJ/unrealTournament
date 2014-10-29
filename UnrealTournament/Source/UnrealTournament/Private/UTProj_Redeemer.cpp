// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProjectileMovementComponent.h"
#include "UTImpactEffect.h"
#include "UTProj_Redeemer.h"
#include "UTLastSecondMessage.h"

AUTProj_Redeemer::AUTProj_Redeemer(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	// Movement
	ProjectileMovement->InitialSpeed = 2000.f;
	ProjectileMovement->MaxSpeed = 2000.f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;

	ExplosionTimings[0] = 0.5;
	ExplosionTimings[1] = 0.2;
	ExplosionTimings[2] = 0.2;
	ExplosionTimings[3] = 0.2;
	ExplosionTimings[4] = 0.2;

	ExplosionRadii[0] = 0.125f;
	ExplosionRadii[1] = 0.3f;
	ExplosionRadii[2] = 0.475f;
	ExplosionRadii[3] = 0.65f;
	ExplosionRadii[4] = 0.825f;
	ExplosionRadii[5] = 1.0f;

	CollisionFreeRadius = 1200;

	InitialLifeSpan = 20.0f;
	bAlwaysShootable = true;
	bExplosionAlwaysRelevant = true;
}

void AUTProj_Redeemer::RedeemerDenied(AController* InstigatedBy)
{
	APlayerState* InstigatorPS = InstigatorController ? InstigatorController->PlayerState : NULL;
	APlayerState* InstigatedbyPS = InstigatedBy ? InstigatedBy->PlayerState : NULL;
	if (Cast<AUTPlayerController>(InstigatedBy))
	{
		Cast<AUTPlayerController>(InstigatedBy)->ClientReceiveLocalizedMessage(UUTLastSecondMessage::StaticClass(), 0, InstigatedbyPS, InstigatorPS, NULL);
	}
	if (Cast<AUTPlayerController>(InstigatorController))
	{
		Cast<AUTPlayerController>(InstigatorController)->ClientReceiveLocalizedMessage(UUTLastSecondMessage::StaticClass(), 0, InstigatedbyPS, InstigatorPS, NULL);
	}
}

void AUTProj_Redeemer::ReceiveAnyDamage(float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, class AActor* DamageCauser)
{
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(InstigatedBy);
	bool bUsingClientSideHits = UTPC && (UTPC->GetPredictionTime() > 0.f);
	if ((Role == ROLE_Authority) && !bUsingClientSideHits)
	{
		Explode(GetActorLocation(), FVector(0.f, 0.f, 1.f), CollisionComp);
		RedeemerDenied(InstigatedBy);

	}
	else if ((Role != ROLE_Authority) && bUsingClientSideHits)
	{
		UTPC->ServerNotifyProjectileHit(this, GetActorLocation(), DamageCauser, GetWorld()->GetTimeSeconds());
	}
}

void AUTProj_Redeemer::NotifyClientSideHit(AUTPlayerController* InstigatedBy, FVector HitLocation, AActor* DamageCauser)
{
	Explode(GetActorLocation(), FVector(0.f, 0.f, 1.f), CollisionComp);
	RedeemerDenied(InstigatedBy);
}

void AUTProj_Redeemer::Explode_Implementation(const FVector& HitLocation, const FVector& HitNormal, UPrimitiveComponent* HitComp)
{
	if (!bExploded)
	{
		bExploded = true;
		
		if (Role == ROLE_Authority)
		{
			bTearOff = true;
			bReplicateUTMovement = true; // so position of explosion is accurate even if flight path was a little off
		}

		if (ExplosionEffects != NULL)
		{
			ExplosionEffects.GetDefaultObject()->SpawnEffect(GetWorld(), FTransform(HitNormal.Rotation(), HitLocation), HitComp, this, InstigatorController);
		}
		
		if (Role == ROLE_Authority)
		{
			ExplodeHitLocation = HitLocation + FVector(0, 0, 400);
			ExplodeMomentum = Momentum;
			ExplodeStage1();
		}
	}
}

void AUTProj_Redeemer::ExplodeStage(float RangeMultiplier)
{
	float AdjustedMomentum = ExplodeMomentum;
	FRadialDamageParams AdjustedDamageParams = GetDamageParams(NULL, ExplodeHitLocation, AdjustedMomentum);
	if (AdjustedDamageParams.OuterRadius > 0.0f)
	{
		TArray<AActor*> IgnoreActors;
		if (ImpactedActor != NULL)
		{
			IgnoreActors.Add(ImpactedActor);
		}

		UUTGameplayStatics::UTHurtRadius(this, AdjustedDamageParams.BaseDamage, AdjustedDamageParams.MinimumDamage, AdjustedMomentum, ExplodeHitLocation, RangeMultiplier * AdjustedDamageParams.InnerRadius, RangeMultiplier * AdjustedDamageParams.OuterRadius, AdjustedDamageParams.DamageFalloff,
			MyDamageType, IgnoreActors, this, InstigatorController, FFInstigatorController, FFDamageType, CollisionFreeRadius);
	}
}

void AUTProj_Redeemer::ExplodeStage1()
{
	ExplodeStage(ExplosionRadii[0]);
	GetWorldTimerManager().SetTimer(this, &AUTProj_Redeemer::ExplodeStage2, ExplosionTimings[0]);
}
void AUTProj_Redeemer::ExplodeStage2()
{
	ExplodeStage(ExplosionRadii[1]);
	GetWorldTimerManager().SetTimer(this, &AUTProj_Redeemer::ExplodeStage3, ExplosionTimings[1]);
}
void AUTProj_Redeemer::ExplodeStage3()
{
	ExplodeStage(ExplosionRadii[2]);
	GetWorldTimerManager().SetTimer(this, &AUTProj_Redeemer::ExplodeStage4, ExplosionTimings[2]);
}
void AUTProj_Redeemer::ExplodeStage4()
{
	ExplodeStage(ExplosionRadii[3]);
	GetWorldTimerManager().SetTimer(this, &AUTProj_Redeemer::ExplodeStage5, ExplosionTimings[3]);
}
void AUTProj_Redeemer::ExplodeStage5()
{
	ExplodeStage(ExplosionRadii[4]);
	GetWorldTimerManager().SetTimer(this, &AUTProj_Redeemer::ExplodeStage6, ExplosionTimings[4]);
}
void AUTProj_Redeemer::ExplodeStage6()
{
	ExplodeStage(ExplosionRadii[5]);
	ShutDown();
}
