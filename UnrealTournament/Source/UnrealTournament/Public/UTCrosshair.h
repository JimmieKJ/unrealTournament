// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Object.h"
#include "UTCrosshair.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class UNREALTOURNAMENT_API UUTCrosshair : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Crosshair)
	FCanvasIcon CrosshairIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Crosshair)
	FText CrosshairName;
	
	UFUNCTION(BlueprintNativeEvent)
	void DrawCrosshair(UCanvas* Canvas, AUTWeapon* Weapon, float DeltaTime, float Scale, FLinearColor Color);

	/**This is used to draw the preview in the menus
	*  Note: There is no player or HUD when this is called
	*/
	UFUNCTION(BlueprintNativeEvent)
	void DrawPreviewCrosshair(UCanvas* Canvas, AUTWeapon* Weapon, float DeltaTime, float Scale, FLinearColor Color);
};
