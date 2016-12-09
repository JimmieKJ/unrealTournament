// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/StringAssetReference.h"
#include "Factories/FbxAssetImportData.h"
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

	/** Base material to instance from when importing materials. */
	UPROPERTY(EditAnywhere, config, Category = Material, meta = (ImportType = "Mesh", AllowedClasses = "MaterialInterface"))
	FStringAssetReference BaseMaterialName;

	UPROPERTY(config, meta = (ImportType = "Mesh"))
	FString BaseColorName;

	UPROPERTY(config, meta = (ImportType = "Mesh"))
	FString BaseDiffuseTextureName;

	UPROPERTY(config, meta = (ImportType = "Mesh"))
	FString BaseNormalTextureName;

	UPROPERTY(config, meta = (ImportType = "Mesh"))
	FString BaseEmissiveColorName;

	UPROPERTY(config, meta = (ImportType = "Mesh"))
	FString BaseEmmisiveTextureName;

	UPROPERTY(config, meta = (ImportType = "Mesh"))
	FString BaseSpecularTextureName;

	bool CanEditChange( const UProperty* InProperty ) const override;
};
