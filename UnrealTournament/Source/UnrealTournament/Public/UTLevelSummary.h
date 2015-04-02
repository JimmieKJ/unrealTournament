// this object holds information on a level for display in menus without loading the full level package
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTLevelSummary.generated.h"

UCLASS(NotPlaceable, CustomConstructor)
class UUTLevelSummary : public UObject
{
	GENERATED_UCLASS_BODY()

	UUTLevelSummary(const FObjectInitializer& OI)
	: Super(OI)
	{
		Author = TEXT("Unknown");
		Description = NSLOCTEXT("LevelSummary", "DefaultDesc", "Needs Description");
		OptimalPlayerCount.X = 8;
		OptimalPlayerCount.Y = 12;
	}

	/** localized title of the map */
	UPROPERTY(EditInstanceOnly, AssetRegistrySearchable, Category = LevelSummary)
	FString Title;
	/** desired number of players for best gameplay */
	UPROPERTY(EditInstanceOnly, Category = LevelSummary)
	FIntPoint OptimalPlayerCount;
	/** name of author(s) */
	UPROPERTY(EditInstanceOnly, Category = LevelSummary)
	FString Author;
	/** level description */
	UPROPERTY(EditInstanceOnly, Category = LevelSummary)
	FText Description;
	/** level screenshot */
	UPROPERTY(EditInstanceOnly, Category = LevelSummary)
	UTexture2D* Screenshot;
};