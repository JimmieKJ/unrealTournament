// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPickup.h"
#include "UTPickupAmmo.h"

AUTPickupAmmo::AUTPickupAmmo(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	Ammo.Amount = 10;
}

void AUTPickupAmmo::ProcessTouch_Implementation(APawn* TouchedBy)
{
	if (Role == ROLE_Authority && Cast<AUTCharacter>(TouchedBy) != NULL)
	{
		Super::ProcessTouch_Implementation(TouchedBy);
	}
}

void AUTPickupAmmo::GiveTo_Implementation(APawn* Target)
{
	AUTCharacter* P = Cast<AUTCharacter>(Target);
	if (P != NULL)
	{
		P->AddAmmo(Ammo);
	}
}