// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUDWidget_GameClock.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_GameClock : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

	virtual void Draw_Implementation(float DeltaTime);
	virtual void InitializeWidget(AUTHUD* Hud);

	virtual bool ShouldDraw_Implementation(bool bShowScores)
	{
		return !bShowScores;
	}

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture BackgroundSlate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture BackgroundBorder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture Skull;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture ClockBackground;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text PlayerScoreText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text ClockText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text PlayerRankText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text PlayerRankThText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text NumPlayersText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture GameStateBackground;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text GameStateText;

	// The scale factor to use on the clock when it has to show hours
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float AltClockScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
		FText NumPlayersDisplay;

	UFUNCTION(BlueprintNativeEvent, Category = "RenderObject")
	FText GetPlayerScoreText();

	UFUNCTION(BlueprintNativeEvent, Category = "RenderObject")
	FText GetClockText();

	UFUNCTION(BlueprintNativeEvent, Category = "RenderObject")
	FText GetPlayerRankText();

	UFUNCTION(BlueprintNativeEvent, Category = "RenderObject")
	FText GetPlayerRankThText();

	UFUNCTION(BlueprintNativeEvent, Category = "RenderObject")
	FText GetNumPlayersText();

private:


};