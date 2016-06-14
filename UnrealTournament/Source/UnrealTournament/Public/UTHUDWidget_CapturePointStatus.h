// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFCapturePoint.h"
#include "UTHUDWidget_CapturePointStatus.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_CapturePointStatus : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture UnlockedPointBackground;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture UnlockedPointFillTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture UnlockedPointLockedPip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture LockedPointTexture;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float BarWidth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float BarHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float UnlockPipOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FVector UnlockedPointStartingScreenPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FVector AdditionalUnlockedPointScreenPositionOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FVector LockedPointStartingScreenPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FVector AdditionalLockedPointScreenPositionOffset;

	virtual void InitializeWidget(AUTHUD* Hud);
	virtual bool ShouldDraw_Implementation(bool bShowScores)
	{
		return !bShowScores;
	}

	virtual void PreDraw(float DeltaTime, AUTHUD* InUTHUDOwner, UCanvas* InCanvas, FVector2D InCanvasCenter) override;
	virtual void Draw_Implementation(float DeltaTime) override;
	virtual void PostDraw(float RenderedTime) override;

protected:
	
	int32 NumUnlockedPoints;
	int32 NumLockedPoints;

	void DrawUnlockedCapturePoint(AUTCTFCapturePoint* CapturePoint);
	void DrawLockedCapturePoint(AUTCTFCapturePoint* CapturePoint);
	
	UPROPERTY()
	TArray<AUTCTFCapturePoint*> CapturePoints;
};