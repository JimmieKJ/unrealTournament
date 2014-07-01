// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProjectile.h"
#include "UTProjectileMovementComponent.h"

AUTProjectile::AUTProjectile(const class FPostConstructInitializeProperties& PCIP) 
	: Super(PCIP)
{
	// Use a sphere as a simple collision representation
	CollisionComp = PCIP.CreateDefaultSubobject<USphereComponent>(this, TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(5.0f);
	CollisionComp->BodyInstance.SetCollisionProfileName("Projectile");			// Collision profiles are defined in DefaultEngine.ini
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AUTProjectile::OnOverlapBegin);
	CollisionComp->bTraceComplexOnMove = true;
	RootComponent = CollisionComp;

	// Use a ProjectileMovementComponent to govern this projectile's movement
	ProjectileMovement = PCIP.CreateDefaultSubobject<UUTProjectileMovementComponent>(this, TEXT("ProjectileComp"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 3000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &AUTProjectile::OnStop);
	ProjectileMovement->OnProjectileBounce.AddDynamic(this, &AUTProjectile::OnBounce);

	// Die after 3 seconds by default
	InitialLifeSpan = 3.0f;

	DamageParams.BaseDamage = 20;
	DamageParams.DamageFalloff = 1.0;
	Momentum = 50000.0f;

	SetReplicates(true);
	bNetTemporary = false;
	bReplicateInstigator = true;
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

	if (Role == ROLE_Authority)
	{
		ProjectileMovement->Velocity.Z += TossZ;
	}
}

void AUTProjectile::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	ProjectileMovement->Velocity = NewVelocity;
}

void AUTProjectile::OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	FHitResult Hit;
	OtherComp->LineTraceComponent(Hit, GetActorLocation() - GetVelocity() * 10.0, GetActorLocation() + GetVelocity(), FCollisionQueryParams(GetClass()->GetFName(), CollisionComp->bTraceComplexOnMove, this));
	ProcessHit(OtherActor, OtherComp, Hit.Location, Hit.Normal);
}
void AUTProjectile::OnStop(const FHitResult& Hit)
{
	ProcessHit(Hit.Actor.Get(), Hit.Component.Get(), Hit.Location, Hit.Normal);
}
void AUTProjectile::OnBounce(const struct FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	bCanHitInstigator = true;

	// Spawn bounce effect
	if (GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BounceEffect, ImpactResult.Location, ImpactResult.ImpactNormal.Rotation(), true);
	}
	// Play bounce sound
	if (BounceSound != NULL)
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), BounceSound, this, SRT_IfSourceNotReplicated, false);
	}
}

void AUTProjectile::ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	// note: on clients we assume spawn time impact is invalid since in such a case the projectile would generally have not survived to be replicated at all
	if ( OtherActor != this && (OtherActor != Instigator || Instigator == NULL || bCanHitInstigator) && OtherComp != NULL && !bExploded && (Role == ROLE_Authority || CreationTime != GetWorld()->TimeSeconds)
		// projectiles that are shootable always win against projectiles that are not
		&& (Cast<AUTProjectile>(OtherActor) == NULL || OtherComp->GetCollisionObjectType() == COLLISION_PROJECTILE_SHOOTABLE || OtherComp->GetCollisionResponseToChannel(COLLISION_PROJECTILE) > ECR_Ignore) )
	{
		if (OtherActor != NULL)
		{
			DamageImpactedActor(OtherActor, OtherComp, HitLocation, HitNormal);
		}

		ImpactedActor = OtherActor;
		Explode(HitLocation, HitNormal);
		ImpactedActor = NULL;

		if (Cast<AUTProjectile>(OtherActor) != NULL)
		{
			// since we'll probably be destroyed or lose collision here, make sure we trigger the other projectile so shootable projectiles colliding is consistent (both explode)

			UPrimitiveComponent* MyCollider = CollisionComp;
			if (CollisionComp == NULL || CollisionComp->GetCollisionObjectType() != COLLISION_PROJECTILE_SHOOTABLE)
			{
				// our primary collision component isn't the shootable one; try to find one that is
				TArray<UPrimitiveComponent*> Components;
				GetComponents<UPrimitiveComponent>(Components);
				for (int32 i = 0; i < Components.Num(); i++)
				{
					if (Components[i]->GetCollisionObjectType() == COLLISION_PROJECTILE_SHOOTABLE)
					{
						MyCollider = Components[i];
						break;
					}
				}
			}

			((AUTProjectile*)OtherActor)->ProcessHit(this, MyCollider, HitLocation, -HitNormal);
		}
	}
}

void AUTProjectile::DamageImpactedActor_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	// treat as point damage if projectile has no radius
	if (DamageParams.OuterRadius > 0.0f)
	{
		FUTRadialDamageEvent Event;
		Event.BaseMomentumMag = Momentum;
		Event.Params = GetDamageParams(OtherActor, HitLocation, Event.BaseMomentumMag);
		Event.Params.MinimumDamage = Event.Params.BaseDamage; // force full damage for direct hit
		Event.DamageTypeClass = MyDamageType;
		Event.Origin = HitLocation;
		new(Event.ComponentHits) FHitResult(OtherActor, OtherComp, HitLocation, HitNormal);
		Event.ComponentHits[0].TraceStart = HitLocation - GetVelocity();
		Event.ComponentHits[0].TraceEnd = HitLocation + GetVelocity();
		OtherActor->TakeDamage(Event.Params.BaseDamage, Event, InstigatorController, this);
	}
	else
	{
		FUTPointDamageEvent Event;
		float AdjustedMomentum = Momentum;
		Event.Damage = GetDamageParams(OtherActor, HitLocation, AdjustedMomentum).BaseDamage;
		Event.DamageTypeClass = MyDamageType;
		Event.HitInfo = FHitResult(OtherActor, OtherComp, HitLocation, HitNormal);
		Event.ShotDirection = GetVelocity().SafeNormal();
		Event.Momentum = Event.ShotDirection * AdjustedMomentum;
		OtherActor->TakeDamage(Event.Damage, Event, InstigatorController, this);
	}
}

void AUTProjectile::Explode_Implementation(const FVector& HitLocation, const FVector& HitNormal)
{
	if (!bExploded)
	{
		float AdjustedMomentum = Momentum;
		FRadialDamageParams AdjustedDamageParams = GetDamageParams(NULL, HitLocation, AdjustedMomentum);
		if (AdjustedDamageParams.OuterRadius > 0.0f)
		{
			TArray<AActor*> IgnoreActors;
			if (ImpactedActor != NULL)
			{
				IgnoreActors.Add(ImpactedActor);
			}
			UUTGameplayStatics::UTHurtRadius(this, AdjustedDamageParams.BaseDamage, AdjustedDamageParams.MinimumDamage, AdjustedMomentum, HitLocation, AdjustedDamageParams.InnerRadius, AdjustedDamageParams.OuterRadius, AdjustedDamageParams.DamageFalloff, MyDamageType, IgnoreActors, this, InstigatorController);
		}
		if (Role == ROLE_Authority)
		{
			bTearOff = true;
		}
		bExploded = true;
		UUTGameplayStatics::UTPlaySound(GetWorld(), ExplosionSound, this, ESoundReplicationType::SRT_IfSourceNotReplicated);
		if (GetNetMode() != NM_DedicatedServer)
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionEffect, GetActorLocation(), HitNormal.Rotation(), true);
		}
		ShutDown();
	}
}

void AUTProjectile::ShutDown()
{
	SetActorEnableCollision(false);
	ProjectileMovement->SetActive(false);
	// hide components that aren't particle systems; deactivate particle systems so they die off naturally; stop ambient sounds
	bool bFoundParticles = false;
	TArray<USceneComponent*> Components;
	GetComponents<USceneComponent>(Components);
	for (int32 i = 0; i < Components.Num(); i++)
	{
		UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(Components[i]);
		if (PSC != NULL)
		{
			// tick the particles one last time for e.g. SpawnPerUnit effects (particularly noticeable improvement for fast moving projectiles)
			PSC->TickComponent(0.0f, LEVELTICK_All, NULL);
			PSC->DeactivateSystem();
			PSC->bAutoDestroy = true;
			bFoundParticles = true;
		}
		else
		{
			UAudioComponent* Audio = Cast<UAudioComponent>(Components[i]);
			if (Audio != NULL)
			{
				Audio->Stop();
			}
			else
			{
				Components[i]->SetHiddenInGame(true);
				Components[i]->SetVisibility(false);
			}
		}
	}
	// if some particles remain, defer destruction a bit to give them time to die on their own
	SetLifeSpan((bFoundParticles && GetNetMode() != NM_DedicatedServer) ? 2.0f : 0.2f);

	OnShutdown();

	bExploded = true;
}

void AUTProjectile::TornOff()
{
	if (bExploded)
	{
		ShutDown(); // make sure it took effect; LifeSpan in particular won't unless we're authority
	}
	else
	{
		Explode(GetActorLocation(), FVector(0.0f, 0.0f, 1.0f));
	}
}

FRadialDamageParams AUTProjectile::GetDamageParams_Implementation(AActor* OtherActor, const FVector& HitLocation, float& OutMomentum) const
{
	OutMomentum = Momentum;
	return DamageParams;
}
