// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTHUD_TeamDM.h"

#include "UTHUD_Showdown.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTHUD_Showdown : public AUTHUD_TeamDM
{
	GENERATED_UCLASS_BODY()

	/** render target for the minimap */
	UPROPERTY()
	UCanvasRenderTarget2D* MinimapTexture;
	/** transformation matrix from world locations to minimap locations */
	FMatrix MinimapTransform;
	/** map transform for rendering on screen (used to convert clicks to map locations) */
	FMatrix MapToScreen;

	/** icon for player starts on the minimap */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
	UTexture2D* PlayerStartTexture;
	/** drawn over selected player starts on the minimap */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, NoClear)
	UTexture2D* SelectedSpawnTexture;

	/** draw the static pre-rendered portions of the minimap to the MinimapTexture */
	UFUNCTION()
	virtual void UpdateMinimapTexture(UCanvas* C, int32 Width, int32 Height);

	/** transform InPos to cordinates corresponding to the map's position on the screen, i.e. transform world -> map then map -> screen */
	FVector2D WorldToMapToScreen(const FVector& InPos) const
	{
		return FVector2D(MapToScreen.TransformPosition(MinimapTransform.TransformPosition(InPos)));
	}

	virtual EInputMode::Type GetInputMode_Implementation();
	virtual bool OverrideMouseClick(FKey Key, EInputEvent EventType) override;

	virtual void BeginPlay() override;
	virtual void DrawHUD() override;
};