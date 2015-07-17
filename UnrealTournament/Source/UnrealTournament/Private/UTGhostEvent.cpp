// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGhostEvent.h"
#include "UTCharacter.h"
#include "UTGhostComponent.h"
#include "UTWeapon.h"


void UUTGhostEvent::ApplyEvent_Implementation(AUTCharacter* UTC)
{


}

void UUTGhostEvent_Move::ApplyEvent_Implementation(AUTCharacter* UTC)
{
	//Copy over the RepMovement so SimulatedMove can play it
	UTC->UTReplicatedMovement = RepMovement;
	UTC->OnRep_UTReplicatedMovement();

	//Set the movement flags
	if (UTC->UTCharacterMovement != nullptr)
	{
		//Rotate the controller so weapon fire is in the right direction
		if (UTC->GetController() != nullptr)
		{
			UTC->GetController()->SetControlRotation(RepMovement.Rotation);
		}

		UTC->UTCharacterMovement->UpdateFromCompressedFlags(CompressedFlags);
		UTC->OnRepFloorSliding();

		UTC->bIsCrouched = bIsCrouched;
		UTC->OnRep_IsCrouched();
		UTC->bApplyWallSlide = bApplyWallSlide;
	}
}

void UUTGhostEvent_MovementEvent::ApplyEvent_Implementation(AUTCharacter* UTC)
{
	UTC->MovementEvent = MovementEvent;
	UTC->MovementEventReplicated();
}

void UUTGhostEvent_Input::ApplyEvent_Implementation(AUTCharacter* UTC)
{
	if (UTC->GetWeapon() != nullptr)
	{
		for (int32 FireBit = 0; FireBit < sizeof(uint8) * 8; FireBit++)
		{
			uint8 OldBit = (UTC->GhostComponent->GhostFireFlags >> FireBit) & 1;
			uint8 NewBit = (FireFlags >> FireBit) & 1;
			if (NewBit != OldBit)
			{
				if (NewBit == 1)
				{
					UTC->GetWeapon()->BeginFiringSequence(FireBit, true);
				}
				else
				{
					UTC->GetWeapon()->EndFiringSequence(FireBit);
				}
			}
		}
		UTC->GhostComponent->GhostFireFlags = FireFlags;
	}
}

void UUTGhostEvent_Weapon::ApplyEvent_Implementation(AUTCharacter* UTC)
{
	if (WeaponClass != nullptr)
	{
		//AUTInventory* Existing = P->FindInventoryType(NewWeaponClass, true);
		//if (Existing == NULL || !Existing->StackPickup(NULL))
		//{
		FActorSpawnParameters Params;
		Params.bNoCollisionFail = true;
		Params.Instigator = UTC;
		AUTWeapon* NewWeapon = UTC->GetWorld()->SpawnActor<AUTWeapon>(WeaponClass, UTC->GetActorLocation(), UTC->GetActorRotation(), Params);

		//Give infinite ammo and instant switch speed to avoid any syncing issues
		for (int32 i = 0; i < NewWeapon->AmmoCost.Num(); i++)
		{
			NewWeapon->AmmoCost[i] = 0;
		}
		NewWeapon->BringUpTime = 0.0f;
		NewWeapon->PutDownTime = 0.0f;

		UTC->AddInventory(NewWeapon, true);
		UTC->SwitchWeapon(NewWeapon);
		//}
	}
}
