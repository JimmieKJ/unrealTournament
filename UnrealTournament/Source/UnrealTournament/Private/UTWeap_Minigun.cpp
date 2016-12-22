// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_Minigun.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"
#include "UTWeaponStateFiringSpinUp.h"
#include "StatNames.h"

AUTWeap_Minigun::AUTWeap_Minigun(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.SetDefaultSubobjectClass<UUTWeaponStateFiringSpinUp>(TEXT("FiringState0")))
{
	DefaultGroup = 6;
	FireEffectInterval = 2;
	Ammo = 80;
	MaxAmmo = 240;
	FOVOffset = FVector(0.01f, 1.f, 1.6f);
	bIgnoreShockballs = true;

	AmmoWarningAmount = 25;
	AmmoDangerAmount = 10;
	HUDIcon = MakeCanvasIcon(HUDIcon.Texture, 453.0f, 509.0f, 148.0f, 53.0f);

	BaseAISelectRating = 0.71f;
	BasePickupDesireability = 0.73f;
	bRecommendSuppressiveFire = true;

	KillStatsName = NAME_MinigunKills;
	AltKillStatsName = NAME_MinigunShardKills;
	DeathStatsName = NAME_MinigunDeaths;
	AltDeathStatsName = NAME_MinigunShardDeaths;
	HitsStatsName = NAME_MinigunHits;
	ShotsStatsName = NAME_MinigunShots;
	
	WeaponCustomizationTag = EpicWeaponCustomizationTags::Minigun;
	WeaponSkinCustomizationTag = EpicWeaponSkinCustomizationTags::Minigun;
	HighlightText = NSLOCTEXT("Weapon", "StingerHighlightText", "Sting Like a Bee");
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
	AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
	// prefer to keep current fire mode as there is a spindown cost associated with switching
	bPreferCurrentMode = bPreferCurrentMode || (Cast<UUTWeaponStateFiringSpinUp>(CurrentState) != nullptr && FMath::FRand() < 0.9f);
	if (Super::CanAttack_Implementation(Target, TargetLoc, bDirectOnly, bPreferCurrentMode, BestFireMode, OptimalTargetLoc))
	{
		if (!bPreferCurrentMode && B != nullptr)
		{
			if (UTOwner->DamageScaling * UTOwner->GetFireRateMultiplier() > 1.0f)
			{
				BestFireMode = 0;
			}
			else
			{
				const float TargetDist = (TargetLoc - B->GetPawn()->GetActorLocation()).Size();
				if (TargetDist < 3000.0f)
				{
					BestFireMode = 0;
				}
				else if (TargetDist < 5000.0f)
				{
					BestFireMode = FMath::FRand() < TargetDist / 5000.0f ? 1 : 0;
				}
				else
				{
					BestFireMode = 1;
				}
			}
		}
		return true;
	}
	else if (B != nullptr && Target == B->GetEnemy() && GetWorld()->TimeSeconds - B->GetEnemyInfo(B->GetEnemy(), false)->LastSeenTime < 1.0f)
	{
		// alt fire radius might still hit
		OptimalTargetLoc = B->GetEnemyInfo(B->GetEnemy(), false)->LastSeenLoc;
		BestFireMode = 1;
		return true;
	}
	else
	{
		return false;
	}
}

bool AUTWeap_Minigun::HasAmmo(uint8 FireModeNum)
{
	return (Ammo >= 1);
}
