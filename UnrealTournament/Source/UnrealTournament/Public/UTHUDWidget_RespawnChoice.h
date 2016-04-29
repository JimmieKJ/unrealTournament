// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUDWidget_RespawnChoice.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_RespawnChoice : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

	virtual void Draw_Implementation(float DeltaTime) override;
	virtual void InitializeWidget(AUTHUD* Hud) override;
	virtual bool ShouldDraw_Implementation(bool bShowScores) override;

protected:
	UPROPERTY()
	class USceneCaptureComponent2D* RespawnChoiceACaptureComponent;

	UPROPERTY()
	class USceneCaptureComponent2D* RespawnChoiceBCaptureComponent;

	UPROPERTY()
	float LastRespawnCaptureTime;

	UPROPERTY()
	bool bHasValidRespawnCapture;
};