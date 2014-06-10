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
	bCallModifyDamageTaken = true;
}

void AUTArmor::ModifyDamageTaken_Implementation(int32& Damage, FVector& Momentum, const FDamageEvent& DamageEvent, AController* InstigatedBy, AActor* DamageCauser)
{
	if (Damage > 0)
	{
		int32 Absorb = FMath::Min<int32>(ArmorAmount, FMath::Max<int32>(1, Damage * AbsorptionPct));
		if (bAbsorbMomentum)
		{
			Momentum *= 1.0f - float(Absorb) / float(Damage);
		}
		Damage -= Absorb;
		ArmorAmount -= Absorb;
		if (ArmorAmount <= 0)
		{
			Destroy();
		}
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