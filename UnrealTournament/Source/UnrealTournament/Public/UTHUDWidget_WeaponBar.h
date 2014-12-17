// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
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

	TArray<AUTWeapon*> WeaponsInGroup;

	FWeaponGroup::FWeaponGroup()
	{
		Group = -1;
	}

	FWeaponGroup::FWeaponGroup(int32 inGroup, AUTWeapon* FirstWeapon)
	{
		Group = inGroup;
		WeaponsInGroup.Add(FirstWeapon);
	}

};

UCLASS()
class UUTHUDWidget_WeaponBar : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual void Draw_Implementation(float DeltaTime);
	virtual void InitializeWidget(AUTHUD* Hud);

protected:

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

	void CollectWeaponData(TArray<FWeaponGroup> &WeaponGroups);

private:
/*
	int32 LastSelectedGroup;		// Holds the group of the last selected weapon
	int32 BouncedWeapon;			// Used to bounce the animation slightly
	float C/urrentWeaponScale[10];	// Holds the scaling for each weapon

	float SelectedWeaponScale;		// How big should the selected weapon be
	float BounceWeaponScale;		// How big should it grow too before bouncing back

	float CurrentSelectedWeaponDisplayTime;
*/
};
