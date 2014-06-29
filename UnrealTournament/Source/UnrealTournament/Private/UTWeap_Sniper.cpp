// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_Sniper.h"
#include "UTProj_Sniper.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"
#include "UTWeaponStateZooming.h"

AUTWeap_Sniper::AUTWeap_Sniper(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP.SetDefaultSubobjectClass<UUTWeaponStateZooming>(TEXT("FiringState1")) )
{
	SlowHeadshotScale = 1.75f;
	RunningHeadshotScale = 0.8f;
	ProjClass.Insert(AUTProj_Sniper::StaticClass(), 0);
	if (FiringStateType.Num() > 1)
	{
		FiringStateType[1] = UUTWeaponStateZooming::StaticClass();
	}

	IconCoordinates = FTextureUVs(726,532,165,51);
}

AUTProjectile* AUTWeap_Sniper::FireProjectile()
{
	AUTProj_Sniper* SniperProj = Cast<AUTProj_Sniper>(Super::FireProjectile());
	if (SniperProj != NULL)
	{
		if (GetUTOwner()->GetVelocity().Size() <= GetUTOwner()->CharacterMovement->MaxWalkSpeed * GetUTOwner()->CharacterMovement->CrouchedSpeedMultiplier)
		{
			SniperProj->HeadScaling *= SlowHeadshotScale;
		}
		else
		{
			SniperProj->HeadScaling *= RunningHeadshotScale;
		}
	}
	return SniperProj;
}

bool AUTWeap_Sniper::HasAnyAmmo()
{
	// Only care about normal fire since alt-fire is zoom
	return HasAmmo(0);
}
