// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUDWidget_CTFFlagStatus.generated.h"

UCLASS()
class UUTHUDWidget_CTFFlagStatus : public UUTHUDWidget
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
	FHUDRenderObject_Texture FlagArrowTemplate;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	FHUDRenderObject_Text FlagStatusText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
	TArray<FHUDRenderObject_Text> FlagHolderNames;

	/** Transient value used to pulse color of status indicators. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
		float StatusScale;

	/** Transient value used to pulse color of status indicators. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RenderObject")
		bool bStatusDir;

	virtual void Draw_Implementation(float DeltaTime);
	virtual void InitializeWidget(AUTHUD* Hud);

	virtual bool ShouldDraw_Implementation(bool bShowScores)
	{
		return !bShowScores;
	}


};