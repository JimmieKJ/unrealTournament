// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUDWidget_TeamLives.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_TeamLives : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

	virtual void InitializeWidget(AUTHUD* Hud);
	virtual bool ShouldDraw_Implementation(bool bShowScores);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture RedTeamIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text RedTeamCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture BlueTeamIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text BlueTeamCount;

	UFUNCTION(BlueprintNativeEvent, Category = "RenderObject")
	FText GetRedTeamLives();

	UFUNCTION(BlueprintNativeEvent, Category = "RenderObject")
	FText GetBlueTeamLives();

private:


};