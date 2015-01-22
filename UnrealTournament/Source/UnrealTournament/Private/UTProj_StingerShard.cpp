// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProj_StingerShard.h"
#include "UnrealNetwork.h"
#include "UTImpactEffect.h"
#include "UTLift.h"
#include "UTProjectileMovementComponent.h"

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
	ImpactedShardDamage = 12;
	ImpactedShardMomentum = 100000.f;
}

void AUTProj_StingerShard::Destroyed()
{
	Explode(GetActorLocation(), ImpactNormal, CollisionComp);
	Super::Destroyed();
}

void AUTProj_StingerShard::ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	// FIXME: temporary? workaround for synchronization issues on clients where it explodes on client but not on server
	if (GetNetMode() == NM_Client && (Cast<APawn>(OtherActor) != NULL || Cast<AUTProjectile>(OtherActor) != NULL))
	{
		return;
	}

	APawn* HitPawn = Cast<APawn>(OtherActor);
	if (HitPawn || Cast<AUTProjectile>(OtherActor) != NULL)
	{
		if (HitPawn && ProjectileMovement->Velocity.IsZero())
		{
			// only provide momentum along plane
			FVector MomentumDir = OtherActor->GetActorLocation() - GetActorLocation();
			MomentumDir = (MomentumDir - (ImpactNormal | MomentumDir) * ImpactNormal).GetSafeNormal();
			MomentumDir.Z = 1.f;
			ProjectileMovement->Velocity = MomentumDir.GetSafeNormal();

			// clamp player z vel
			AUTCharacter* HitChar = Cast<AUTCharacter>(HitPawn);
			if (HitChar)
			{
				UUTCharacterMovement* UTMove = Cast<UUTCharacterMovement>(HitChar->GetCharacterMovement());
				if (UTMove)
				{
					float CurrentVelZ = UTMove->Velocity.Z;
					if (CurrentVelZ > -1.5f * UTMove->JumpZVelocity)
					{
						CurrentVelZ = FMath::Min(0.f, 3.f * (CurrentVelZ + UTMove->JumpZVelocity));
					}
					UTMove->Velocity.Z = CurrentVelZ;
					UTMove->ClearPendingImpulse();
					UTMove->NeedsClientAdjustment();
				}
			}
			
			Explode(GetActorLocation(), ImpactNormal, OtherComp);

			FUTPointDamageEvent Event;
			float AdjustedMomentum = ImpactedShardMomentum;
			Event.Damage = ImpactedShardDamage;
			Event.DamageTypeClass = MyDamageType;
			Event.HitInfo = FHitResult(HitPawn, OtherComp, HitLocation, HitNormal);
			Event.ShotDirection = ProjectileMovement->Velocity;
			Event.Momentum = Event.ShotDirection * AdjustedMomentum;
			HitPawn->TakeDamage(Event.Damage, Event, InstigatorController, this);
			return;
		}
		Super::ProcessHit_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);
		return;
	}

	//Stop the projectile and give it collision
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->Velocity = FVector::ZeroVector;
	ShardMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	bCanHitInstigator = true;

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
