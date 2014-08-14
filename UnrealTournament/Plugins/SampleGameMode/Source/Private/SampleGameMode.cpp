// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "SampleGameMode.h"

ASampleGameMode::ASampleGameMode(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void ASampleGameMode::GiveDefaultInventory(APawn* PlayerPawn)
{
	// Don't call Super
		
	AUTCharacter* UTCharacter = Cast<AUTCharacter>(PlayerPawn);
	if (UTCharacter != nullptr && UTCharacter->GetInventory() == nullptr)
	{
		int32 InventoryIndex = PlayerPawn->PlayerState->Score;
		if (InventoryIndex >= StartingInventories.Num())
		{
			InventoryIndex = StartingInventories.Num() - 1;
		}
		
		if (StartingInventories.IsValidIndex(InventoryIndex))
		{
			for (int32 i = 0; i < StartingInventories[InventoryIndex].Inventory.Num(); i++)
			{
				if (StartingInventories[InventoryIndex].Inventory[i] != nullptr)
				{
					UTCharacter->AddInventory(GetWorld()->SpawnActor<AUTInventory>(StartingInventories[InventoryIndex].Inventory[i], FVector(0.0f), FRotator(0, 0, 0)), true);
				}
			}
		}
	}
}

void ASampleGameMode::ScoreKill(AController* Killer, AController* Other, TSubclassOf<UDamageType> DamageType)
{
	// Just a suicide, pass it through
	if (Killer == Other || Killer == nullptr)
	{
		Super::ScoreKill(Killer, Other, DamageType);
		return;
	}

	APlayerState* KillerPlayerState = Killer->PlayerState;
	if (KillerPlayerState)
	{
		int32 DamageIndex = KillerPlayerState->Score;
		if (DamageIndex >= ScoringDamageTypes.Num())
		{
			DamageIndex = ScoringDamageTypes.Num() - 1;
		}

		for (int32 i = 0; i < ScoringDamageTypes[DamageIndex].DamageType.Num(); i++)
		{
			if (DamageType == ScoringDamageTypes[DamageIndex].DamageType[i])
			{
				Super::ScoreKill(Killer, Other, DamageType);
				
				// If we changed score, give player a new gun
				if (DamageIndex != KillerPlayerState->Score)
				{
					AUTCharacter* UTCharacter = Cast<AUTCharacter>(Killer->GetPawn());
					if (UTCharacter != nullptr)
					{
						GiveNewGun(UTCharacter);
					}
				}

				break;
			}
		}
	}
}

void ASampleGameMode::GiveNewGun(AUTCharacter *UTCharacter)
{
	UTCharacter->DiscardAllInventory();

	int32 InventoryIndex = UTCharacter->PlayerState->Score;
	if (InventoryIndex >= StartingInventories.Num())
	{
		InventoryIndex = StartingInventories.Num() - 1;
	}

	if (StartingInventories.IsValidIndex(InventoryIndex))
	{
		for (int32 i = 0; i < StartingInventories[InventoryIndex].Inventory.Num(); i++)
		{
			if (StartingInventories[InventoryIndex].Inventory[i] != nullptr)
			{
				UTCharacter->AddInventory(GetWorld()->SpawnActor<AUTInventory>(StartingInventories[InventoryIndex].Inventory[i], FVector(0.0f), FRotator(0, 0, 0)), true);
			}
		}
	}
}