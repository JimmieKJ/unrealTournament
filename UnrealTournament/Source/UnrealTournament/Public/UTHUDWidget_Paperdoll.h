// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 **/

#include "UTHUDWidget_Paperdoll.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_Paperdoll : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:

	virtual bool ShouldDraw_Implementation(bool bShowScores) override;
	virtual void Draw_Implementation(float DeltaTime) override;
	virtual void InitializeWidget(AUTHUD* Hud) override;

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture BackgroundSlate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture BackgroundBorder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture HealthIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture ArmorIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture PaperDollBase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture PaperDoll_ShieldBeltOverlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture PaperDoll_ChestArmorOverlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture PaperDoll_ThighPadArmorOverlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture PaperDoll_BootsOverlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture PaperDoll_HelmetOverlay;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text HealthText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text ArmorText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float HealthFlashTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FLinearColor HealthPositiveFlashColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FLinearColor HealthNegativeFlashColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float ArmorFlashTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FLinearColor ArmorPositiveFlashColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FLinearColor ArmorNegativeFlashColor;


	UFUNCTION(BlueprintNativeEvent, Category = "RenderObject")
	FText GetPlayerHealth();

	UFUNCTION(BlueprintNativeEvent, Category = "RenderObject")
	FText GetPlayerArmor();

	UFUNCTION(BlueprintCallable, Category = "PaperDoll")
	virtual void ProcessArmor();



	int32 PlayerArmor;


private:
	int32 LastHealth;
	float HealthFlashTimer;

	int32 LastArmor;
	float ArmorFlashTimer;

};
