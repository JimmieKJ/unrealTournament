// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTProj_WeaponScreen.h"

AUTProj_WeaponScreen::AUTProj_WeaponScreen(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	CollisionBox = PCIP.CreateDefaultSubobject<UBoxComponent>(this, FName(TEXT("CollisionBox")));
	CollisionBox->SetBoxExtent(FVector(5.0f, 50.0f, 50.0f), false);
	CollisionBox->SetCollisionProfileName(TEXT("ProjectileShootable"));
	FCollisionResponseContainer Responses = CollisionBox->GetCollisionResponseToChannels();
	for (int32 i = 0; i < ARRAY_COUNT(Responses.EnumArray); i++)
	{
		if (Responses.EnumArray[i] == ECollisionResponse::ECR_Block)
		{
			Responses.EnumArray[i] = ECollisionResponse::ECR_Overlap;
		}
	}
	Responses.EnumArray[COLLISION_TRACE_WEAPON] = ECollisionResponse::ECR_Block;
	Responses.WorldStatic = ECollisionResponse::ECR_Ignore;
	Responses.WorldDynamic = ECollisionResponse::ECR_Ignore;
	// note that we leave the default collision sphere around to use as a world blocker (so we're blocked if center hits world)
	CollisionBox->AttachParent = CollisionComp;

	InitialLifeSpan = 0.5f;
	ProjectileMovement->InitialSpeed = 5000.0f;
	ProjectileMovement->MaxSpeed = 5000.0f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ScaleRate = FVector(0.0f, 10.0f, 10.0f);

	DamageParams.BaseDamage = 0;
	Momentum = 0.0f;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

float AUTProj_WeaponScreen::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	AUTProjectile* OtherProj = Cast<AUTProjectile>(DamageCauser);
	if (OtherProj != NULL)
	{
		OtherProj->FFInstigatorController = InstigatorController;
		OtherProj->FFDamageType = BlockedProjDamageType;
	}
	return 0.f;
}
void AUTProj_WeaponScreen::ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	AUTProjectile* OtherProj = Cast<AUTProjectile>(OtherActor);
	if (OtherProj != NULL)
	{
		OtherProj->FFInstigatorController = InstigatorController;
		OtherProj->FFDamageType = BlockedProjDamageType;
		OtherProj->Explode(OtherProj->GetActorLocation(), -HitNormal);
	}

	// this projectile doesn't blow up from collisions
}

void AUTProj_WeaponScreen::OnStop(const FHitResult& Hit)
{
	Super::OnStop(Hit);

	if (Role == ROLE_Authority)
	{
		bTearOff = true;
		bReplicateMovement = true; // so position of explosion is accurate even if flight path was a little off
	}
	UUTGameplayStatics::UTPlaySound(GetWorld(), ExplosionSound, this, ESoundReplicationType::SRT_IfSourceNotReplicated);
	if (GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionEffect, GetActorLocation(), Hit.Normal.Rotation(), true);
	}
	ShutDown();
}

void AUTProj_WeaponScreen::Tick(float DeltaTime)
{
	if (RootComponent != NULL)
	{
		RootComponent->SetWorldScale3D(RootComponent->GetComponentScale() + ScaleRate * DeltaTime);
	}

	Super::Tick(DeltaTime);
}
