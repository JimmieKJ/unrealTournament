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
	return true;
}

void AUTArmor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTArmor, ArmorAmount, COND_None);
}