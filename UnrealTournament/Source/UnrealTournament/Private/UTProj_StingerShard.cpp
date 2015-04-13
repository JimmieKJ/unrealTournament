// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	if (PawnOverlapSphere != NULL)
	{
		PawnOverlapSphere->SetRelativeLocation(FVector(-20.f, 0.f, 0.f));
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
	if (GetNetMode() == NM_Client && (Cast<APawn>(OtherActor) != NULL || Cast<AUTProjectile>(OtherActor) != NULL) && !OtherActor->bTearOff)
	{
		return;
	}

	APawn* HitPawn = Cast<APawn>(OtherActor);
	if (AttachedPawns.Contains(HitPawn))
	{
		return;
	}
	else if (HitPawn != NULL || Cast<AUTProjectile>(OtherActor) != NULL)
	{
		if (HitPawn != NULL)
		{
			if (ProjectileMovement->Velocity.IsZero())
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
			}
			else
			{
				AUTCharacter* HitChar = Cast<AUTCharacter>(HitPawn);
				if (HitChar != NULL && HitChar->IsDead() && HitChar->IsRagdoll())
				{
					AttachToRagdoll(HitChar, HitLocation);
				}
				else
				{
					Super::ProcessHit_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);
				}
			}
		}
		else
		{
			Super::ProcessHit_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);
		}
	}
	else if (!ShouldIgnoreHit(OtherActor, OtherComp))
	{
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
		if (Lift != NULL && Lift->GetEncroachComponent())
		{
			AttachRootComponentTo(Lift->GetEncroachComponent(), NAME_None, EAttachLocation::KeepWorldPosition);
		}
	}
}

void AUTProj_StingerShard::AttachToRagdoll(AUTCharacter* HitChar, const FVector& HitLocation)
{
	// create a physics constraint to the ragdoll to drag it and ultimately pin it
	const FBodyInstance* ClosestRagdollPart = NULL;
	float BestDist = FLT_MAX;
	for (const FBodyInstance* TestBody : HitChar->GetMesh()->Bodies)
	{
		if (TestBody->BodySetup.IsValid())
		{
			float TestDist = (TestBody->GetUnrealWorldTransform().GetLocation() - HitLocation).SizeSquared();
			if (TestDist < BestDist)
			{
				ClosestRagdollPart = TestBody;
				BestDist = TestDist;
			}
		}
	}
	if (ClosestRagdollPart != NULL)
	{
		UPhysicsConstraintComponent* NewConstraint = NewObject<UPhysicsConstraintComponent>(this);
		NewConstraint->OnComponentCreated();
		NewConstraint->SetWorldLocation(HitLocation); // note: important! won't work right if not in the proper location
		NewConstraint->RegisterComponent();
		NewConstraint->ConstraintInstance.bDisableCollision = true;
		NewConstraint->SetConstrainedComponents(HitChar->GetMesh(), ClosestRagdollPart->BodySetup.Get()->BoneName, Cast<UPrimitiveComponent>(ProjectileMovement->UpdatedComponent), NAME_None);
		NewConstraint->ConstraintInstance.ProjectionLinearTolerance = 0.05f;
		//NewConstraint->ConstraintInstance.EnableProjection();
	}
	AttachedPawns.Add(HitChar);
}

void AUTProj_StingerShard::Explode_Implementation(const FVector& HitLocation, const FVector& HitNormal, UPrimitiveComponent* HitComp)
{
	// if the impacted actor is a corpse (or became so as a result of our direct damage) then don't explode and keep going
	APawn* HitPawn = Cast<APawn>(ImpactedActor);
	if (HitPawn == NULL || ProjectileMovement->Velocity.IsZero())
	{
		Super::Explode_Implementation(HitLocation, HitNormal, HitComp);
	}
	else if (!AttachedPawns.Contains(HitPawn))
	{
		// note: dedicated servers don't create ragdolls, but they also turn off corpse collision immediately, so in effect the client's "pass through" behavior here actually matches the server, however unintuitive
		AUTCharacter* UTC = Cast<AUTCharacter>(HitPawn);
		if (UTC == NULL || !UTC->IsDead() || !UTC->IsRagdoll())
		{
			Super::Explode_Implementation(HitLocation, HitNormal, HitComp);
		}
		else
		{
			AttachToRagdoll(UTC, HitLocation);
		}
	}
}

float AUTProj_StingerShard::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser)
{
	Explode(GetActorLocation(), ImpactNormal, CollisionComp);
	return DamageAmount;
}
