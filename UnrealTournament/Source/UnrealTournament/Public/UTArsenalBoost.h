// this is a "powerup" that gives other items to interface with the activated boost system
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTInventory.h"

#include "UTArsenalBoost.generated.h"

UCLASS()
class AUTArsenalBoost : public AUTInventory
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly)
	TArray<TSubclassOf<AUTInventory>> GrantedItems;

	virtual bool HandleGivenTo_Implementation(AUTCharacter* NewOwner) override
	{
		Instigator = NewOwner;
		SetOwner(NewOwner);
		UTOwner = NewOwner;
		for (TSubclassOf<AUTInventory> It : GrantedItems)
		{
			AUTInventory* Existing = UTOwner->FindInventoryType(It);
			if (Existing == NULL || !Existing->StackPickup(NULL))
			{
				UTOwner->CreateInventory(It);
			}
		}
		return true;
	}
};