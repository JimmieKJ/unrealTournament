// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTProj_WeaponScreen.h"
#include "UTImpactEffect.h"
#include "UTProj_TransDisk.h"

AUTProj_WeaponScreen::AUTProj_WeaponScreen(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	CollisionBox = ObjectInitializer.CreateDefaultSubobject<UBoxComponent>(this, FName(TEXT("CollisionBox")));
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
	Responses.EnumArray[COLLISION_TRACE_WEAPONNOCHARACTER] = ECollisionResponse::ECR_Block;
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
	bAlwaysShootable = true;
	FriendlyMomentumScaling = 1.f;
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
		OtherProj->ImpactedActor = this;
		if (Cast<AUTProj_TransDisk>(OtherProj))
		{
			if (((AUTProj_TransDisk *)(OtherProj))->bCanShieldBounce)
			{
				((AUTProj_TransDisk *)(OtherProj))->bCanShieldBounce = false;
				OtherProj->ProjectileMovement->Velocity = OtherProj->ProjectileMovement->Velocity - 2.f * (OtherProj->ProjectileMovement->Velocity | HitNormal) * HitNormal;
			}
		}
		else
		{
			OtherProj->Explode(OtherProj->GetActorLocation(), -HitNormal);
		}
	}
	else if (bCauseMomentumToPawns && Cast<APawn>(OtherActor) != NULL && OtherActor != Instigator && !HitPawns.Contains((APawn*)OtherActor))
	{
		FCollisionQueryParams Params(FName(TEXT("WeaponScreenOverlap")), false, this);
		Params.AddIgnoredActor(OtherActor);
		if (!GetWorld()->LineTraceTestByChannel(HitLocation, OtherActor->GetActorLocation(), ECC_Visibility, Params))
		{
			HitPawns.Add((APawn*)OtherActor);
			FVector MomentumDir = GetVelocity().GetSafeNormal();
			if (MomentumDir.IsZero())
			{
				MomentumDir = GetActorRotation().Vector();
			}
			const float Radius = FMath::Max<float>(1.0f, CollisionBox->Bounds.SphereRadius);
			const float OtherDist = FMath::PointDistToLine(OtherComp->GetComponentLocation(), GetVelocity().GetSafeNormal(), GetActorLocation() - GetVelocity());
			float MomentumScale = (Momentum * (1.0f - FMath::Clamp<float>(OtherDist / Radius, 0.0f, 0.8f)));
			AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
			if (GS != NULL && GS->OnSameTeam(OtherActor, InstigatorController))
			{
				MomentumScale *= FriendlyMomentumScaling;
			}

			FUTPointDamageEvent DmgEvent(0.0f, FHitResult(OtherActor, OtherComp, HitLocation, HitNormal), MomentumDir, MyDamageType, MomentumDir * MomentumScale);
			OtherActor->TakeDamage(0.0f, DmgEvent, InstigatorController, this);
		}
	}
	// this projectile doesn't blow up from collisions
}

void AUTProj_WeaponScreen::OnStop(const FHitResult& Hit)
{
	Super::OnStop(Hit);

	if (Role == ROLE_Authority)
	{
		bTearOff = true;
		bReplicateUTMovement = true; // so position of explosion is accurate even if flight path was a little off
	}
	if (ExplosionEffects != NULL)
	{
		ExplosionEffects.GetDefaultObject()->SpawnEffect(GetWorld(), FTransform(Hit.Normal.Rotation(), Hit.Location), Hit.Component.Get(), this, InstigatorController);
	}
	ShutDown();
}

void AUTProj_WeaponScreen::TornOff()
{
	if (ExplosionEffects != NULL)
	{
		ExplosionEffects.GetDefaultObject()->SpawnEffect(GetWorld(), FTransform((-GetVelocity()).Rotation(), GetActorLocation()), NULL, this, InstigatorController);
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
