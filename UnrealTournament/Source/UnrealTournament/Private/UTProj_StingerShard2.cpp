// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTProj_StingerShard2.h"
#include "UnrealNetwork.h"
#include "UTImpactEffect.h"
#include "UTLift.h"
#include "UTProjectileMovementComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

AUTProj_StingerShard2::AUTProj_StingerShard2(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ShardMesh = ObjectInitializer.CreateOptionalDefaultSubobject<UStaticMeshComponent>(this, FName(TEXT("Mesh")));
	if (ShardMesh != NULL)
	{
		ShardMesh->SetCollisionProfileName(FName(TEXT("NoCollision")));
		ShardMesh->SetupAttachment(RootComponent);
	}
	if (PawnOverlapSphere != NULL)
	{
		PawnOverlapSphere->SetRelativeLocation(FVector(-10.f, 0.f, 0.f));
	}
	InitialLifeSpan = 5.0f;
	bLowPriorityLight = true;
	bNetTemporary = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	DirectHitDamage = 25;
	DirectHitMomentum = 75000.0f;
	OverlapRadius = 1.0f;
	OverlapSphereGrowthRate = 800.0f;
	MaxOverlapSphereSize = 180.0f;
}

void AUTProj_StingerShard2::DamageImpactedActor_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	const FRadialDamageParams SavedParams = DamageParams;
	const float SavedMomentum = Momentum;
	
	DamageParams.InnerRadius = DamageParams.OuterRadius = 0.0f;
	DamageParams.BaseDamage = DirectHitDamage;
	Momentum = DirectHitMomentum;
	Super::DamageImpactedActor_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);

	DamageParams = SavedParams;
	Momentum = SavedMomentum;
}

void AUTProj_StingerShard2::OnPawnSphereOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* P = Cast<APawn>(OtherActor);
	if (P != nullptr && (P != Instigator || bCanHitInstigator))
	{
		PotentialTargets.Add(P);
	}
}

void AUTProj_StingerShard2::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (PawnOverlapSphere != nullptr && PawnOverlapSphere->GetUnscaledSphereRadius() < MaxOverlapSphereSize)
	{
		PawnOverlapSphere->SetSphereRadius(FMath::Min<float>(MaxOverlapSphereSize, PawnOverlapSphere->GetUnscaledSphereRadius() + OverlapSphereGrowthRate * DeltaTime));
	}
	if (!ProjectileMovement->Velocity.IsZero())
	{
		const FVector MovementDir = ProjectileMovement->Velocity.GetUnsafeNormal();
		const FVector MyLoc = GetActorLocation();
		for (APawn* P : PotentialTargets)
		{
			if (P != nullptr && !P->bTearOff && ((P->GetActorLocation() - MyLoc).GetSafeNormal() | MovementDir) <= 0.0f && !GetWorld()->LineTraceTestByChannel(MyLoc, P->GetActorLocation(), COLLISION_TRACE_WEAPON, FCollisionQueryParams(NAME_None, true, P)))
			{
				Explode(GetActorLocation(), -MovementDir);
				break;
			}
		}
	}
}