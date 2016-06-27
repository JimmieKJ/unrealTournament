// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFGameState.h"
#include "UTHUDWidget_DetectedWarning.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_DetectedWarning : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

	// The text that will be displayed if you are visible to the enemy team by Tac Com.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText DetectedText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Message")
	float AnimationAlpha;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture WarningIconTemplate;

	/** Transient value used to keep offscreen indicators from flipping sides too much. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	bool bWasLeftRecently;

	/** Distance to start scaling in world indicators. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float ScalingStartDist;

	/** Distance to stop scaling in world indicators. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float ScalingEndDist;

	/** Largest scaling for in world indicators. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float MaxIconScale;

	/** Smallest scaling for in world indicators. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float MinIconScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float EdgeOffsetPercentX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float MinEdgeOffsetPercentY;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float MaxEdgeOffsetPercentY;

	UPROPERTY(EditAnywhere, BlueprintreadWrite, Category = "RenderObject")
	float MinPulseRenderScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float PulseSpeed;

	virtual void Draw_Implementation(float DeltaTime);
	virtual void InitializeWidget(AUTHUD* Hud);
	virtual bool ShouldDraw_Implementation(bool bShowScores)
	{
			return !bShowScores;
	}

	bool bSuppressMessage;

protected:
	virtual void DrawDetectedNoticeWorld(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation);
	virtual void DrawDetectedCameraNotice(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, const UUTSecurityCameraComponent* DetectingCamera);
	
	virtual void AdjustScreenPosition(/*OUT*/ FVector& ScreenPosition, const FVector& WorldPosition, const FVector& ViewDir, const FVector& CameraDir);
	virtual void AdjustRenderScaleForAnimation();

	bool IsLocalPlayerFlagCarrier() const;
};