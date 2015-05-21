// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 **/

#include "UTHUDWidget_WeaponBar.generated.h"

USTRUCT()
struct FWeaponGroup
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 Group;

	UPROPERTY()
	TArray<AUTWeapon*> WeaponsInGroup;

	FWeaponGroup()
	{
		Group = -1;
	}

	FWeaponGroup(int32 inGroup, AUTWeapon* FirstWeapon)
	{
		Group = inGroup;
		if (FirstWeapon)
		{
			WeaponsInGroup.Add(FirstWeapon);
		}
	}

};

UCLASS(Config=Game)
class UNREALTOURNAMENT_API UUTHUDWidget_WeaponBar : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual void Draw_Implementation(float DeltaTime);
	virtual void InitializeWidget(AUTHUD* Hud);

protected:

	// Insures this many groups will be shown on the weapon bar
	UPROPERTY(config)
	int32 RequiredGroups;

	// This is the background slate for the weapon icon portion of the bar.  Index 0 is the for the first item in a group, Index 1 is for all other items
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FHUDRenderObject_Texture> CellBackground;

	// This is the border for the weapon icon portion of the bar.  Index 0 is the for the first item in a group, Index 1 is for all other items
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FHUDRenderObject_Texture> CellBorders;

	// The header cap is the border piece that goes between the header tab and the weeapon icon portion of the bar.   Index 0 is the for the first item in a group, Index 1 is for all other items
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FHUDRenderObject_Texture> GroupHeaderCap;

	// This is the border piece that stretches between the Group header cap and the header tab.  It will expand as needed to fill the gap.  Index 0 is the for the first item in a group, Index 1 is for all other items
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FHUDRenderObject_Texture> GroupSpacerBorder;

	// The Header Tab is the portion of the bar that shows the "group" key to trigger the group.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FHUDRenderObject_Texture> HeaderTab;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text GroupText;
	
	// The Header Tab is the portion of the bar that shows the "group" key to trigger the group.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture WeaponIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text WeaponNameText;

	// How long to show the name of a weapon when switching to it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float WeaponNameDisplayTime;

	// How wide (unscaled) should the final cell be.  This should be the width of all the parts together.  However,
	// the CellBackground and CellBorders will be stretched to fill any portion of this not filled.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	float CellWidth;

	// How much should a selected cell be scaled by.  NOTE: All of
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	float SelectedCellScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	float SelectedAnimRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	UTexture* BarTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	FTextureUVs BarTextureUVs;

	/** Fills the WeaponGroups and returns the number of weapons found. */
	int32 CollectWeaponData(TArray<FWeaponGroup> &WeaponGroups, float DeltaTime);

	// Override the default version to return the secondary scaling
	virtual float GetDrawScaleOverride();

private:

	float InactiveOpacity;
	float InactiveIconOpacity;

	float WeaponNameDisplayTimer;

	float FadeTimer;
	int32 LastGroup;
	float LastGroupSlot;
};
