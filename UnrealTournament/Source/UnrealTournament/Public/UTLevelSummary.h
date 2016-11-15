// this object holds information on a level for display in menus without loading the full level package
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTLevelSummary.generated.h"

UCLASS(NotPlaceable, CustomConstructor)
class UNREALTOURNAMENT_API UUTLevelSummary : public UObject
{
	GENERATED_UCLASS_BODY()

	UUTLevelSummary(const FObjectInitializer& OI)
	: Super(OI)
	{
		Author = TEXT("Unknown");
		Description = NSLOCTEXT("LevelSummary", "DefaultDesc", "");
		OptimalPlayerCount = 6;
		OptimalTeamPlayerCount = 10;
	}

	/** localized title of the map */
	UPROPERTY(EditInstanceOnly, AssetRegistrySearchable, Category = LevelSummary)
	FString Title;

	/** desired number of players for best gameplay */
	UPROPERTY(EditInstanceOnly, AssetRegistrySearchable, Category = LevelSummary)
	int32 OptimalPlayerCount;

	/** desired number of players for best gameplay */
	UPROPERTY(EditInstanceOnly, AssetRegistrySearchable, Category = LevelSummary)
	int32 OptimalTeamPlayerCount;

	/** name of author(s) */
	UPROPERTY(EditInstanceOnly, AssetRegistrySearchable, Category = LevelSummary)
	FString Author;

	/** level description */
	UPROPERTY(EditInstanceOnly, AssetRegistrySearchable, Category = LevelSummary)
	FText Description;

	/** level screenshot */
	UPROPERTY(EditInstanceOnly, Category = LevelSummary)
	UTexture2D* Screenshot;

	/** Holds a string reference to the screenshot so various things can lazy load them */
	UPROPERTY(AssetRegistrySearchable)
	FString ScreenshotReference;

	UPROPERTY(EditInstanceOnly, AssetRegistrySearchable, Category = LevelSummary)
	TArray<FMapVignetteInfo> Vignettes;

	virtual void PreSave(const class ITargetPlatform* TargetPlatform)
	{
		if (Screenshot)
		{
			ScreenshotReference = Screenshot->GetPathName();
		}
		else
		{
			ScreenshotReference = TEXT("");
		}
	}

};