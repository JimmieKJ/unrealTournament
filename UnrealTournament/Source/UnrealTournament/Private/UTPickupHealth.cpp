// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPickup.h"
#include "UTPickupHealth.h"

AUTPickupHealth::AUTPickupHealth(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	HealAmount = 25;
}

int32 AUTPickupHealth::GetHealMax_Implementation(AUTCharacter* P)
{
	if (P == NULL)
	{
		return 0;
	}
	else
	{
		return bSuperHeal ? P->SuperHealthMax : P->HealthMax;
	}
}

void AUTPickupHealth::ProcessTouch_Implementation(APawn* TouchedBy)
{
	if (Role == ROLE_Authority)
	{
		AUTCharacter* P = Cast<AUTCharacter>(TouchedBy);
		if (P != NULL && (bSuperHeal || P->Health < GetHealMax(P)))
		{
			Super::ProcessTouch_Implementation(TouchedBy);
		}
	}
}

void AUTPickupHealth::GiveTo_Implementation(APawn* Target)
{
	AUTCharacter* P = Cast<AUTCharacter>(Target);
	if (P != NULL)
	{
		P->Health = FMath::Max<int32>(P->Health, FMath::Min<int32>(P->Health + HealAmount, GetHealMax(P)));
	}
}