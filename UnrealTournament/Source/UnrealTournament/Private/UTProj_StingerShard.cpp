// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProj_StingerShard.h"
#include "UnrealNetwork.h"
#include "UTImpactEffect.h"
#include "UTLift.h"
#include "UTProjectileMovementComponent.h"

static const float GOO_TIMER_TICK = 0.5f;

AUTProj_StingerShard::AUTProj_StingerShard(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ShardMesh = ObjectInitializer.CreateOptionalDefaultSubobject<UStaticMeshComponent>(this, FName(TEXT("Mesh")));
	if (ShardMesh != NULL)
	{
		ShardMesh->SetCollisionProfileName(FName(TEXT("NoCollision")));
		ShardMesh->AttachParent = RootComponent;
	}
	InitialLifeSpan = 8.f;
	JumpOffDamage = 12;
}

void AUTProj_StingerShard::Destroyed()
{
	Explode(GetActorLocation(), ImpactNormal, CollisionComp);
	Super::Destroyed();
}

void AUTProj_StingerShard::JumpedOffBy(AUTCharacter* BasedCharacter)
{
	Explode(GetActorLocation(), ImpactNormal, CollisionComp);

	FUTPointDamageEvent Event;
	float AdjustedMomentum = 0.f;
	Event.Damage = JumpOffDamage;
	Event.DamageTypeClass = MyDamageType;
	Event.HitInfo = FHitResult(BasedCharacter, BasedCharacter->GetCapsuleComponent(), GetActorLocation(), FVector(0.f, 0.f, 1.f));
	Event.ShotDirection = FVector(0.f, 0.f, 1.f);
	Event.Momentum = FVector(0.f);
	BasedCharacter->TakeDamage(Event.Damage, Event, BasedCharacter->GetController(), this);
}

/**Overridden to do the landing*/
void AUTProj_StingerShard::ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	// FIXME: temporary? workaround for synchronization issues on clients where it explodes on client but not on server
	if (GetNetMode() == NM_Client && (Cast<APawn>(OtherActor) != NULL || Cast<AUTProjectile>(OtherActor) != NULL))
	{
		return;
	}

	if (Cast<APawn>(OtherActor) != NULL || Cast<AUTProjectile>(OtherActor) != NULL)
	{
		Super::ProcessHit_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);
		return;
	}

	//Stop the projectile and give it collision
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->Velocity = FVector::ZeroVector;
	ShardMesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);

	if (Role == ROLE_Authority)
	{
		bReplicateUTMovement = true;
	}

	SetActorLocation(HitLocation + HitNormal);
	ImpactNormal = HitNormal;

	AUTLift* Lift = Cast<AUTLift>(OtherActor);
	if (Lift && Lift->GetEncroachComponent())
	{
		AttachRootComponentTo(Lift->GetEncroachComponent(), NAME_None, EAttachLocation::KeepWorldPosition);
	}
}

float AUTProj_StingerShard::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser)
{
	Explode(GetActorLocation(), ImpactNormal, CollisionComp);
	return DamageAmount;
}
