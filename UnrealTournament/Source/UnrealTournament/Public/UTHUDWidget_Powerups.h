// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget_Powerups.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_Powerups : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual void Draw_Implementation(float DeltaTime);
	virtual bool ShouldDraw_Implementation(bool bShowScores);

protected:

	// This is the background slate for the weapon icon portion of the bar.  Index 0 is the for the first item in a group, Index 1 is for all other items
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FHUDRenderObject_Texture> LeftTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FHUDRenderObject_Texture> TileTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FHUDRenderObject_Texture> RightTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture IconTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text TimeText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float ScaleFactor;
};
