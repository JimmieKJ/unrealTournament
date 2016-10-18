// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UserWidget.h"
#include "AssetData.h"
#include "UTUMGHudWidget.h"
#include "UTUMGHudWidget_Crosshair.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTUMGHudWidget_Crosshair : public UUTUMGHudWidget
{
	GENERATED_UCLASS_BODY()

	/** Called from the HUD_Widget when it has detected that the player's weapon has changed. This is called on the new crosshair */
	UFUNCTION(BlueprintNativeEvent, Category = Crosshair)
	void ApplyCustomizations(const FWeaponCustomizationInfo& CustomizationsToApply);

protected:

	/** Holds the current customizations to apply to this crosshair. */
	UPROPERTY(BlueprintReadOnly, Category = Crosshair)
	FWeaponCustomizationInfo Customizations;

public:
	UPROPERTY(BlueprintReadOnly, Category = Crosshair)
	TWeakObjectPtr<AUTWeapon> AssociatedWeapon;


};
