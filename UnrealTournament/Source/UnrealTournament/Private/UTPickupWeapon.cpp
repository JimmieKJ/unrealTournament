// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTPickup.h"
#include "UTPickupInventory.h"
#include "UTPickupWeapon.h"

void AUTPickupWeapon::BeginPlay()
{
	AUTPickup::BeginPlay(); // skip AUTPickupInventory so we can propagate WeaponType as InventoryType

	SetInventoryType(WeaponType);
}

void AUTPickupWeapon::SetInventoryType(TSubclassOf<AUTInventory> NewType)
{
	WeaponType = *NewType;
	if (WeaponType == NULL)
	{
		NewType = NULL;
	}
	Super::SetInventoryType(NewType);
}

bool AUTPickupWeapon::IsTaken(APawn* TestPawn)
{
	for (int32 i = Customers.Num() - 1; i >= 0; i--)
	{
		if (Customers[i].P == NULL || Customers[i].P->bTearOff || Customers[i].P->bPendingKillPending)
		{
			Customers.RemoveAt(i);
		}
		else if (Customers[i].P == TestPawn)
		{
			return (GetWorld()->TimeSeconds < Customers[i].NextPickupTime);
		}
	}
	return false;
}

void AUTPickupWeapon::ProcessTouch_Implementation(APawn* TouchedBy)
{
	if (State.bActive && Cast<AUTCharacter>(TouchedBy) != NULL)
	{
		if (WeaponType == NULL || !WeaponType.GetDefaultObject()->bWeaponStay)
		{
			Super::ProcessTouch_Implementation(TouchedBy);
		}
		else if (!IsTaken(TouchedBy))
		{
			new(Customers) FWeaponPickupCustomer(TouchedBy, GetWorld()->TimeSeconds + RespawnTime);
			if (Role == ROLE_Authority)
			{
				GiveTo(TouchedBy);
			}
			PlayTakenEffects(false);
			if (TouchedBy->IsLocallyControlled())
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(TouchedBy->Controller);
				if (PC != NULL)
				{
					PC->AddWeaponPickup(this);
				}
			}
		}
	}
}