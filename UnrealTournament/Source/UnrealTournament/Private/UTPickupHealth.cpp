// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPickup.h"
#include "UTPickupHealth.h"

AUTPickupHealth::AUTPickupHealth(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	HealAmount = 25;
}

void AUTPickupHealth::ProcessTouch_Implementation(APawn* TouchedBy)
{
	if (Role == ROLE_Authority && Cast<AUTCharacter>(TouchedBy) != NULL)
	{
		Super::ProcessTouch_Implementation(TouchedBy);
	}
}

void AUTPickupHealth::GiveTo_Implementation(APawn* Target)
{
	AUTCharacter* P = Cast<AUTCharacter>(Target);
	if (P != NULL)
	{
		P->Health = FMath::Min<int32>(P->Health + HealAmount, bSuperHeal ? P->SuperHealthMax : P->HealthMax);
	}
}