// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.h"
#include "UTReplicatedLoadoutInfo.generated.h"

UCLASS()
class AUTReplicatedLoadoutInfo : public AInfo
{
	GENERATED_UCLASS_BODY()

	// The weapon
	UPROPERTY(Replicated)
	TSubclassOf<AUTInventory> ItemClass;

	// What rounds are this weapon available in
	UPROPERTY(Replicated)
	uint8 RoundMask;

	UPROPERTY(Replicated)
	uint32 bDefaultInclude:1;

	UPROPERTY(Replicated)
	uint32 bPurchaseOnly:1;

	// What is the current cost of this weapon
	UPROPERTY(Replicated)
	float CurrentCost;

public:

		

};



