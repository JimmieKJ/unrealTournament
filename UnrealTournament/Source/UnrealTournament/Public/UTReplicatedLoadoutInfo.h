// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.h"
#include "UTReplicatedLoadoutInfo.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTReplicatedLoadoutInfo : public AInfo
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Replicated)
	FName ItemTag;

	// The weapon
	UPROPERTY(Replicated, replicatedUsing  = LoadItemImage)
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

#if !UE_SERVER
	TSharedPtr<FSlateDynamicImageBrush> SlateImage;
#endif

public:
	// Returns the slate image brush for this loadout info.  NOTE: It will create it if it doesn't
	// exist.
	UFUNCTION()
	void LoadItemImage();

#if !UE_SERVER
	// Used to get the Slate brush in the UI.  NOTE:  LoadItemImage should be called at least once before calling this
	// to prime the image.
	const FSlateBrush* GetItemImage() const;

#endif
	virtual void Destroyed() override;


public:
	UPROPERTY()
	bool bUnlocked;
};



