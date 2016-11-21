// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPickup.h"
#include "UTPickupAmmo.h"

AUTPickupAmmo::AUTPickupAmmo(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	Ammo.Amount = 10;
	BaseDesireability = 0.2f;
	PickupMessageString = NSLOCTEXT("PickupMessage", "AmmoPickedUp", "Ammo");
	Collision->InitCapsuleSize(60.f, 60.f);
}

bool AUTPickupAmmo::AllowPickupBy_Implementation(APawn* Other, bool bDefaultAllowPickup)
{
	return Super::AllowPickupBy_Implementation(Other, bDefaultAllowPickup && Cast<AUTCharacter>(Other) != NULL && !((AUTCharacter*)Other)->HasMaxAmmo(Ammo.Type) && !((AUTCharacter*)Other)->IsRagdoll());
}

void AUTPickupAmmo::GiveTo_Implementation(APawn* Target)
{
	AUTCharacter* P = Cast<AUTCharacter>(Target);
	if (P != NULL)
	{
		P->AddAmmo(Ammo);
		AUTPickup::GiveTo_Implementation(Target);
	}
}

float AUTPickupAmmo::BotDesireability_Implementation(APawn* Asker, AController* RequestOwner, float TotalDistance)
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
		return BotDesireability(Asker, Asker->Controller, TotalDistance);
	}
	else
	{
		// always want ammo for bot's favorite weapon
		AUTBot* B = Cast<AUTBot>(P->Controller);
		if (B != NULL && B->IsFavoriteWeapon(Ammo.Type))
		{
			return BotDesireability(Asker, Asker->Controller, TotalDistance);
		}
		else
		{
			// if have weapon and need ammo for it, then detour
			AUTWeapon* W = P->FindInventoryType<AUTWeapon>(Ammo.Type, true);
			if (W != NULL && W->Ammo <= W->MaxAmmo / 2)
			{
				return BotDesireability(Asker, Asker->Controller, TotalDistance);
			}
			else
			{
				return 0.0f;
			}
		}
	}
}