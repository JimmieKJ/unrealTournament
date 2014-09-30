// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTInventory.h"
#include "UTArmor.h"
#include "UnrealNetwork.h"

AUTArmor::AUTArmor(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	ArmorAmount = 50;
	AbsorptionPct = 0.333f;
	bCallDamageEvents = true;
	BasePickupDesireability = 1.5f;
}

void AUTArmor::GivenTo(AUTCharacter* NewOwner, bool bAutoActivate)
{
	Super::GivenTo(NewOwner, bAutoActivate);

	if (OverlayMaterial != NULL)
	{
		NewOwner->SetCharacterOverlay(OverlayMaterial, true);
	}

	NewOwner->CheckArmorStacking();
}

void AUTArmor::Removed()
{
	if (OverlayMaterial != NULL)
	{
		GetUTOwner()->SetCharacterOverlay(OverlayMaterial, false);
	}
	Super::Removed();
}

void AUTArmor::ModifyDamageTaken_Implementation(int32& Damage, FVector& Momentum, AUTInventory*& HitArmor, const FDamageEvent& DamageEvent, AController* InstigatedBy, AActor* DamageCauser)
{
	if (Damage > 0)
	{
		if (HitArmor == NULL)
		{
			HitArmor = this;
		}
		int32 Absorb = FMath::Min<int32>(ArmorAmount, FMath::Max<int32>(1, Damage * AbsorptionPct));
		if (bAbsorbMomentum)
		{
			Momentum *= 1.0f - float(Absorb) / float(Damage);
		}
		Damage -= Absorb;
		ReduceArmor(Absorb);
	}
}

void AUTArmor::ReduceArmor(int Amount)
{
	ArmorAmount -= Amount;
	if (ArmorAmount <= 0)
	{
		Destroy();
	}
}

bool AUTArmor::StackPickup_Implementation(AUTInventory* ContainedInv)
{
	if (ContainedInv != NULL)
	{
		ArmorAmount = FMath::Clamp<int32>(ArmorAmount + Cast<AUTArmor>(ContainedInv)->ArmorAmount, ArmorAmount, GetClass()->GetDefaultObject<AUTArmor>()->ArmorAmount);
	}
	else
	{
		ArmorAmount = GetClass()->GetDefaultObject<AUTArmor>()->ArmorAmount;
	}
	GetUTOwner()->CheckArmorStacking();
	return true;
}

void AUTArmor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTArmor, ArmorAmount, COND_None);
}

float AUTArmor::BotDesireability_Implementation(APawn* Asker, AActor* Pickup, float PathDistance) const
{
	AUTCharacter* P = Cast<AUTCharacter>(Asker);
	if (P == NULL)
	{
		return 0.0f;
	}
	else
	{
		int32 MatchingArmor = 0;
		int32 TotalArmor = 0;
		float MaxAbsorbPct = 0.0f;
		for (AUTInventory* Inv = P->GetInventory(); Inv != NULL; Inv = Inv->GetNext())
		{
			AUTArmor* Armor = Cast<AUTArmor>(Inv);
			if (Armor != NULL)
			{
				TotalArmor += Armor->ArmorAmount;
				MaxAbsorbPct = FMath::Max<float>(MaxAbsorbPct, Armor->AbsorptionPct);
				if (Armor->GetClass() == GetClass())
				{
					MatchingArmor += Armor->ArmorAmount;
				}
			}
		}
		// if this armor will overwrite other armor consider full value regardless of stacking
		int32 Value = (MaxAbsorbPct < AbsorptionPct) ? (ArmorAmount - MatchingArmor) : FMath::Min<int32>(P->MaxStackedArmor - TotalArmor, ArmorAmount - MatchingArmor);
		float Desire = 0.013f * BasePickupDesireability * float(Value);
		//if (!WorldInfo.Game.bTeamGame && UTBot(C) != None && UTBot(C).Skill >= 4.0)
		//{
			// high skill bots keep considering powerups that they don't need if they can still pick them up
			// to deny the enemy any chance of getting them
		//	Desire = FMax(Desire, 0.001);
		//}
		return Desire;
	}
}