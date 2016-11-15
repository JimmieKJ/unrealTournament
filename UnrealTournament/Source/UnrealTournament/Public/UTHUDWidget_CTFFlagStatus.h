// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFGameState.h"
#include "UTHUDWidget_CTFFlagStatus.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_CTFFlagStatus : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

	// The text that will be displayed if you have the enemy flag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText YouHaveFlagText;

	// The text that will be displayed if you have the team flag, taking it to enemy base.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
		FText YouHaveFlagTextAlt;

	// The text that will be displayed if the enemy has your flag
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText EnemyHasFlagText;

	// The text that will be displayed if both your flag is out and you have an enemy flag
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText BothFlagsText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Message")
	float AnimationAlpha;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture CircleTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture CircleBorderTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text FlagHolderNameTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture FlagIconTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture DroppedIconTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture TakenIconTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
		FHUDRenderObject_Texture FlagGoneIconTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
		FHUDRenderObject_Texture CameraIconTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text FlagStatusText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
		FHUDRenderObject_Texture ArrowTemplate;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Message")
	float InWorldAlpha;

	/** Transient value used to pulse color of status indicators. */
	UPROPERTY(BlueprintReadWrite, Category = "WorldOverlay")
	float StatusScale;

	UPROPERTY(BlueprintReadWrite, Category = "WorldOverlay")
	float EnemyFlagStartDrawTime;

	UPROPERTY(BlueprintReadWrite, Category = "WorldOverlay")
	bool bEnemyFlagWasDrawn;

	/** Transient value used to pulse color of status indicators. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	bool bStatusDir;

	/** Transient value used to keep offscreen indicators from flipping sides too much. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	bool bRedWasLeft;

	/** Transient value used to keep offscreen indicators from flipping sides too much. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	bool bBlueWasLeft;

	/** Largest scaling for in world indicators. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	float MaxIconScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FVector2D> TeamPositions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text RallyText;

	UPROPERTY(BlueprintReadWrite, Category = "RenderObject")
		bool bAlwaysDrawFlagHolderName;

	UPROPERTY()
		FName OldFlagState[2];

	UPROPERTY()
		float StatusChangedScale;

	UPROPERTY()
		float CurrentStatusScale[2];

	UPROPERTY()
		float ScaleDownTime;

	virtual void Draw_Implementation(float DeltaTime);
	virtual void InitializeWidget(AUTHUD* Hud);

	virtual bool ShouldDraw_Implementation(bool bShowScores)
	{
		return !bShowScores && UTGameState && UTGameState->HasMatchStarted() && !UTGameState->HasMatchEnded() && (UTGameState->GetMatchState() != MatchState::MatchIntermission);
	}

	bool bSuppressMessage;

protected:
	virtual void DrawStatusMessage(float DeltaTime);
	virtual void DrawIndicators(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, float DeltaTime);
	virtual void DrawFlagStatus(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, uint8 TeamNum, FVector2D IndicatorPosition, AUTCTFFlagBase* FlagBase, AUTCTFFlag* Flag, AUTPlayerState* FlagHolder);
	virtual void DrawFlagWorld(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, uint8 TeamNum, AUTCTFFlagBase* FlagBase, AUTCTFFlag* Flag, AUTPlayerState* FlagHolder);
	virtual void DrawFlagBaseWorld(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, uint8 TeamNum, AUTCTFFlagBase* FlagBase, AUTCTFFlag* Flag, AUTPlayerState* FlagHolder);
	virtual FText GetFlagReturnTime(AUTCTFFlag* Flag);
	virtual FVector GetAdjustedScreenPosition(const FVector& WorldPosition, const FVector& ViewPoint, const FVector& ViewDir, float Dist, float Edge, bool& bDrawEdgeArrow, int32 Team);
	virtual void DrawEdgeArrow(FVector InWorldPosition, FVector ScreenPosition, float CurrentWorldAlpha, float WorldRenderScale, int32 Team);
	virtual FText GetBaseMessage(AUTCTFFlagBase* Base, AUTCTFFlag* Flag);
	virtual bool ShouldDrawFlag(AUTCTFFlag* Flag, bool bIsEnemyFlag);
};