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
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AUTProjectile::OnOverlapBegin);
	CollisionComp->bTraceComplexOnMove = true;
	RootComponent = CollisionComp;

	// Use a ProjectileMovementComponent to govern this projectile's movement
	ProjectileMovement = PCIP.CreateDefaultSubobject<UProjectileMovementComponent>(this, TEXT("ProjectileComp"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 3000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &AUTProjectile::OnStop);

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

void AUTProjectile::ProcessHit(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	// note: on clients we assume spawn time impact is invalid since in such a case the projectile would generally have not survived to be replicated at all
	if (OtherActor != this && (OtherActor != Instigator || Instigator == NULL) && OtherComp != NULL && !bExploded && (Role == ROLE_Authority || CreationTime != GetWorld()->TimeSeconds))
	{
		// TODO: replicated momentum handling
		if (OtherComp != NULL)
		{
			OtherComp->AddImpulseAtLocation(GetVelocity() * 100.0f, GetActorLocation());
		}

		if (OtherActor != NULL)
		{
			FRadialDamageEvent Event;
			Event.Params = DamageParams;
			Event.Params.MinimumDamage = DamageParams.BaseDamage; // force full damage for direct hit
			Event.DamageTypeClass = MyDamageType;
			Event.Origin = HitLocation;
			OtherActor->TakeDamage(DamageParams.BaseDamage, Event, InstigatorController, this);
		}

		ImpactedActor = OtherActor;
		Explode(HitLocation, HitNormal);
		ImpactedActor = NULL;
	}
}

// TEMPORARY PARTIAL C/P of UGameplayStatics::ApplyRadialDamageWithFalloff() to work around its current requirement that targets block visibility traces
static bool ComponentIsVisibleFrom(UPrimitiveComponent* VictimComp, FVector const& Origin, AActor const* IgnoredActor, const TArray<AActor*>& IgnoreActors, FHitResult& OutHitResult)
{
	static FName NAME_ComponentIsVisibleFrom = FName(TEXT("ComponentIsVisibleFrom"));
	FCollisionQueryParams LineParams(NAME_ComponentIsVisibleFrom, true, IgnoredActor);
	LineParams.AddIgnoredActors(IgnoreActors);

	// Do a trace from origin to middle of box
	UWorld* World = VictimComp->GetWorld();
	check(World);

	FVector const TraceEnd = VictimComp->Bounds.Origin;
	FVector TraceStart = Origin;
	if (Origin == TraceEnd)
	{
		// tiny nudge so LineTraceSingle doesn't early out with no hits
		TraceStart.Z += 0.01f;
	}
	bool const bHadBlockingHit = World->LineTraceSingle(OutHitResult, TraceStart, TraceEnd, ECC_Visibility, LineParams);
	//::DrawDebugLine(World, TraceStart, TraceEnd, FLinearColor::Red, true);

	// If there was a blocking hit, it will be the last one
	if (bHadBlockingHit)
	{
		if (OutHitResult.Component == VictimComp)
		{
			// if blocking hit was the victim component, it is visible
			return true;
		}
		else
		{
			// if we hit something else blocking, it's not
			UE_LOG(LogDamage, Log, TEXT("Radial Damage to %s blocked by %s (%s)"), *GetNameSafe(VictimComp), *GetNameSafe(OutHitResult.GetActor()), *GetNameSafe(OutHitResult.Component.Get()));
			return false;
		}
	}

	// didn't hit anything, including the victim component. ASSUME VISIBLE
	VictimComp->LineTraceComponent(OutHitResult, TraceStart, TraceEnd, LineParams);
	return true;
}
static bool ApplyRadialDamageWithFalloff(UObject* WorldContextObject, float BaseDamage, float MinimumDamage, const FVector& Origin, float DamageInnerRadius, float DamageOuterRadius, float DamageFalloff, TSubclassOf<class UDamageType> DamageTypeClass, const TArray<AActor*>& IgnoreActors, AActor* DamageCauser, AController* InstigatedByController)
{
	static FName NAME_ApplyRadialDamage = FName(TEXT("ApplyRadialDamage"));
	FCollisionQueryParams SphereParams(NAME_ApplyRadialDamage, false, DamageCauser);

	SphereParams.AddIgnoredActors(IgnoreActors);

	// query scene to see what we hit
	TArray<FOverlapResult> Overlaps;
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	World->OverlapMulti(Overlaps, Origin, FQuat::Identity, FCollisionShape::MakeSphere(DamageOuterRadius), SphereParams, FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllDynamicObjects));

	// collate into per-actor list of hit components
	TMap<AActor*, TArray<FHitResult> > OverlapComponentMap;
	for (int32 Idx = 0; Idx < Overlaps.Num(); ++Idx)
	{
		FOverlapResult const& Overlap = Overlaps[Idx];
		AActor* const OverlapActor = Overlap.GetActor();

		if (OverlapActor &&
			OverlapActor->bCanBeDamaged &&
			OverlapActor != DamageCauser &&
			Overlap.Component.IsValid())
		{
			FHitResult Hit;
			if (ComponentIsVisibleFrom(Overlap.Component.Get(), Origin, DamageCauser, IgnoreActors, Hit))
			{
				TArray<FHitResult>& HitList = OverlapComponentMap.FindOrAdd(OverlapActor);
				HitList.Add(Hit);
			}
		}
	}

	// make sure we have a good damage type
	TSubclassOf<UDamageType> const ValidDamageTypeClass = (DamageTypeClass == NULL) ? TSubclassOf<UDamageType>(UDamageType::StaticClass()) : DamageTypeClass;

	bool bAppliedDamage = false;

	// call damage function on each affected actors
	for (TMap<AActor*, TArray<FHitResult> >::TIterator It(OverlapComponentMap); It; ++It)
	{
		AActor* const Victim = It.Key();
		TArray<FHitResult> const& ComponentHits = It.Value();

		FRadialDamageEvent DmgEvent;
		DmgEvent.DamageTypeClass = ValidDamageTypeClass;
		DmgEvent.ComponentHits = ComponentHits;
		DmgEvent.Origin = Origin;
		DmgEvent.Params = FRadialDamageParams(BaseDamage, MinimumDamage, DamageInnerRadius, DamageOuterRadius, DamageFalloff);

		Victim->TakeDamage(BaseDamage, DmgEvent, InstigatedByController, DamageCauser);

		bAppliedDamage = true;
	}

	return bAppliedDamage;
}

void AUTProjectile::Explode(const FVector& HitLocation, const FVector& HitNormal)
{
	if (!bExploded)
	{
		TArray<AActor*> IgnoreActors;
		IgnoreActors.Add(this);
		if (ImpactedActor != NULL)
		{
			IgnoreActors.Add(ImpactedActor);
		}
		ApplyRadialDamageWithFalloff(this, DamageParams.BaseDamage, DamageParams.MinimumDamage, HitLocation, DamageParams.InnerRadius, DamageParams.OuterRadius, DamageParams.DamageFalloff, MyDamageType, IgnoreActors, this, InstigatorController);
		bTearOff = true;
		bExploded = true;
		if (GetNetMode() != NM_DedicatedServer)
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionEffect, GetActorLocation(), HitNormal.Rotation(), true);
		}
		ShutDown();
	}
}

void AUTProjectile::ShutDown()
{
	SetLifeSpan(0.2f);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMovement->SetActive(false);
	SetActorHiddenInGame(true);
	bExploded = true;
	// TODO: remove effects, sounds
}

void AUTProjectile::TornOff()
{
	Explode(GetActorLocation(), FVector(0.0f, 0.0f, 1.0f));
}