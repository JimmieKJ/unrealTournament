// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUDWidget_CTFFlagStatus.h"
#include "UTHUDWidget_FlagRunStatus.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_FlagRunStatus : public UUTHUDWidget_CTFFlagStatus
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
		FHUDRenderObject_Texture RedTeamIconTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
		FHUDRenderObject_Texture BlueTeamIconTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
		float LineGlow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
		float NormalLineBrightness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
		float PulseLength;

	UPROPERTY()
		float LastFlagStatusChange;

	UPROPERTY()
		FName LastFlagStatus;

	virtual void DrawStatusMessage(float DeltaTime) override;
	virtual bool ShouldDraw_Implementation(bool bShowScores) override;

protected:
	virtual void DrawIndicators(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, float DeltaTime) override;
	virtual bool ShouldDrawFlag(AUTCTFFlag* Flag, bool bIsEnemyFlag) override;
	virtual void DrawFlagWorld(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, uint8 TeamNum, AUTCTFFlagBase* FlagBase, AUTCTFFlag* Flag, AUTPlayerState* FlagHolder) override;
	virtual void DrawFlagBaseWorld(AUTCTFGameState* GameState, FVector PlayerViewPoint, FRotator PlayerViewRotation, uint8 TeamNum, AUTCTFFlagBase* FlagBase, AUTCTFFlag* Flag, AUTPlayerState* FlagHolder) override;
};