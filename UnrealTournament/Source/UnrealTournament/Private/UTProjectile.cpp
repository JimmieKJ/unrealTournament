// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProjectile.h"


AUTProjectile::AUTProjectile(const class FPostConstructInitializeProperties& PCIP) 
	: Super(PCIP)
{
	// Use a sphere as a simple collision representation
	CollisionComp = PCIP.CreateDefaultSubobject<USphereComponent>(this, TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(5.0f);
	CollisionComp->BodyInstance.SetCollisionProfileName("Projectile");			// Collision profiles are defined in DefaultEngine.ini
	CollisionComp->OnComponentHit.AddDynamic(this, &AUTProjectile::OnHit);		// set up a notification for when this component overlaps something
	CollisionComp->bTraceComplexOnMove = true;
	RootComponent = CollisionComp;

	// Use a ProjectileMovementComponent to govern this projectile's movement
	ProjectileMovement = PCIP.CreateDefaultSubobject<UProjectileMovementComponent>(this, TEXT("ProjectileComp"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 3000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = true;

	// Die after 3 seconds by default
	InitialLifeSpan = 3.0f;

	DamageParams.BaseDamage = 20;
	DamageParams.DamageFalloff = 1.0;
	Momentum = 50000.0f;
}

void AUTProjectile::BeginPlay()
{
	if (SpawnInstigator != NULL)
	{
		Instigator = SpawnInstigator;
	}

	Super::BeginPlay();
	
	if (Instigator != NULL)
	{
		InstigatorController = Instigator->Controller;
	}
}

void AUTProjectile::OnHit(AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor != NULL && OtherActor != this && OtherActor != Instigator && OtherComp != NULL && !bExploded)
	{
		// TODO: replicated momentum handling
		OtherComp->AddImpulseAtLocation(GetVelocity() * 100.0f, GetActorLocation());

		FRadialDamageEvent Event;
		Event.Params = DamageParams;
		Event.Params.MinimumDamage = DamageParams.BaseDamage; // force full damage for direct hit
		Event.DamageTypeClass = MyDamageType;
		Event.Origin = Hit.Location;
		OtherActor->TakeDamage(DamageParams.BaseDamage, Event, InstigatorController, this);

		ImpactedActor = OtherActor;
		Explode(Hit.Location, Hit.Normal);
		ImpactedActor = NULL;
	}
}

void AUTProjectile::Explode(const FVector& HitLocation, const FVector& HitNormal)
{
	if (!bExploded)
	{
		TArray<AActor*> IgnoreActors;
		if (ImpactedActor != NULL)
		{
			IgnoreActors.Add(ImpactedActor);
		}
		UGameplayStatics::ApplyRadialDamageWithFalloff(this, DamageParams.BaseDamage, DamageParams.MinimumDamage, HitLocation, DamageParams.InnerRadius, DamageParams.OuterRadius, DamageParams.DamageFalloff, MyDamageType, IgnoreActors, this, InstigatorController);
		bTearOff = true;
		bExploded = true;
		if (GetNetMode() != NM_DedicatedServer)
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionEffect, GetActorLocation(), GetActorRotation(), true);
		}
		ShutDown();
	}
}

void AUTProjectile::ShutDown()
{
	SetLifeSpan(0.2f);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMovement->SetActive(false);
	bHidden = true;
	bExploded = true;
	// TODO: remove effects, sounds
}

void AUTProjectile::TornOff()
{
	Explode(GetActorLocation(), FVector(0.0f, 0.0f, 1.0f));
}