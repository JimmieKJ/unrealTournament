// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUDWidget_CTFFlagStatus.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_CTFFlagStatus : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

	// The text that will be displayed if you have the flag
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText YouHaveFlagText;

	// The text that will be displayed if the enemy has your flag
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText EnemyHasFlagText;

	// The text that will be displayed if both your flag is out and you have an enemy flag
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText BothFlagsText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Message")
	float AnimationAlpha;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FHUDRenderObject_Texture> CircleSlate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FHUDRenderObject_Texture> CircleBorder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture FlagIconTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture DroppedIconTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Texture TakenIconTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
		FHUDRenderObject_Texture FlagGoneIconTemplate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text FlagStatusText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FHUDRenderObject_Text> FlagHolderNames;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Message")
		float InWorldAlpha;

	/** Transient value used to pulse color of status indicators. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldOverlay")
		float StatusScale;

	/** Transient value used to pulse color of status indicators. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
		bool bStatusDir;

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

	virtual void Draw_Implementation(float DeltaTime);
	virtual void InitializeWidget(AUTHUD* Hud);

	virtual bool ShouldDraw_Implementation(bool bShowScores)
	{
		return !bShowScores;
	}


};