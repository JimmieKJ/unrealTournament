// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProjectile.h"
#include "UTProjectileMovementComponent.h"
#include "UnrealNetwork.h"
#include "Engine/ActorChannel.h"
#include "Particles/ParticleSystemComponent.h"
#include "UTImpactEffect.h"
#include "UTTeleporter.h"

DEFINE_LOG_CATEGORY_STATIC(LogUTProjectile, Log, All);

AUTProjectile::AUTProjectile(const class FPostConstructInitializeProperties& PCIP) 
	: Super(PCIP)
{
	// Use a sphere as a simple collision representation
	CollisionComp = PCIP.CreateOptionalDefaultSubobject<USphereComponent>(this, TEXT("SphereComp"));
	if (CollisionComp != NULL)
	{
		CollisionComp->InitSphereRadius(0.0f);
		CollisionComp->BodyInstance.SetCollisionProfileName("Projectile");			// Collision profiles are defined in DefaultEngine.ini
		CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AUTProjectile::OnOverlapBegin);
		CollisionComp->bTraceComplexOnMove = true;
		RootComponent = CollisionComp;
	}

	// @TODO FIXMESTEVE move to just projectiles that want this, add warning if no PawnOverlapSphere and set OverlapRadius
	OverlapRadius = 10.f;
	PawnOverlapSphere = PCIP.CreateOptionalDefaultSubobject<USphereComponent>(this, TEXT("AssistSphereComp"));
	if (PawnOverlapSphere != NULL)
	{
		//PawnOverlapSphere->bHiddenInGame = false;
		//PawnOverlapSphere->bVisible = true;
		PawnOverlapSphere->InitSphereRadius(OverlapRadius);
		PawnOverlapSphere->BodyInstance.SetCollisionProfileName("ProjectileOverlap");
		PawnOverlapSphere->bTraceComplexOnMove = true; // @TODO FIXMESTEVE should be false?
		PawnOverlapSphere->AttachParent = RootComponent;
	}

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
	InstigatorVelocityPct = 0.f;

	SetReplicates(true);
	bNetTemporary = false;
	bReplicateInstigator = true;

	InitialReplicationTick.bCanEverTick = true;
	InitialReplicationTick.bTickEvenWhenPaused = true;
	InitialReplicationTick.SetTickFunctionEnable(true);
	ProjectileMovement->PrimaryComponentTick.AddPrerequisite(this, InitialReplicationTick);

	bAlwaysShootable = false;
	bIsEnergyProjectile = false;
	bFakeClientProjectile = false;
	bReplicateUTMovement = false;
	bReplicateMovement = false;

	bInitiallyWarnTarget = true;
}

void AUTProjectile::BeginPlay()
{
	if (PawnOverlapSphere != NULL)
	{
		PawnOverlapSphere->SetSphereRadius(OverlapRadius);
	}

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

		if (bInitiallyWarnTarget && InstigatorController != NULL)
		{
			AUTBot* TargetBot = NULL;

			AUTPlayerController* PC = Cast<AUTPlayerController>(InstigatorController);
			if (PC != NULL)
			{
				if (PC->LastShotTargetGuess != NULL)
				{
					TargetBot = Cast<AUTBot>(PC->LastShotTargetGuess->Controller);
				}
			}
			else
			{
				AUTBot* MyBot = Cast<AUTBot>(InstigatorController);
				if (MyBot != NULL && Cast<APawn>(MyBot->GetTarget()) != NULL)
				{
					TargetBot = Cast<AUTBot>(((APawn*)MyBot->GetTarget())->Controller);
				}
			}
			if (TargetBot != NULL)
			{
				TargetBot->ReceiveProjWarning(this);
			}
		}
	}
	else
	{
		AUTPlayerController* MyPlayer = Cast<AUTPlayerController>(InstigatorController ? InstigatorController : GEngine->GetFirstLocalPlayerController(GetWorld()));
		if (MyPlayer)
		{
			// Move projectile to match where it is on server now (to make up for replication time)
			float CatchupTickDelta = MyPlayer->GetPredictionTime();
			if ((CatchupTickDelta > 0.f) && ProjectileMovement)
			{
				ProjectileMovement->TickComponent(CatchupTickDelta, LEVELTICK_All, NULL);
			}

			// look for associated fake client projectile
			AUTProjectile* BestMatch = NULL;
			FVector VelDir = GetVelocity().SafeNormal();
			for (int32 i = 0; i < MyPlayer->FakeProjectiles.Num(); i++)
			{
				AUTProjectile* Fake = MyPlayer->FakeProjectiles[i];
				if (!Fake || Fake->IsPendingKillPending())
				{
					MyPlayer->FakeProjectiles.RemoveAt(i, 1);
					i--;
				}
				else if (Fake->GetClass() == GetClass())
				{
					if (BestMatch)
					{
						// see if new one is better
						// must share direction unless falling! 
						if ((BestMatch->ProjectileMovement->ProjectileGravityScale > 0.f) || ((Fake->GetVelocity().SafeNormal() | VelDir) > 0.95f))
						{
							if ((BestMatch->GetActorLocation() - GetActorLocation()).Size() > (Fake->GetActorLocation() - GetActorLocation()).Size())
							{
								BestMatch = Fake;
							}
						}
					}
					else
					{
						BestMatch = Fake;
					}
				}
			}
			if (BestMatch)
			{
				// @TODO FIXMESTEVE - teleport to bestmatch location and interpolate?
				// @TODO FIXMESTEVE - rep to server to reduce error by changing Server ping overhead value

				float Error = (GetActorLocation() - BestMatch->GetActorLocation()).Size();
				if (((GetActorLocation() - BestMatch->GetActorLocation()) | BestMatch->GetVelocity()) > 0.f)
				{
					Error *= -1.f;
				}
				//UE_LOG(UT, Warning, TEXT("%s CORRECTION %f in msec %f"), *GetName(), Error, 1000.f * Error/GetVelocity().Size());

				BestMatch->ReplicatedMovement.Location = GetActorLocation();
				BestMatch->ReplicatedMovement.Rotation = GetActorRotation();
				BestMatch->PostNetReceiveLocationAndRotation();
				MyFakeProjectile = BestMatch;

				// @TODO FIXMESTEVE Can I move components instead of having two actors?
				// @TODO FIXMESTEVE if not, should interp fake projectile to my location instead of teleporting?
				TArray<USceneComponent*> Components;
				GetComponents<USceneComponent>(Components);
				for (int32 i = 0; i < Components.Num(); i++)
				{
					Components[i]->SetHiddenInGame(true);
					Components[i]->SetVisibility(false);
				}

				//BestMatch->Destroy();
				// @TODO FIXMESTEVE If bNetTemporary, destroy me right away (after fully replicated - need to copy over those properties), let fake become real
			}
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

void AUTProjectile::NotifyClientSideHit(AUTPlayerController* InstigatedBy, FVector HitLocation, AActor* DamageCauser)
{
}

static void GetLifetimeBlueprintReplicationList(const AActor * ThisActor, const UBlueprintGeneratedClass * MyClass, TArray< FLifetimeProperty > & OutLifetimeProps)
{
	if (MyClass == NULL)
	{
		return;
	}

	uint32 PropertiesLeft = MyClass->NumReplicatedProperties;

	for (TFieldIterator<UProperty> It(MyClass, EFieldIteratorFlags::ExcludeSuper); It && PropertiesLeft > 0; ++It)
	{
		UProperty * Prop = *It;
		if (Prop != NULL && Prop->GetPropertyFlags() & CPF_Net)
		{
			PropertiesLeft--;
			OutLifetimeProps.Add(FLifetimeProperty(Prop->RepIndex));
		}
	}

	return GetLifetimeBlueprintReplicationList(ThisActor, Cast< UBlueprintGeneratedClass >(MyClass->GetSuperStruct()), OutLifetimeProps);
}

void AUTProjectile::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	GetLifetimeBlueprintReplicationList(this, Cast< UBlueprintGeneratedClass >(GetClass()), OutLifetimeProps);

	//DOREPLIFETIME(AActor, Role);
	//DOREPLIFETIME(AActor, RemoteRole);
	//DOREPLIFETIME(AActor, Owner);
	DOREPLIFETIME(AActor, bHidden);

	DOREPLIFETIME(AActor, bTearOff);
	DOREPLIFETIME(AActor, bCanBeDamaged);
	DOREPLIFETIME(AActor, AttachmentReplication);

	DOREPLIFETIME(AActor, Instigator);

	//DOREPLIFETIME_CONDITION(AActor, ReplicatedMovement, COND_SimulatedOrPhysics);

	DOREPLIFETIME_CONDITION(AUTProjectile, UTProjReplicatedMovement, COND_SimulatedOrPhysics);
}

void AUTProjectile::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
	if (bForceNextRepMovement || bReplicateUTMovement)
	{
		GatherCurrentMovement();
		bForceNextRepMovement = false;
	}
}

void AUTProjectile::GatherCurrentMovement()
{
	/* @TODO FIXMESTEVE support projectiles uing rigid body physics
	UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(GetRootComponent());
	if (RootPrimComp && RootPrimComp->IsSimulatingPhysics())
	{
		FRigidBodyState RBState;
		RootPrimComp->GetRigidBodyState(RBState);

		ReplicatedMovement.FillFrom(RBState);
	}
	else 
	*/
	if (RootComponent != NULL)
	{
		// If we are attached, don't replicate absolute position
		if (RootComponent->AttachParent != NULL)
		{
			// Networking for attachments assumes the RootComponent of the AttachParent actor. 
			// If that's not the case, we can't update this, as the client wouldn't be able to resolve the Component and would detach as a result.
			if (AttachmentReplication.AttachParent != NULL)
			{
				AttachmentReplication.LocationOffset = RootComponent->RelativeLocation;
				AttachmentReplication.RotationOffset = RootComponent->RelativeRotation;
				AttachmentReplication.RelativeScale3D = RootComponent->RelativeScale3D;
			}
		}
		else
		{
			UTProjReplicatedMovement.Location = RootComponent->GetComponentLocation();
			UTProjReplicatedMovement.Rotation = RootComponent->GetComponentRotation();
			UTProjReplicatedMovement.LinearVelocity = GetVelocity();
		}
	}
}

void AUTProjectile::OnRep_UTProjReplicatedMovement()
{
	if (Role == ROLE_SimulatedProxy)
	{
		//ReplicatedAccel = UTReplicatedMovement.Acceleration;
		ReplicatedMovement.Location = UTProjReplicatedMovement.Location;
		ReplicatedMovement.Rotation = UTProjReplicatedMovement.Rotation;
		ReplicatedMovement.LinearVelocity = UTProjReplicatedMovement.LinearVelocity;
		ReplicatedMovement.AngularVelocity = FVector(0.f);
		ReplicatedMovement.bSimulatedPhysicSleep = false;
		ReplicatedMovement.bRepPhysics = false;

		OnRep_ReplicatedMovement();
	}
}

void AUTProjectile::PostNetReceiveLocationAndRotation()
{
	// FIXME: overwrote super because 4.4 version is busted and fails - check if this is fixed in 4.5...
	//Super::PostNetReceiveLocationAndRotation();
	if (RootComponent != NULL && RootComponent->IsRegistered() && (ReplicatedMovement.Location != GetActorLocation() || ReplicatedMovement.Rotation != GetActorRotation()))
	{
		TeleportTo(ReplicatedMovement.Location, ReplicatedMovement.Rotation, false, true); // note the 'true' for bNoCheck
	}

	// forward predict to get to position on server now
	if (!bFakeClientProjectile)
	{
		AUTPlayerController* MyPlayer = Cast<AUTPlayerController>(InstigatorController ? InstigatorController : GEngine->GetFirstLocalPlayerController(GetWorld()));
		if (MyPlayer)
		{
			float CatchupTickDelta = MyPlayer->GetPredictionTime();
			if ((CatchupTickDelta > 0.f) && ProjectileMovement)
			{
				ProjectileMovement->TickComponent(CatchupTickDelta, LEVELTICK_All, NULL);
			}
		}
	}

	if (MyFakeProjectile)
	{
		MyFakeProjectile->ReplicatedMovement.Location = GetActorLocation();
		MyFakeProjectile->ReplicatedMovement.Rotation = GetActorRotation();
		MyFakeProjectile->PostNetReceiveLocationAndRotation();
	}
	else
	{
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
}

void AUTProjectile::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	ProjectileMovement->Velocity = NewVelocity;
	if (MyFakeProjectile)
	{
		MyFakeProjectile->ProjectileMovement->Velocity = NewVelocity;
	}
}

void AUTProjectile::OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	UE_LOG(LogUTProjectile, Verbose, TEXT("%s::OnOverlapBegin OtherActor:%s bFromSweep:%d"), *GetName(), OtherActor ? *OtherActor->GetName() : TEXT("NULL"), int32(bFromSweep));

	FHitResult Hit;

	if (bFromSweep)
	{
		Hit = SweepResult;
	}
	else
	{
		OtherComp->LineTraceComponent(Hit, GetActorLocation() - GetVelocity() * 10.0, GetActorLocation() + GetVelocity(), FCollisionQueryParams(GetClass()->GetFName(), CollisionComp->bTraceComplexOnMove, this));
	}

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

bool AUTProjectile::InteractsWithProj(AUTProjectile* OtherProj)
{
	return (bAlwaysShootable || OtherProj->bAlwaysShootable || (bIsEnergyProjectile && OtherProj->bIsEnergyProjectile)) && (!bFakeClientProjectile && !OtherProj->bFakeClientProjectile);
}

void AUTProjectile::InitFakeProjectile(AUTPlayerController* OwningPlayer)
{
	bFakeClientProjectile = true;
	OwningPlayer->FakeProjectiles.Add(this);
}

void AUTProjectile::ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	UE_LOG(LogUTProjectile, Verbose, TEXT("%s::ProcessHit OtherActor:%s"), *GetName(), OtherActor ? *OtherActor->GetName() : TEXT("NULL"));

	// note: on clients we assume spawn time impact is invalid since in such a case the projectile would generally have not survived to be replicated at all
	if (OtherActor != this && (OtherActor != Instigator || Instigator == NULL || bCanHitInstigator) && OtherComp != NULL && !bExploded  && (Role == ROLE_Authority || CreationTime != GetWorld()->TimeSeconds)
		// don't blow up on non-blocking volumes
		// special case not blowing up on teleporters on overlap so teleporters have the option to teleport the projectile
		&& ((Cast<AUTTeleporter>(OtherActor) == NULL && Cast<AVolume>(OtherActor) == NULL) || GetVelocity().IsZero())
		&& (Cast<AUTProjectile>(OtherActor) == NULL || InteractsWithProj(Cast<AUTProjectile>(OtherActor))) )
	{
		if (bFakeClientProjectile)
		{
			ShutDown();
			return;
		}
		if (OtherActor != NULL)
		{
			DamageImpactedActor(OtherActor, OtherComp, HitLocation, HitNormal);
		}

		ImpactedActor = OtherActor;
		Explode(HitLocation, HitNormal, OtherComp);
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
	if (bFakeClientProjectile)
	{
		return;
	}
	AController* ResolvedInstigator = InstigatorController;
	TSubclassOf<UDamageType> ResolvedDamageType = MyDamageType;
	if (FFInstigatorController != NULL && InstigatorController != NULL)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL && GS->OnSameTeam(OtherActor, InstigatorController))
		{
			ResolvedInstigator = FFInstigatorController;
			if (FFDamageType != NULL)
			{
				ResolvedDamageType = FFDamageType;
			}
		}
	}

	// treat as point damage if projectile has no radius
	if (DamageParams.OuterRadius > 0.0f)
	{
		FUTRadialDamageEvent Event;
		Event.BaseMomentumMag = Momentum;
		Event.Params = GetDamageParams(OtherActor, HitLocation, Event.BaseMomentumMag);
		Event.Params.MinimumDamage = Event.Params.BaseDamage; // force full damage for direct hit
		Event.DamageTypeClass = ResolvedDamageType;
		Event.Origin = HitLocation;
		new(Event.ComponentHits) FHitResult(OtherActor, OtherComp, HitLocation, HitNormal);
		Event.ComponentHits[0].TraceStart = HitLocation - GetVelocity();
		Event.ComponentHits[0].TraceEnd = HitLocation + GetVelocity();
		OtherActor->TakeDamage(Event.Params.BaseDamage, Event, ResolvedInstigator, this);
	}
	else
	{
		FUTPointDamageEvent Event;
		float AdjustedMomentum = Momentum;
		Event.Damage = GetDamageParams(OtherActor, HitLocation, AdjustedMomentum).BaseDamage;
		Event.DamageTypeClass = ResolvedDamageType;
		Event.HitInfo = FHitResult(OtherActor, OtherComp, HitLocation, HitNormal);
		Event.ShotDirection = GetVelocity().SafeNormal();
		Event.Momentum = Event.ShotDirection * AdjustedMomentum;
		OtherActor->TakeDamage(Event.Damage, Event, ResolvedInstigator, this);
	}
}

void AUTProjectile::Explode_Implementation(const FVector& HitLocation, const FVector& HitNormal, UPrimitiveComponent* HitComp)
{
	if (!bExploded)
	{
		bExploded = true;
		if (!bFakeClientProjectile)
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
				UUTGameplayStatics::UTHurtRadius(this, AdjustedDamageParams.BaseDamage, AdjustedDamageParams.MinimumDamage, AdjustedMomentum, HitLocation, AdjustedDamageParams.InnerRadius, AdjustedDamageParams.OuterRadius, AdjustedDamageParams.DamageFalloff,
					MyDamageType, IgnoreActors, this, InstigatorController, FFInstigatorController, FFDamageType);
			}
			if (Role == ROLE_Authority)
			{
				bTearOff = true;
				bReplicateUTMovement = true; // so position of explosion is accurate even if flight path was a little off
			}
			if (ExplosionEffects != NULL)
			{
				ExplosionEffects.GetDefaultObject()->SpawnEffect(GetWorld(), FTransform(HitNormal.Rotation(), HitLocation), HitComp, this, InstigatorController);
			}
			// TODO: remove when no longer used
			else
			{
				UUTGameplayStatics::UTPlaySound(GetWorld(), ExplosionSound, this, ESoundReplicationType::SRT_IfSourceNotReplicated);
				if (GetNetMode() != NM_DedicatedServer)
				{
					UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionEffect, GetActorLocation(), HitNormal.Rotation(), true);
				}
			}
		}
		ShutDown();
	}
}

void AUTProjectile::Destroyed()
{
	if (MyFakeProjectile)
	{
		MyFakeProjectile->Destroy();
	}
	Super::Destroyed();
}

void AUTProjectile::ShutDown()
{
	if (MyFakeProjectile)
	{
		MyFakeProjectile->ShutDown();
	}
	if (!bPendingKillPending)
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
	}

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

float AUTProjectile::GetTimeToLocation(const FVector& TargetLoc) const
{
	const float Dist = (TargetLoc - GetActorLocation()).Size();
	if (ProjectileMovement == NULL)
	{
		UE_LOG(UT, Warning, TEXT("Unable to calculate time to location for %s; please implement GetTimeToLocation()"), *GetName());
		return 0.0f;
	}
	else
	{
		UUTProjectileMovementComponent* UTMovement = Cast<UUTProjectileMovementComponent>(ProjectileMovement);
		if (UTMovement == NULL || UTMovement->AccelRate == 0.0f)
		{
			return (Dist / ProjectileMovement->InitialSpeed);
		}
		else
		{

			// figure out how long it would take if we accelerated the whole way
			float ProjTime = (-UTMovement->InitialSpeed + FMath::Sqrt(FMath::Square<float>(UTMovement->InitialSpeed) - (2.0 * UTMovement->AccelRate * -Dist))) / UTMovement->AccelRate;
			// figure out how long it will actually take to accelerate to max speed
			float AccelTime = (UTMovement->MaxSpeed - UTMovement->InitialSpeed) / UTMovement->AccelRate;
			if (ProjTime > AccelTime)
			{
				// figure out distance traveled while accelerating to max speed
				const float AccelDist = (UTMovement->MaxSpeed * AccelTime) + (0.5 * UTMovement->AccelRate * FMath::Square<float>(AccelTime));
				// add time to accelerate to max speed plus time to travel remaining dist at max speed
				ProjTime = AccelTime + ((Dist - AccelDist) / UTMovement->MaxSpeed);
			}
			return ProjTime;
		}
	}
}

float AUTProjectile::GetMaxDamageRadius_Implementation() const
{
	return DamageParams.OuterRadius;
}