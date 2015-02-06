// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PaperRuntimeSettings.generated.h"

/**
 * Implements the settings for the Paper2D plugin.
 */
UCLASS(config = Engine, defaultconfig)
class PAPER2D_API UPaperRuntimeSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	// The default scaling factor between pixels and Unreal units (cm) to use for newly created sprite assets (e.g., 0.64 would make a 64 pixel wide sprite take up 100 cm)
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Settings)
	float DefaultPixelsPerUnrealUnit;

#if WITH_EDITORONLY_DATA
	// Should the source texture be scanned when creating new sprites to determine the appropriate material? (if false, the Default Masked Material is always used)
	UPROPERTY(config, EditAnywhere, Category=Settings)
	bool bPickBestMaterialWhenCreatingSprite;

	// The default masked material for newly created sprites (masked means binary opacity: things are either opaque or see-thru, with nothing in between)
	UPROPERTY(config, EditAnywhere, Category=Settings, meta=(AllowedClasses="MaterialInterface", DisplayName="Default Masked Material"))
	FStringAssetReference DefaultMaskedMaterialName;

	// The default translucent material for newly created sprites (translucent means smooth opacity which can vary continuously from 0..1, but translucent rendering is more expensive that opaque or masked rendering and has different sorting rules)
	UPROPERTY(config, EditAnywhere, Category=Settings, meta=(AllowedClasses="MaterialInterface", DisplayName="Default Translucent Material", EditCondition="bPickBestMaterialWhenCreatingSprite"))
	FStringAssetReference DefaultTranslucentMaterialName;

	// The default opaque material for newly created sprites
	UPROPERTY(config, EditAnywhere, Category=Settings, meta=(AllowedClasses="MaterialInterface", DisplayName="Default Opaque Sprite Material"))
	FStringAssetReference DefaultOpaqueMaterialName;
#endif

	// Enables experimental *incomplete and unsupported* texture atlas groups that sprites can be assigned to
	UPROPERTY(EditAnywhere, config, Category=Experimental)
	bool bEnableSpriteAtlasGroups;

	// Enables experimental *incomplete and unsupported* 2D terrain spline editing. Note: You need to restart the editor when enabling this setting for the change to fully take effect.
	UPROPERTY(EditAnywhere, config, Category=Experimental)
	bool bEnableTerrainSplineEditing;
};
