// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Object.h"
#include "UTUMGHudWidget_Crosshair.h"
#include "UTCrosshair.generated.h"


/**
 * 
 */
UCLASS(Blueprintable)
class UNREALTOURNAMENT_API UUTCrosshair : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	// The name tag of this crosshair.  All weapons will have a tag that is used to determine what crosshair to use
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Crosshair)
	FName CrosshairTag;

	/** 
	 *  Called from the HUD_Widget when it has detected that the player's weapon has changed. This is called on the new crosshair.  It's here that
	 *  we create and UMG widget associated with this crosshair and make sure they are in view
	 **/
	UFUNCTION(BlueprintNativeEvent, Category = Crosshair)
	void ActivateCrosshair(AUTHUD* TargetHUD, const FWeaponCustomizationInfo& CustomizationsToApply, AUTWeapon* Weapon);

	/** 
	 * Called from the HUD_Widget when it has detected that the player's weapon has changed.  This is called on the previously active crosshair 
	 * and is where we should remove any umg.
	 **/
	UFUNCTION(BlueprintNativeEvent, Category = Crosshair)
	void DeactivateCrosshair(AUTHUD* TargetHUD);

	/**
	 *	This is the starting point for drawing a crosshair.  It checks to see if a UMG version of the crosshair is being rendered and if
	 *  it is, quickly exists.  Otherwise it passes the call to DrawCrosshair which is blueprint implementable.
	 **/
	UFUNCTION()
	void NativeDrawCrosshair(AUTHUD* TargetHUD, UCanvas* Canvas, AUTWeapon* Weapon, float DeltaTime, const FWeaponCustomizationInfo& CustomizationsToApply);

	UFUNCTION(BlueprintNativeEvent)
	void DrawCrosshair(AUTHUD* TargetHUD, UCanvas* Canvas, AUTWeapon* Weapon, float DeltaTime, const FWeaponCustomizationInfo& CustomizationsToApply);

	// Holds the name of the UMG class to instance when using this crosshair
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Crosshair)
	FString UMGClassname;

	// If there is an active UMG scene assoicated with the current crosshair, this will hold the reference
	TWeakObjectPtr<UUTUMGHudWidget_Crosshair> ActiveUMG;

	// This is the canvas icon that defines the crosshair.  NOTE: If a UMGClassname is included, then for now, this icon will only be used
	// in the menus to display what the crosshair should look like.  The UMG file referenced by will be displayed in game.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Crosshair)
	FCanvasIcon CrosshairIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Crosshair)
	FText CrosshairName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Crosshair)
	FVector2D OffsetAdjust;
};
