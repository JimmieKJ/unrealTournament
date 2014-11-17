

#include "UnrealTournament.h"
#include "UTProj_TransDisk.h"
#include "UTWeap_Translocator.h"
#include "Particles/ParticleSystemComponent.h"
#include "UnrealNetwork.h"
#include "UTDamageType.h"
#include "UTLift.h"

AUTProj_TransDisk::AUTProj_TransDisk(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicateUTMovement = true;
	TransState = TLS_InAir;
	CollisionComp->SetCollisionProfileName("ProjectileShootable");
	bAlwaysShootable = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	ProjectileMovement->Buoyancy = 0.5f;
	MaxSpeedUnderWater = 1300.f;
}

void AUTProj_TransDisk::BeginFakeProjectileSynch(AUTProjectile* InFakeProjectile)
{
	InFakeProjectile->Destroy();
}

void AUTProj_TransDisk::InitFakeProjectile(AUTPlayerController* OwningPlayer)
{
	Super::InitFakeProjectile(OwningPlayer);
	TArray<UPrimitiveComponent*> Components;
	GetComponents<UPrimitiveComponent>(Components);
	for (int32 i = 0; i < Components.Num(); i++)
	{
		if (Components[i] != CollisionComp)
		{
			Components[i]->SetCollisionResponseToAllChannels(ECR_Ignore);
		}
	}
}

float AUTProj_TransDisk::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser)
{
	if (Role == ROLE_Authority && EventInstigator != NULL && TransState != TLS_Disrupted)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS == NULL || !GS->OnSameTeam(EventInstigator, Instigator))
		{
			TransState = TLS_Disrupted;
			DisruptedController = EventInstigator;
			//Play and deactivate effects
			OnDisrupted();
		}
	}
	FVector DamageImpulse = 0.01f * UTGetDamageMomentum(DamageEvent, this, EventInstigator);
	if (DamageImpulse.SizeSquared() > 2.f)
	{
		DamageImpulse.Z = FMath::Abs(DamageImpulse.Z);
		ProjectileMovement->Velocity += DamageImpulse;
		ProjectileMovement->SetUpdatedComponent(CollisionComp);
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AUTProj_TransDisk::OnRep_TransState()
{
	if (TransState == TLS_Disrupted)
	{
		OnDisrupted();
	}
	else if (TransState == TLS_OnGround)
	{
		OnLanded();
	}
}

void AUTProj_TransDisk::OnLanded_Implementation()
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		//Deactivate any flight particles
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
			}
		}

		//Spawn the landing effect
		UParticleSystemComponent* Particle = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), LandedEffect, GetActorLocation(), FRotator::ZeroRotator, true);

		if (Cast<AUTCharacter>(Instigator) != NULL)
		{
			static FName NAME_TeamColor(TEXT("TeamColor"));
			Particle->SetColorParameter(NAME_TeamColor, Cast<AUTCharacter>(Instigator)->GetTeamColor());
		}
	}
}

void AUTProj_TransDisk::OnDisrupted_Implementation()
{
	if (TransState == TLS_Disrupted && GetNetMode() != NM_DedicatedServer)
	{
		//Deactivate particles and lights 
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
			}

			ULightComponent* LC = Cast<ULightComponent>(Components[i]);
			if (LC != NULL)
			{
				LC->SetIntensity(0.0F);
			}
		}
	}
}

void AUTProj_TransDisk::ShutDown()
{
	if (MyTranslocator != NULL)
	{
		MyTranslocator->ClearDisk();
	}
	Super::ShutDown();
}

/**Copied from UProjectileMovementComponent*/
FVector AUTProj_TransDisk::ComputeBounceResult(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	FVector TempVelocity = ProjectileMovement->Velocity;

	// Project velocity onto normal in reflected direction.
	const FVector ProjectedNormal = Hit.Normal * -(TempVelocity | Hit.Normal);

	// Point velocity in direction parallel to surface
	TempVelocity += ProjectedNormal;

	// Only tangential velocity should be affected by friction.
	TempVelocity *= FMath::Clamp(1.f - ProjectileMovement->Friction, 0.f, 1.f);

	// Coefficient of restitution only applies perpendicular to impact.
	TempVelocity += (ProjectedNormal * FMath::Max(ProjectileMovement->Bounciness, 0.f));

	// Bounciness or Friction > 1 could cause us to exceed velocity.
	if (ProjectileMovement->GetMaxSpeed() > 0.f)
	{
		TempVelocity = TempVelocity.ClampMaxSize(ProjectileMovement->GetMaxSpeed());
	}

	return TempVelocity;
}

void AUTProj_TransDisk::Recall()
{
	AUTCharacter* UTInstigator = Cast<AUTCharacter>(Instigator);
	if (UTInstigator && MyTranslocator && UTInstigator->GetWeapon() == MyTranslocator)
	{
		MyTranslocator->RecallDisk();
	}
	else
	{
		ShutDown();
	}
}

void AUTProj_TransDisk::ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	if (OtherActor == Instigator)
	{
		//Pick up Disk
		if (TransState == TLS_OnGround)
		{
			Recall();
		}
	}
	else if (Cast<AUTProjectile>(OtherActor))
	{

	}
	else if (TransState == TLS_InAir)
	{
		APhysicsVolume* WaterVolume = Cast<APhysicsVolume>(OtherActor);
		if (WaterVolume && WaterVolume->bWaterVolume)
		{
			ProjectileMovement->MaxSpeed = MaxSpeedUnderWater;
		}
		else if (Cast<AVolume>(OtherActor) == NULL)
		{
			FHitResult Hit(OtherActor, OtherComp, HitLocation, HitNormal);
			ProjectileMovement->Velocity = ComputeBounceResult(Hit, 0.0f, FVector::ZeroVector);
			OnBounce(Hit, ProjectileMovement->Velocity);
			DetachRootComponentFromParent(true);

			// if bot, check for telefrag
			if (Cast<AUTCharacter>(OtherActor) != NULL)
			{
				AUTBot* B = Cast<AUTBot>(InstigatorController);
				if (B != NULL && B->WeaponProficiencyCheck() && FMath::FRand() < 0.1f * (B->Skill + B->Personality.Alertness + B->Personality.Tactics + B->Personality.ReactionTime))
				{
					AUTCharacter* UTC = Cast<AUTCharacter>(Instigator);
					if (UTC != NULL && UTC->GetWeapon() == MyTranslocator && MyTranslocator->TransDisk == this)
					{
						UTC->StartFire(1);
						UTC->StopFire(1);
						B->ClearFocus(SCRIPTEDMOVE_FOCUS_PRIORITY);
						B->MoveTimer = -1.0f;
						B->LastTranslocTime = GetWorld()->TimeSeconds;
					}
				}
			}
		}
	}
}

void AUTProj_TransDisk::OnStop(const FHitResult& Hit)
{
	if (TransState != TLS_Disrupted)
	{
		TransState = TLS_OnGround;

		//Play Effects
		OnLanded();
	}
	//Fix the rotation
	FRotator NewRot = GetActorRotation();
	NewRot.Pitch = 0.0f;
	NewRot.Roll = 0.0f;
	SetActorRotation(NewRot);

	if (DiskMesh)
	{
		FRotator DiskRot = (Hit.Normal).Rotation();
		DiskRot.Pitch += 90.f;
		DiskMesh->SetRelativeRotation(DiskRot - NewRot);
	}
	AUTLift* Lift = Cast<AUTLift>(Hit.Actor.Get());
	if (Lift && Lift->GetEncroachComponent())
	{
		AttachRootComponentTo(Lift->GetEncroachComponent(), NAME_None, EAttachLocation::KeepWorldPosition);
	}
}

void AUTProj_TransDisk::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTProj_TransDisk, MyTranslocator, COND_InitialOnly);

	//TODO: Do rep on only visible so players cant hack and see if it was disrupted
	DOREPLIFETIME_CONDITION(AUTProj_TransDisk, TransState, COND_None);
}

bool AUTProj_TransDisk::IsAcceptableTranslocationTo(const FVector& DesiredDest)
{
	FCollisionQueryParams Params(FName(TEXT("TransDiskAI")), false, this);
	Params.AddIgnoredActor(Instigator);
	if (GetWorld()->LineTraceTest(GetActorLocation(), DesiredDest, ECC_Pawn, Params) || !GetWorld()->LineTraceTest(GetActorLocation(), GetActorLocation() - FVector(0.0f, 0.0f, 500.0f), ECC_Pawn, Params))
	{
		return false;
	}
	else
	{
		// if Instigator is moving, its velocity will be maintained, so check for ground in that direction as well
		FVector InstigatorVel = Instigator->GetVelocity();
		InstigatorVel.Z = 0.0f;
		return (InstigatorVel.IsNearlyZero() || GetWorld()->LineTraceTest(GetActorLocation(), GetActorLocation() - FVector(0.0f, 0.0f, 500.0f) + InstigatorVel * 0.25f, ECC_Pawn, Params));
	}
}

void AUTProj_TransDisk::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// AI interface
	AUTCharacter* UTC = Cast<AUTCharacter>(Instigator);
	if (Role == ROLE_Authority && MyTranslocator != NULL && UTC != NULL && UTC->GetWeapon() == MyTranslocator && MyTranslocator->TransDisk == this && !MyTranslocator->IsFiring())
	{
		AUTBot* B = Cast<AUTBot>(InstigatorController);
		if (B != NULL)
		{
			switch (B->ShouldTriggerTranslocation(GetActorLocation(), GetVelocity()))
			{
				case BMS_Activate:
					UTC->StartFire(1);
					UTC->StopFire(1);
					B->ClearFocus(SCRIPTEDMOVE_FOCUS_PRIORITY);
					B->TranslocTarget = FVector::ZeroVector;
					B->MoveTimer = -1.0f;
					B->LastTranslocTime = GetWorld()->TimeSeconds;
					break;
				case BMS_Abort:
					UTC->StartFire(0);
					UTC->StopFire(0);
					break;
				default:
					break;
			}
		}
	}
}