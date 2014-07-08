// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProjectile.h"
#include "UTProjectileMovementComponent.h"
#include "UnrealNetwork.h"

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

	InitialReplicationTick.bCanEverTick = true;
	InitialReplicationTick.bTickEvenWhenPaused = true;
	InitialReplicationTick.SetTickFunctionEnable(true);
	ProjectileMovement->PrimaryComponentTick.AddPrerequisite(this, InitialReplicationTick);
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

		UNetDriver* NetDriver = GetNetDriver();
		if (NetDriver != NULL && NetDriver->IsServer())
		{
			InitialReplicationTick.Target = this;
			InitialReplicationTick.RegisterTickFunction(GetLevel());
		}
	}
}

void AUTProjectile::SendInitialReplication()
{
	// force immediate replication for projectiles with extreme speed or radial effects
	// this prevents clients from being hit by invisible projectiles in almost all cases, because it'll exist locally before it has even been moved
	UNetDriver* NetDriver = GetNetDriver();
	if (NetDriver != NULL && NetDriver->IsServer() && !bPendingKillPending && (ProjectileMovement->Velocity.Size() >= 7500.0f || DamageParams.OuterRadius > 0.0f))
	{
		NetDriver->ReplicationFrame++;
		for (int32 i = 0; i < NetDriver->ClientConnections.Num(); i++)
		{
			if (NetDriver->ClientConnections[i]->State == USOCK_Open && NetDriver->ClientConnections[i]->PlayerController != NULL && NetDriver->ClientConnections[i]->IsNetReady(0))
			{
				AActor* ViewTarget = NetDriver->ClientConnections[i]->PlayerController->GetViewTarget();
				if (ViewTarget == NULL)
				{
					ViewTarget = NetDriver->ClientConnections[i]->PlayerController;
				}
				FVector ViewLocation = ViewTarget->GetActorLocation();
				{
					FRotator ViewRotation = NetDriver->ClientConnections[i]->PlayerController->GetControlRotation();
					NetDriver->ClientConnections[i]->PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
				}
				if (IsNetRelevantFor(NetDriver->ClientConnections[i]->PlayerController, ViewTarget, ViewLocation))
				{
					UActorChannel* Ch = NetDriver->ClientConnections[i]->ActorChannels.FindRef(this);
					if (Ch == NULL)
					{
						// can't - protected: if (NetDriver->IsLevelInitializedForActor(this, NetDriver->ClientConnections[i]))
						if (NetDriver->ClientConnections[i]->ClientWorldPackageName == GetWorld()->GetOutermost()->GetFName() && NetDriver->ClientConnections[i]->ClientHasInitializedLevelFor(this))
						{
							Ch = (UActorChannel *)NetDriver->ClientConnections[i]->CreateChannel(CHTYPE_Actor, 1);
							if (Ch != NULL)
							{
								Ch->SetChannelActor(this);
							}
						}
					}
					if (Ch != NULL && Ch->OpenPacketId.First == INDEX_NONE)
					{
						// bIsReplicatingActor being true should be impossible but let's be sure
						if (!Ch->bIsReplicatingActor)
						{
							Ch->ReplicateActor();

							// force a replicated location update at the end of the frame after the physics as well
							bForceNextRepMovement = true;
						}
					}
				}
			}
		}
	}
}

void AUTProjectile::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	if (&ThisTickFunction == &InitialReplicationTick)
	{
		SendInitialReplication();
		InitialReplicationTick.UnRegisterTickFunction();
	}
	else
	{
		Super::TickActor(DeltaTime, TickType, ThisTickFunction);
	}
}

void AUTProjectile::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
	if (bForceNextRepMovement)
	{
		GatherCurrentMovement();
		DOREPLIFETIME_ACTIVE_OVERRIDE(AActor, ReplicatedMovement, true);
		bForceNextRepMovement = false;
	}
	else
	{
		Super::PreReplication(ChangedPropertyTracker);
	}
}

void AUTProjectile::PostNetReceiveLocation()
{
	Super::PostNetReceiveLocation();

	// tick particle systems for e.g. SpawnPerUnit trails
	if (!bTearOff && !bExploded) // if torn off ShutDown() will do this
	{
		TArray<USceneComponent*> Components;
		GetComponents<USceneComponent>(Components);
		for (int32 i = 0; i < Components.Num(); i++)
		{
			UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(Components[i]);
			if (PSC != NULL)
			{
				PSC->TickComponent(0.0f, LEVELTICK_All, NULL);
			}
		}
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
			bReplicateMovement = true; // so position of explosion is accurate even if flight path was a little off
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
				// only stop looping (ambient) sounds - note that the just played explosion sound may be encountered here
				if (Audio->Sound != NULL && Audio->Sound->GetDuration() >= INDEFINITELY_LOOPING_DURATION)
				{
					Audio->Stop();
				}
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
