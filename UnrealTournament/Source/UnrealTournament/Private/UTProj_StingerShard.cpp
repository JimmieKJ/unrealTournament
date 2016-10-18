// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProj_StingerShard.h"
#include "UnrealNetwork.h"
#include "UTImpactEffect.h"
#include "UTLift.h"
#include "UTProjectileMovementComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

AUTProj_StingerShard::AUTProj_StingerShard(const class FObjectInitializer& ObjectInitializer)
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
	InitialLifeSpan = 3.f;
	ImpactedShardDamage = 12;
	ImpactedShardMomentum = 50000.f;
	bLowPriorityLight = true;
	bNetTemporary = true;
}

void AUTProj_StingerShard::Destroyed()
{
	// detonate shards embedded in walls when they time out
	if (!ImpactNormal.IsZero())
	{
		Explode(GetActorLocation(), ImpactNormal, CollisionComp);
	}
	Super::Destroyed();
}

void AUTProj_StingerShard::ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	// FIXME: temporary? workaround for synchronization issues on clients where it explodes on client but not on server
	if (GetNetMode() == NM_Client && (Cast<APawn>(OtherActor) != NULL || Cast<AUTProjectile>(OtherActor) != NULL) && !OtherActor->bTearOff)
	{
		return;
	}

	AUTCharacter* HitChar = Cast<AUTCharacter>(OtherActor);
	if (HitChar != NULL || Cast<AUTProjectile>(OtherActor) != NULL || (OtherActor && OtherActor->bCanBeDamaged))
	{
		if (HitChar != NULL)
		{
			if (ProjectileMovement->Velocity.IsZero())
			{
				// only provide momentum along plane
				FVector MomentumDir = OtherActor->GetActorLocation() - GetActorLocation();
				MomentumDir = (MomentumDir - (ImpactNormal | MomentumDir) * ImpactNormal).GetSafeNormal();
				MomentumDir.Z = 1.f;
				ProjectileMovement->Velocity = MomentumDir.GetSafeNormal();

				// clamp player z vel
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

				Explode(GetActorLocation(), ImpactNormal, OtherComp);

				FUTPointDamageEvent Event;
				float AdjustedMomentum = ImpactedShardMomentum;
				Event.Damage = ImpactedShardDamage;
				Event.DamageTypeClass = MyDamageType;
				Event.HitInfo = FHitResult(HitChar, OtherComp, HitLocation, HitNormal);
				Event.ShotDirection = ProjectileMovement->Velocity;
				Event.Momentum = Event.ShotDirection * AdjustedMomentum;
				HitChar->TakeDamage(Event.Damage, Event, InstigatorController, this);
			}
			else
			{
				if (HitChar->IsDead())
				{
					Momentum = 20000.f;
				}
				Super::ProcessHit_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);
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
		SetLifeSpan(0.9f);
		if (BounceSound != NULL)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), BounceSound, this, SRT_IfSourceNotReplicated, false, FVector::ZeroVector, NULL, NULL, true, SAT_WeaponFoley);
		}

		SetActorLocation(HitLocation + HitNormal);
		ImpactNormal = HitNormal;

		AUTLift* Lift = Cast<AUTLift>(OtherActor);
		if (Lift != NULL && Lift->GetEncroachComponent())
		{
			AttachToComponent(Lift->GetEncroachComponent(), FAttachmentTransformRules::KeepWorldTransform);
		}

		// turn off in-flight sound and particle system
		TArray<UAudioComponent*> AudioComponents;
		GetComponents<UAudioComponent>(AudioComponents);
		for (int32 i = 0; i < AudioComponents.Num(); i++)
		{
			if (AudioComponents[i] && AudioComponents[i]->Sound != NULL && AudioComponents[i]->Sound->GetDuration() >= INDEFINITELY_LOOPING_DURATION)
			{
				AudioComponents[i]->Stop();
			}
		}
		TArray<UParticleSystemComponent*> ParticleComponents;
		GetComponents<UParticleSystemComponent>(ParticleComponents);
		for (int32 i = 0; i < ParticleComponents.Num(); i++)
		{
			if (ParticleComponents[i])
			{
				UParticleSystem* SavedTemplate = ParticleComponents[i]->Template;
				ParticleComponents[i]->DeactivateSystem();
				ParticleComponents[i]->KillParticlesForced();
				// FIXME: KillParticlesForced() doesn't kill particles immediately for GPU particles, but the below does...
				ParticleComponents[i]->SetTemplate(NULL);
				ParticleComponents[i]->SetTemplate(SavedTemplate);
			}
		}
	}
}

float AUTProj_StingerShard::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser)
{
	Explode(GetActorLocation(), ImpactNormal, CollisionComp);
	return DamageAmount;
}
