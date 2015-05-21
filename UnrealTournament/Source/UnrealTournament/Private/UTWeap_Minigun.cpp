// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_Minigun.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"
#include "UTWeaponStateFiringSpinUp.h"
#include "StatNames.h"

AUTWeap_Minigun::AUTWeap_Minigun(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.SetDefaultSubobjectClass<UUTWeaponStateFiringSpinUp>(TEXT("FiringState0")))
{
	if (FiringState.Num() > 0)
	{
		FireInterval[0] = 0.091f;
		Spread.Add(0.0675f);
		InstantHitInfo.AddZeroed();
		InstantHitInfo[0].Damage = 14;
		InstantHitInfo[0].TraceRange = 10000.0f;
	}
	if (AmmoCost.Num() < 2)
	{
		AmmoCost.SetNum(2);
	}
	ClassicGroup = 6;
	AmmoCost[0] = 1;
	AmmoCost[1] = 2;
	FireEffectInterval = 2;
	Ammo = 100;
	MaxAmmo = 300;
	FOVOffset = FVector(0.01f, 1.f, 1.6f);

	HUDIcon = MakeCanvasIcon(HUDIcon.Texture, 453.0f, 509.0f, 148.0f, 53.0f);

	BaseAISelectRating = 0.71f;
	BasePickupDesireability = 0.73f;
	bRecommendSuppressiveFire = true;

	KillStatsName = NAME_MinigunKills;
	AltKillStatsName = NAME_MinigunShardKills;
	DeathStatsName = NAME_MinigunDeaths;
	AltDeathStatsName = NAME_MinigunShardDeaths;
}

float AUTWeap_Minigun::GetAISelectRating_Implementation()
{
	AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
	if (B == NULL || B->GetEnemy() == NULL)
	{
		return BaseAISelectRating;
	}
	else if (!CanAttack(B->GetEnemy(), B->GetEnemyLocation(B->GetEnemy(), false), false))
	{
		return BaseAISelectRating - 0.15f;
	}
	else
	{
		return BaseAISelectRating * FMath::Min<float>(UTOwner->DamageScaling * UTOwner->GetFireRateMultiplier(), 1.5f);
	}
}

bool AUTWeap_Minigun::CanAttack_Implementation(AActor* Target, const FVector& TargetLoc, bool bDirectOnly, bool bPreferCurrentMode, uint8& BestFireMode, FVector& OptimalTargetLoc)
{
	// prefer to keep current fire mode as there is a spindown cost associated with switching
	return Super::CanAttack_Implementation(Target, TargetLoc, bDirectOnly, bPreferCurrentMode || (IsFiring() && FMath::FRand() < 0.9f), BestFireMode, OptimalTargetLoc);
}

bool AUTWeap_Minigun::HasAmmo(uint8 FireModeNum)
{
	return (Ammo >= 1);
}
