// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget_Paperdoll.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_Paperdoll : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual bool ShouldDraw_Implementation(bool bShowScores) override;
	virtual void PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter) override;
	virtual void Draw_Implementation(float DeltaTime) override;
	virtual void InitializeWidget(AUTHUD* Hud) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture HealthBackground;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture ArmorBackground;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture FlagBackground;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture ShieldOverlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture HealthIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture ArmorIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture FlagIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text HealthText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text ArmorText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text FlagText;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FTextureUVs FlagHolderIconUVs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FTextureUVs RallyIconUVs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icons")
		FHUDRenderObject_Texture DetectedIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icons")
		FHUDRenderObject_Texture CombatIcon;

	int32 PlayerArmor;
	
	bool bAnimating;
	bool bShowFlagInfo;

	FVector2D DrawOffset;
	float DrawOffsetTransitionTime;

	void DrawRallyIcon(float DeltaTime, class AUTRallyPoint* RallyPoint);

	TArray<float> RallyAnimTimers;

private:
	int32 LastHealth;
	float HealthFlashTimer;

	int32 LastArmor;
	float ArmorFlashTimer;
};
