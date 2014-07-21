

#include "UnrealTournament.h"
#include "UTProj_TransDisk.h"
#include "UTWeap_Translocator.h"
#include "Particles/ParticleSystemComponent.h"
#include "UnrealNetwork.h"

AUTProj_TransDisk::AUTProj_TransDisk(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	//bLanded = false;
	//bDisrupted = false;
	TransState = TLS_InAir;

	CollisionComp->SetCollisionProfileName("ProjectileShootable");
}

float AUTProj_TransDisk::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser)
{
	if (EventInstigator != NULL && TransState != TLS_Disrupted)
	{
		AUTCharacter* UTP = Cast<AUTCharacter>(EventInstigator->GetPawn());
		AUTCharacter* UTOwner = Cast<AUTCharacter>(Instigator);
		if (UTP != NULL && UTOwner != NULL && (UTP->GetTeamNum() == 255 || UTP->GetTeamNum() != UTOwner->GetTeamNum()))
		{
			TransState = TLS_Disrupted;
			//Play and deactivate effects
			OnDisrupted();
		}
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
				LC->SetBrightness(0.0F);
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

/**Coppied from UProjectileMovementComponent*/
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


void AUTProj_TransDisk::ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	if (OtherActor == Instigator)
	{
		//Pickup Disk
		if (TransState == TLS_OnGround)
		{
			ShutDown();
		}
	}
	else if (Cast<AUTProjectile>(OtherActor))
	{

	}
	else if (TransState == TLS_InAir)
	{
		FHitResult Hit(OtherActor, OtherComp, HitLocation, HitNormal);
		ProjectileMovement->Velocity = ComputeBounceResult(Hit, 0.0f, FVector::ZeroVector);
		OnBounce(Hit, ProjectileMovement->Velocity);
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
}

void AUTProj_TransDisk::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTProj_TransDisk, MyTranslocator, COND_InitialOnly);

	//TODO: Do rep on only visible so players cant hack and see if it was disrupted
	DOREPLIFETIME_CONDITION(AUTProj_TransDisk, TransState, COND_None);
}