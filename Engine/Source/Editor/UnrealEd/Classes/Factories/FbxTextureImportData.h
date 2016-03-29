// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "FbxTextureImportData.generated.h"

/**
 * Import data and options used when importing any mesh from FBX
 */
UCLASS(AutoExpandCategories=(Texture))
class UNREALED_API UFbxTextureImportData : public UFbxAssetImportData
{
	GENERATED_UCLASS_BODY()

	/** If importing textures is enabled, this option will cause normal map Y (Green) values to be inverted */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category=ImportSettings, meta=(OBJRestrict="true"))
	uint32 bInvertNormalMaps:1;

	bool CanEditChange( const UProperty* InProperty ) const override;
};
