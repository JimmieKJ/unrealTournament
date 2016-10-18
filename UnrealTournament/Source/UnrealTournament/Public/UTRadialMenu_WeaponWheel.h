// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget.h"
#include "UTRadialMenu.h"
#include "UTRadialMenu_WeaponWheel.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTRadialMenu_WeaponWheel : public UUTRadialMenu
{
	GENERATED_UCLASS_BODY()

public:
	virtual void InitializeWidget(AUTHUD* Hud);
	virtual void BecomeInteractive();
	virtual void BecomeNonInteractive();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture OutOfAmmoTemplate;

protected:

	UPROPERTY()
	TArray<AUTWeapon*> WeaponList;

	virtual void Execute();
	virtual void DrawMenu(FVector2D ScreenCenter, float RenderDelta);
	virtual void DrawCursor(FVector2D ScreenCenter, float RenderDelta);
	
	virtual bool ShouldDraw_Implementation(bool bShowScores)
	{
		return UTHUDOwner && UTHUDOwner->bShowWeaponWheel;
	}

	bool bActiveSlotOutOfAmmo;

};

