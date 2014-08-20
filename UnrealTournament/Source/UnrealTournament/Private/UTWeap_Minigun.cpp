// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_Minigun.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"
#include "UTWeaponStateFiringSpinUp.h"

AUTWeap_Minigun::AUTWeap_Minigun(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP.SetDefaultSubobjectClass<UUTWeaponStateFiringSpinUp>(TEXT("FiringState0")))
{
	if (FiringState.Num() > 0)
	{
#if WITH_EDITORONLY_DATA
		FiringStateType[0] = UUTWeaponStateFiringSpinUp::StaticClass();
#endif
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
	AmmoCost[0] = 1;
	AmmoCost[1] = 2;

	Ammo = 100;
	MaxAmmo = 300;

	HUDIcon = MakeCanvasIcon(HUDIcon.Texture, 453.0f, 509.0f, 148.0f, 53.0f);
}