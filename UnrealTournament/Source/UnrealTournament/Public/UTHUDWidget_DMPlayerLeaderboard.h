// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 **/

#include "UTHUDWidget_DMPlayerLeaderboard.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_DMPlayerLeaderboard : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual void Draw_Implementation(float DeltaTime);
	virtual void InitializeWidget(AUTHUD* Hud);
	virtual bool ShouldDraw_Implementation(bool bShowScores) override;

protected:

	// This is the header where the player's "Place" will be displayed.  Needs 2 elemenets, 0 = the slate, 1 = the header
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FHUDRenderObject_Texture> Header;

	// This is the bar where the player's "name" will be displayed.  Needs 2 elemenets, 0 = the slate, 1 = the header
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FHUDRenderObject_Texture> Bar;

	// This is the bar where the player's "name" will be displayed.  Needs 2 elemenets, 0 = the slate, 1 = the header
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FHUDRenderObject_Texture> SpreadBar;

	// This is the image that will be colorized Gold, Silver or Bronze if the player is in the top 3
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture Medal;

	// This is the up indicator
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture UpIcon;

	// This is the up indicator
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture DownIcon;

	// This is the template to use when drawing the player's "Place"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text PlaceTextTemplate;
	
	// This is the template to use when drawing the player's "Name"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text NameTextTemplate;

	// This is the template to use when drawing the player's "Name"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text SpreadTextTemplate;

	// This is the template to use when drawing the player's "Name"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FLinearColor OwnerNameColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
		FText SpreadText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float BarWidth;

	void DrawPlayer(float& YPosition, int32 PlayerIndex, AUTPlayerState* OwnerPS);
};
