// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUDWidget_QuickStats.generated.h"

class AUTWeapon;

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_QuickStats : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

	virtual void InitializeWidget(AUTHUD* Hud);
	virtual void Draw_Implementation(float DeltaTime) override;
	virtual void PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter);

	virtual bool ShouldDraw_Implementation(bool bShowScores);


protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	FHUDRenderObject_Texture HealthIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	FHUDRenderObject_Texture HealthBorder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	FHUDRenderObject_Texture HealthBackground;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	FHUDRenderObject_Text HealthText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	FHUDRenderObject_Texture ArmorIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	FHUDRenderObject_Texture ArmorBorder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	FHUDRenderObject_Texture ArmorBackground;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	FHUDRenderObject_Text ArmorText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	FHUDRenderObject_Texture AmmoIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	FHUDRenderObject_Texture AmmoBorder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	FHUDRenderObject_Texture AmmoBackground;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	FHUDRenderObject_Text AmmoText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boots")
	FHUDRenderObject_Texture BootsIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boots")
	FHUDRenderObject_Texture BootsBorder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boots")
	FHUDRenderObject_Texture BootsBackground;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boots")
	FHUDRenderObject_Text BootsText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flag")
	FHUDRenderObject_Texture FlagIcon;


	UFUNCTION(BlueprintNativeEvent, Category = "RenderObject")
	FText GetPlayerHealthText();

	UFUNCTION(BlueprintNativeEvent, Category = "RenderObject")
	FText GetPlayerArmorText();

	UFUNCTION(BlueprintNativeEvent, Category = "RenderObject")
	FText GetPlayerAmmoText();

	UFUNCTION(BlueprintNativeEvent, Category = "RenderObject")
	FText GetPlayerBootsText();

private:
	int32 PlayerArmor;
	int32 PlayerHealth;
	int32 PlayerAmmo;
	int32 PlayerBoots;

	AUTWeapon* LastWeapon;

	int32 LastAmmoAmount;
	int32 LastArmorAmount;
	int32 LastHealthAmount;
	int32 LastBootsAmount;

	FLinearColor GetFlashColor(float Delta);
	float GetFlashOpacity(float Delta);

};