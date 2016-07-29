// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 **/

#include "UTHUDWidget_Boost.generated.h"

const float UNLOCK_ANIM_DURATION =0.65f;

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_Boost : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual void InitializeWidget(AUTHUD* Hud);
	virtual void PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter) override;
	//virtual void Draw_Implementation(float DeltaTime);
	virtual bool ShouldDraw_Implementation(bool bShowScores);
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FVector2D LockedPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FVector2D UnLockedPosition;

	// This is the background slate for the weapon icon portion of the bar.  Index 0 is the for the first item in a group, Index 1 is for all other items
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture Background;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture BoostIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture BoostSkull;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text BoostText;

	bool bAnimating;
	float AnimTime;
	float LastAnimTime;

private:
	UPROPERTY()
	UMaterialInterface* HudTimerMI;

	UPROPERTY()
	UMaterialInstanceDynamic* HudTimerMID;
	float IconScale;
};
