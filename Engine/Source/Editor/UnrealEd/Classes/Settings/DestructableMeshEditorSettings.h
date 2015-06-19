// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DestructableMeshEditorSettings.h: Declares the UDestructableMeshEditorSettings class.
=============================================================================*/

#pragma once


#include "DestructableMeshEditorSettings.generated.h"


/**
 * Implements the settings for the destructable mesh editor.
 */
UCLASS(config=EditorPerProjectUserSettings)
class UNREALED_API UDestructableMeshEditorSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, config, Category=AnimationPreview, meta=(DisplayName="Viewport Floor Color"))
	FColor AnimPreviewFloorColor;

	UPROPERTY(EditAnywhere, config, Category=AnimationPreview, meta=(DisplayName="Viewport Sky Color"))
	FColor AnimPreviewSkyColor;

	UPROPERTY(EditAnywhere, config, Category=AnimationPreview, meta=(DisplayName="Viewport Sky Brightness"))
	float AnimPreviewSkyBrightness;

	UPROPERTY(EditAnywhere, config, Category=AnimationPreview, meta=(DisplayName="Viewport Light Brightness"))
	float AnimPreviewLightBrightness;

	UPROPERTY(EditAnywhere, config, Category=AnimationPreview, meta=(DisplayName="Viewport Lighting Direction"))
	FRotator AnimPreviewLightingDirection;

	UPROPERTY(EditAnywhere, config, Category=AnimationPreview, meta=(DisplayName="Viewport Directional Color"))
	FColor AnimPreviewDirectionalColor;
};
