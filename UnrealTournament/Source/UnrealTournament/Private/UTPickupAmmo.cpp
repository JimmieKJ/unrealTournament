// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPickup.h"
#include "UTPickupAmmo.h"

AUTPickupAmmo::AUTPickupAmmo(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	Ammo.Amount = 10;
	bDisplayRespawnTimer = false;
	BaseDesireability = 0.2f;
}

void AUTPickupAmmo::ProcessTouch_Implementation(APawn* TouchedBy)
{
	if (Role == ROLE_Authority && Cast<AUTCharacter>(TouchedBy) != NULL && !((AUTCharacter*)TouchedBy)->HasMaxAmmo(Ammo.Type))
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

float AUTPickupAmmo::BotDesireability_Implementation(APawn* Asker, float TotalDistance)
{
	//Bot = UTBot(C);
	//if (Bot != None && !Bot.bHuntPlayer)
	if (Ammo.Type == NULL)
	{
		return 0.0f;
	}
	else
	{
		AUTCharacter* P = Cast<AUTCharacter>(Asker);
		if (P == NULL)
		{
			return 0.0f;
		}
		else
		{
			int32 MaxAmmo = Ammo.Type.GetDefaultObject()->MaxAmmo;
			AUTWeapon* W = P->FindInventoryType<AUTWeapon>(Ammo.Type, true);
			if (W != NULL)
			{
				MaxAmmo = W->MaxAmmo;
			}
			float Result = BaseDesireability * (1.0f - float(P->GetAmmoAmount(Ammo.Type)) / float(MaxAmmo));
			// increase desireability for the bot's favorite weapon
			//if (ClassIsChildOf(TargetWeapon, Bot.FavoriteWeapon))
			//{
			//	Result *= 1.5;
			//}
			return Result;
		}
	}
}
float AUTPickupAmmo::DetourWeight_Implementation(APawn* Asker, float TotalDistance)
{
	AUTCharacter* P = Cast<AUTCharacter>(Asker);
	if (P == NULL)
	{
		return 0.0f;
	}
	// if short distance always grab in case we find the weapon
	else if (TotalDistance < 1200.0f)
	{
		return BotDesireability(Asker, TotalDistance);
	}
	else
	{
		// always want ammo for bot's favorite weapon
		AUTBot* B = Cast<AUTBot>(P->Controller);
		if (B != NULL && B->IsFavoriteWeapon(Ammo.Type))
		{
			return BotDesireability(Asker, TotalDistance);
		}
		else
		{
			// if have weapon and need ammo for it, then detour
			AUTWeapon* W = P->FindInventoryType<AUTWeapon>(Ammo.Type, true);
			if (W != NULL && W->Ammo <= W->MaxAmmo / 2)
			{
				return BotDesireability(Asker, TotalDistance);
			}
			else
			{
				return 0.0f;
			}
		}
	}
}