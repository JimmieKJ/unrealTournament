// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_Sniper.h"
#include "UTProj_Sniper.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"
#include "UTWeaponStateZooming.h"
#include "Particles/ParticleSystemComponent.h"

AUTWeap_Sniper::AUTWeap_Sniper(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP.SetDefaultSubobjectClass<UUTWeaponStateZooming>(TEXT("FiringState1")) )
{
	BringUpTime = 0.54f;
	PutDownTime = 0.41f;
	SlowHeadshotScale = 1.6f;
	RunningHeadshotScale = 0.0f;
	HeadshotDamageMult = 2.0f;
	ProjClass.Insert(AUTProj_Sniper::StaticClass(), 0);
	if (FiringState.Num() > 1)
	{
#if WITH_EDITORONLY_DATA
		FiringStateType[1] = UUTWeaponStateZooming::StaticClass();
#endif
	}

	HUDIcon = MakeCanvasIcon(HUDIcon.Texture, 726, 532, 165, 51);
}

float AUTWeap_Sniper::GetHeadshotScale() const
{
	if ( GetUTOwner()->GetVelocity().Size() <= GetUTOwner()->CharacterMovement->MaxWalkSpeedCrouched + 1.0f &&
		(GetUTOwner()->bIsCrouched || GetUTOwner()->CharacterMovement == NULL || GetUTOwner()->CharacterMovement->GetCurrentAcceleration().Size() < GetUTOwner()->CharacterMovement->MaxWalkSpeedCrouched + 1.0f) )
	{
		return SlowHeadshotScale;
	}
	else
	{
		return RunningHeadshotScale;
	}
}

AUTProjectile* AUTWeap_Sniper::FireProjectile()
{
	AUTProj_Sniper* SniperProj = Cast<AUTProj_Sniper>(Super::FireProjectile());
	if (SniperProj != NULL)
	{
		SniperProj->HeadScaling *= GetHeadshotScale();
	}
	return SniperProj;
}

void AUTWeap_Sniper::FireInstantHit(bool bDealDamage, FHitResult* OutHit)
{
	checkSlow(InstantHitInfo.IsValidIndex(CurrentFireMode));

	const FVector SpawnLocation = GetFireStartLoc();
	const FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);
	const FVector FireDir = SpawnRotation.Vector();
	const FVector EndTrace = SpawnLocation + FireDir * InstantHitInfo[CurrentFireMode].TraceRange;

	FHitResult Hit;
	AUTPlayerController* UTPC = UTOwner ? Cast<AUTPlayerController>(UTOwner->Controller) : NULL;
	float PredictionTime = UTPC ? UTPC->GetPredictionTime() : 0.f;
	HitScanTrace(SpawnLocation, EndTrace, Hit, PredictionTime);

	if (Role == ROLE_Authority && Cast<AUTCharacter>(Hit.Actor.Get()) == NULL)
	{
		// in some cases the head sphere is partially outside the capsule
		// so do a second search just for that
		AUTCharacter* AltTarget = Cast<AUTCharacter>(UUTGameplayStatics::PickBestAimTarget(GetUTOwner()->Controller, SpawnLocation, FireDir, 0.99f, (Hit.Location - SpawnLocation).Size(), AUTCharacter::StaticClass()));
		if (AltTarget != NULL && AltTarget->IsHeadShot(SpawnLocation, FireDir, GetHeadshotScale(), false, PredictionTime))
		{
			Hit = FHitResult(AltTarget, AltTarget->CapsuleComponent, SpawnLocation + FireDir * ((AltTarget->GetHeadLocation() - SpawnLocation).Size() - AltTarget->CapsuleComponent->GetUnscaledCapsuleRadius()), -FireDir);
		}
	}

	if (Role == ROLE_Authority)
	{
		UTOwner->SetFlashLocation(Hit.Location, CurrentFireMode);
	}
	if (Hit.Actor != NULL && Hit.Actor->bCanBeDamaged && bDealDamage)
	{
		int32 Damage = InstantHitInfo[CurrentFireMode].Damage;
		TSubclassOf<UDamageType> DamageType = InstantHitInfo[CurrentFireMode].DamageType;

		AUTCharacter* C = Cast<AUTCharacter>(Hit.Actor.Get());
		if (C != NULL && C->IsHeadShot(Hit.Location, FireDir, GetHeadshotScale(), true, PredictionTime))
		{
			Damage *= HeadshotDamageMult;
			if (HeadshotDamageType != NULL)
			{
				DamageType = HeadshotDamageType;
			}
		}
		Hit.Actor->TakeDamage(Damage, FUTPointDamageEvent(Damage, Hit, FireDir, DamageType, FireDir * InstantHitInfo[CurrentFireMode].Momentum), UTOwner->Controller, this);
	}
	if (OutHit != NULL)
	{
		*OutHit = Hit;
	}
}
