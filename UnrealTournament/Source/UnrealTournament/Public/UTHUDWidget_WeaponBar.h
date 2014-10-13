// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 **/

#include "UTHUDWidget_WeaponBar.generated.h"

UCLASS()
class UUTHUDWidget_WeaponBar : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY()
	UTexture* OldHudTexture;

	UPROPERTY()
	UTexture* OldWeaponTexture;
	
	UPROPERTY()
	float WeaponScaleSpeed;

	UPROPERTY()
	float CellWidth;

	UPROPERTY()
	float CellHeight;

	UPROPERTY()
	FVector2D AmmoBarOffset;

	UPROPERTY()
	FVector2D AmmoBarSize;

	UPROPERTY()
	FVector2D SlotOffset;

	UPROPERTY()
	FVector2D MaxIconSize;

	UPROPERTY()
	FText SelectedWeaponDisplayName;

	UPROPERTY()
	float SelectedWeaponDisplayTime;

	virtual void Draw_Implementation(float DeltaTime);

protected:

private:

	int32 LastSelectedGroup;		// Holds the group of the last selected weapon
	int32 BouncedWeapon;			// Used to bounce the animation slightly
	float CurrentWeaponScale[10];	// Holds the scaling for each weapon

	float SelectedWeaponScale;		// How big should the selected weapon be
	float BounceWeaponScale;		// How big should it grow too before bouncing back

	float CurrentSelectedWeaponDisplayTime;

};
