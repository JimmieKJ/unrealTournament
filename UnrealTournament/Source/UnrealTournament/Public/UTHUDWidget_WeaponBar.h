// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 **/

#include "UTWeapon.h"
#include "UTHUDWidget_WeaponBar.generated.h"

const float CELL_ASPECT_RATIO = 2.43478260869565f;
const float DEFAULT_CELL_WIDTH = 112.0f;
const float DEFUALT_CELL_HEIGHT = 46.0f;
const float CELL_PADDING_VERT = 4.0f;
const float GROUP_PADDING_VERT = 8.0f;
const float CELL_PADDING_HORZ = 4.0f;
const float GROUP_PADDING_HORZ = 12.0f;
const float ACTIVE_FADE_DELAY = 1.5f;
const float ACTIVE_FADE_TIME = 0.75f;

USTRUCT()
struct FWeaponGroupInfo
{
	GENERATED_USTRUCT_BODY()

	// Holds a list of class of all of the possible weapons in this group.
	UPROPERTY()
	TArray<TSubclassOf<AUTWeapon> > WeaponClasses;

	// Holds a list of the actual weapons the pawn owns in this group.  NOTE: these might be null as one entry is created per WeaponClass
	UPROPERTY()
	TArray<AUTWeapon*> Weapons;

	UPROPERTY()
	int32 Group;

	FWeaponGroupInfo()
		: Group(0)
	{}

	void AddWeapon(TSubclassOf<AUTWeapon> inWeaponClass, AUTWeapon* inWeapon, int32 inGroup)
	{
		if (inWeaponClass != nullptr && WeaponClasses.Find(inWeaponClass) == INDEX_NONE)
		{
			WeaponClasses.Add(inWeaponClass);
			Weapons.Add(inWeapon);
			Group = inGroup;
		}
	}

	void UpdateWeapon(TSubclassOf<AUTWeapon> inWeaponClass, AUTWeapon* inWeapon, int32 inGroup)
	{
		int32 Index = WeaponClasses.Find(inWeaponClass);
		if (Index != INDEX_NONE)
		{
			Weapons[Index] = inWeapon;		
		}
		else
		{
			AddWeapon(inWeaponClass, inWeapon, inGroup);
		}
	}
};

USTRUCT()
struct FWeaponBarCell
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector2D DrawPosition;

	UPROPERTY()
	FVector2D DrawSize;

	UPROPERTY()
	AUTWeapon* Weapon;

	UPROPERTY()
	TSubclassOf<AUTWeapon> WeaponClass;

	UPROPERTY()
	int32 WeaponGroup;

	FWeaponBarCell()
		: DrawPosition(FVector2D(0.0f,0.0f))
		, DrawSize(FVector2D(0.0f,0.0f))
		, Weapon(nullptr)
		, WeaponGroup(-1)
	{
	}

	FWeaponBarCell(FVector2D inDrawPosition, FVector2D inDrawSize, AUTWeapon* inWeapon, TSubclassOf<AUTWeapon> inWeaponClass, int32 inGroup)
		: DrawPosition(inDrawPosition)
		, DrawSize(inDrawSize)
		, Weapon(inWeapon)
		, WeaponClass(inWeaponClass)
		, WeaponGroup(inGroup)
	{
	}
};

UCLASS(Config=Game)
class UNREALTOURNAMENT_API UUTHUDWidget_WeaponBar : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual void PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter) override;
	virtual void Draw_Implementation(float DeltaTime);
	virtual void InitializeWidget(AUTHUD* Hud);
	virtual bool ShouldDraw_Implementation(bool bShowScores) override;


protected:
	void UpdateGroups(AUTHUD* Hud);

	// weapons available in each slot (in this persistent copy only the classes are filled out)
	UPROPERTY()
	TArray<FWeaponGroupInfo> KnownWeaponMap;

	// Where on the screen in pixels should this bar be displayed when in it's vertical form
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	FVector2D VerticalPosition;

	// Where on the screen in relative positioning should this bar be displayed when in it's vertical form
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	FVector2D VerticalScreenPosition;

	// Where on the screen in pixels should this bar be displayed when in it's horz form
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	FVector2D HorizontalPosition;

	// Where on the screen in relative positioning should this bar be displayed when in it's horz. form
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	FVector2D HorizontalScreenPosition;

	// What is the max size of the widget in 1080p.  X is used for horz. layouts, Y for vertical.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	FVector2D MaxSize;

	// How much space in the cell should the ammo bar take
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	FVector2D AmmoBarSizePct;
	
	// The text to use for the Group / Weapon #
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	FHUDRenderObject_Text GroupText;
	
	// The Header Tab is the portion of the bar that shows the "group" key to trigger the group.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	FHUDRenderObject_Texture WeaponIcon;

	// The background image for each cell
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	FHUDRenderObject_Texture CellBackground;

	// The background for the ammo bar
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	FHUDRenderObject_Texture AmmoBarBackground;

	// The fill for the ammo bar
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	FHUDRenderObject_Texture AmmoBarFill;

	// How fast should a cell transition between scales
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	float SelectedAnimRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	float SelectedOpacity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	float ActiveOpacity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponBar")
	float InactiveOpacity;

	// Override the default version to return the secondary scaling
	virtual float GetDrawScaleOverride();

	UPROPERTY()
	UTexture2D* DefaultWeaponIconAtlas;

	FTextureUVs ActiveBackgroundUVs;
	FTextureUVs InactiveBackgroundUVs;
	FTextureUVs SelectedBackgroundUVs;

	TArray<FWeaponBarCell> Cells;

	int32 NumWeaponsToDraw;
	bool bVerticalLayout;

	UPROPERTY()
	AUTWeapon* LastSelectedWeapon;

	float LastActiveTime;
	float InactiveFadePerc;
};
