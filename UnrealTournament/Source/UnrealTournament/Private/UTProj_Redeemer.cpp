// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UnrealNetwork.h"
#include "UTProjectileMovementComponent.h"
#include "UTImpactEffect.h"
#include "UTProj_Redeemer.h"
#include "UTCTFRewardMessage.h"

AUTProj_Redeemer::AUTProj_Redeemer(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	CapsuleComp = ObjectInitializer.CreateOptionalDefaultSubobject<UCapsuleComponent>(this, TEXT("CapsuleComp"));
	if (CapsuleComp != NULL)
	{
		CapsuleComp->BodyInstance.SetCollisionProfileName("ProjectileShootable");			// Collision profiles are defined in DefaultEngine.ini
		CapsuleComp->OnComponentBeginOverlap.AddDynamic(this, &AUTProjectile::OnOverlapBegin);
		CapsuleComp->bTraceComplexOnMove = true;
		CapsuleComp->InitCapsuleSize(16.f, 70.0f);
		CapsuleComp->SetRelativeRotation(FRotator(90.f, 90.f, 90.f));
		CapsuleComp->AttachParent = RootComponent;
	}

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
}

void AUTProj_Redeemer::RedeemerDenied(AController* InstigatedBy)
{
	APlayerState* InstigatorPS = InstigatorController ? InstigatorController->PlayerState : NULL;
	APlayerState* InstigatedbyPS = InstigatedBy ? InstigatedBy->PlayerState : NULL;
	if (Cast<AUTPlayerController>(InstigatedBy))
	{
		Cast<AUTPlayerController>(InstigatedBy)->SendPersonalMessage(UUTCTFRewardMessage::StaticClass(), 0, InstigatedbyPS, InstigatorPS, NULL);
	}
	if (Cast<AUTPlayerController>(InstigatorController))
	{
		Cast<AUTPlayerController>(InstigatorController)->SendPersonalMessage(UUTCTFRewardMessage::StaticClass(), 0, InstigatedbyPS, InstigatorPS, NULL);
	}
}

float AUTProj_Redeemer::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(EventInstigator);
	bool bUsingClientSideHits = UTPC && (UTPC->GetPredictionTime() > 0.f);
	if ((Role == ROLE_Authority) && !bUsingClientSideHits)
	{
		Detonate(EventInstigator);
		RedeemerDenied(EventInstigator);
	}
	else if ((Role != ROLE_Authority) && bUsingClientSideHits)
	{
		UTPC->ServerNotifyProjectileHit(this, GetActorLocation(), DamageCauser, GetWorld()->GetTimeSeconds());
	}

	return Damage;
}

void AUTProj_Redeemer::NotifyClientSideHit(AUTPlayerController* InstigatedBy, FVector HitLocation, AActor* DamageCauser)
{
	Detonate(InstigatedBy);
	RedeemerDenied(InstigatedBy);
}

void AUTProj_Redeemer::Detonate(class AController* InstigatedBy)
{
	bDetonated = true;
	if (!bExploded)
	{
		bExploded = true;

		if (Role == ROLE_Authority)
		{
			bTearOff = true;
			bReplicateUTMovement = true; // so position of explosion is accurate even if flight path was a little off
		}

		if (DetonateEffects != NULL)
		{
			DetonateEffects.GetDefaultObject()->SpawnEffect(GetWorld(), FTransform(GetActorRotation(), GetActorLocation()), CollisionComp, this, InstigatorController);
		}

		if (Role == ROLE_Authority)
		{
			if (DetonateDamageParams.OuterRadius > 0.0f)
			{
				TArray<AActor*> IgnoreActors;
				UUTGameplayStatics::UTHurtRadius(this, DetonateDamageParams.BaseDamage, DetonateDamageParams.MinimumDamage, DetonateMomentum, GetActorLocation(), DetonateDamageParams.InnerRadius, DetonateDamageParams.OuterRadius, DetonateDamageParams.DamageFalloff,
					DetonateDamageType, IgnoreActors, this, InstigatedBy, nullptr, nullptr, 0.f);
			}
		}
		ShutDown();
	}
}

void AUTProj_Redeemer::Explode_Implementation(const FVector& HitLocation, const FVector& HitNormal, UPrimitiveComponent* HitComp)
{
	if (!bExploded)
	{
		if (bDetonated)
		{
			Detonate(NULL);
			return;
		}

		//Guarantee detonation on projectile collision
		AUTProjectile* Projectile = Cast<AUTProjectile>(ImpactedActor);
		if (Projectile != nullptr)
		{
			if (Role == ROLE_Authority && Projectile->InstigatorController != nullptr)
			{
				RedeemerDenied(Projectile->InstigatorController);
			}
			Detonate(Projectile->InstigatorController);
			return;
		}

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
			ExplodeHitLocation = HitLocation + HitNormal;
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
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTProj_Redeemer::ExplodeStage2, ExplosionTimings[0]);
}
void AUTProj_Redeemer::ExplodeStage2()
{
	ExplodeStage(ExplosionRadii[1]);
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTProj_Redeemer::ExplodeStage3, ExplosionTimings[1]);
}
void AUTProj_Redeemer::ExplodeStage3()
{
	ExplodeStage(ExplosionRadii[2]);
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTProj_Redeemer::ExplodeStage4, ExplosionTimings[2]);
}
void AUTProj_Redeemer::ExplodeStage4()
{
	ExplodeStage(ExplosionRadii[3]);
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTProj_Redeemer::ExplodeStage5, ExplosionTimings[3]);
}
void AUTProj_Redeemer::ExplodeStage5()
{
	ExplodeStage(ExplosionRadii[4]);
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTProj_Redeemer::ExplodeStage6, ExplosionTimings[4]);
}
void AUTProj_Redeemer::ExplodeStage6()
{
	ExplodeStage(ExplosionRadii[5]);
	ShutDown();
}

void AUTProj_Redeemer::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTProj_Redeemer, bDetonated, COND_None);
}

