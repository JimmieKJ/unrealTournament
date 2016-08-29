// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreUObject.h"
#include "MaterialMerging.generated.h"

// Use FMaterialProxySettings instead
USTRUCT()
struct FMaterialSimplificationSettings
{
	GENERATED_USTRUCT_BODY()

		// Size of generated BaseColor map
		UPROPERTY(Category = Material, EditAnywhere)
		FIntPoint BaseColorMapSize;

	// Whether to generate normal map
	UPROPERTY(Category = Material, EditAnywhere, meta = (InlineEditConditionToggle))
		bool bNormalMap;

	// Size of generated specular map
	UPROPERTY(Category = Material, EditAnywhere, meta = (editcondition = "bNormalMap"))
		FIntPoint NormalMapSize;

	// Metallic constant
	UPROPERTY(Category = Material, EditAnywhere, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
		float MetallicConstant;

	// Whether to generate metallic map
	UPROPERTY(Category = Material, EditAnywhere, meta = (InlineEditConditionToggle))
		bool bMetallicMap;

	// Size of generated metallic map
	UPROPERTY(Category = Material, EditAnywhere, meta = (editcondition = "bMetallicMap"))
		FIntPoint MetallicMapSize;

	// Roughness constant
	UPROPERTY(Category = Material, EditAnywhere, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
		float RoughnessConstant;

	// Whether to generate roughness map
	UPROPERTY(Category = Material, EditAnywhere, meta = (InlineEditConditionToggle))
		bool bRoughnessMap;

	// Size of generated roughness map
	UPROPERTY(Category = Material, EditAnywhere, meta = (editcondition = "bRoughnessMap"))
		FIntPoint RoughnessMapSize;

	// Specular constant
	UPROPERTY(Category = Material, EditAnywhere, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
		float SpecularConstant;

	// Whether to generate specular map
	UPROPERTY(Category = Material, EditAnywhere, meta = (InlineEditConditionToggle))
		bool bSpecularMap;

	// Size of generated specular map
	UPROPERTY(Category = Material, EditAnywhere, meta = (editcondition = "bSpecularMap"))
		FIntPoint SpecularMapSize;

	FMaterialSimplificationSettings()
		: BaseColorMapSize(1024, 1024)
		, bNormalMap(true)
		, NormalMapSize(1024, 1024)
		, MetallicConstant(0.0f)
		, bMetallicMap(false)
		, MetallicMapSize(1024, 1024)
		, RoughnessConstant(0.5f)
		, bRoughnessMap(false)
		, RoughnessMapSize(1024, 1024)
		, SpecularConstant(0.5f)
		, bSpecularMap(false)
		, SpecularMapSize(1024, 1024)
	{
	}

	bool operator == (const FMaterialSimplificationSettings& Other) const
	{
		return BaseColorMapSize == Other.BaseColorMapSize
			&& bNormalMap == Other.bNormalMap
			&& NormalMapSize == Other.NormalMapSize
			&& MetallicConstant == Other.MetallicConstant
			&& bMetallicMap == Other.bMetallicMap
			&& MetallicMapSize == Other.MetallicMapSize
			&& RoughnessConstant == Other.RoughnessConstant
			&& bRoughnessMap == Other.bRoughnessMap
			&& RoughnessMapSize == Other.RoughnessMapSize
			&& SpecularConstant == Other.SpecularConstant
			&& bSpecularMap == Other.bSpecularMap
			&& SpecularMapSize == Other.SpecularMapSize;
	}
};

UENUM()
enum ETextureSizingType
{
	TextureSizingType_UseSingleTextureSize UMETA(DisplayName = "Use TextureSize for all material properties"),
	TextureSizingType_UseAutomaticBiasedSizes UMETA(DisplayName = "Use automatically biased texture sizes based on TextureSize"),
	TextureSizingType_UseManualOverrideTextureSize UMETA(DisplayName = "Use per property manually overriden texture sizes"),
	TextureSizingType_UseSimplygonAutomaticSizing UMETA(DisplayName = "Use Simplygon's automatic texture sizing"),
	TextureSizingType_MAX,
};

UENUM()
enum EMaterialMergeType
{
	MaterialMergeType_Default,
	MaterialMergeType_Simplygon
};

USTRUCT()
struct FMaterialProxySettings
{
	GENERATED_USTRUCT_BODY()

	// Size of generated BaseColor map
	UPROPERTY(Category = Material, EditAnywhere)
	FIntPoint TextureSize;

	UPROPERTY(Category = Material, EditAnywhere)
	TEnumAsByte<ETextureSizingType> TextureSizingType;

	UPROPERTY(Category = Material, AdvancedDisplay, EditAnywhere)
	float GutterSpace;

	// Whether to generate normal map
	UPROPERTY(Category = Material, EditAnywhere)
		bool bNormalMap;

	// Whether to generate metallic map
	UPROPERTY(Category = Material, EditAnywhere)
		bool bMetallicMap;

	// Metallic constant
	UPROPERTY(Category = Material, EditAnywhere, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1", editcondition = "!bMetallicMap"))
		float MetallicConstant;

	// Whether to generate roughness map
	UPROPERTY(Category = Material, EditAnywhere)
		bool bRoughnessMap;

	// Roughness constant
	UPROPERTY(Category = Material, EditAnywhere, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1", editcondition = "!bRoughnessMap"))
		float RoughnessConstant;

	// Whether to generate specular map
	UPROPERTY(Category = Material, EditAnywhere)
		bool bSpecularMap;

	// Specular constant
	UPROPERTY(Category = Material, EditAnywhere, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1", editcondition = "!bSpecularMap"))
		float SpecularConstant;

	// Whether to generate emissive map
	UPROPERTY(Category = Material, EditAnywhere)
		bool bEmissiveMap;

	// Whether to generate opacity map
	UPROPERTY()
	bool bOpacityMap;

	// Override diffuse map size
	UPROPERTY(Category = Material, AdvancedDisplay, EditAnywhere)
	FIntPoint DiffuseTextureSize;

	// Override normal map size
	UPROPERTY(Category = Material, AdvancedDisplay, EditAnywhere)
	FIntPoint NormalTextureSize;

	// Override metallic map size
	UPROPERTY(Category = Material, AdvancedDisplay, EditAnywhere)
	FIntPoint MetallicTextureSize;

	// Override roughness map size
	UPROPERTY(Category = Material, AdvancedDisplay, EditAnywhere)
	FIntPoint RoughnessTextureSize;

	// Override specular map size
	UPROPERTY(Category = Material, AdvancedDisplay, EditAnywhere)
	FIntPoint SpecularTextureSize;

	// Override emissive map size
	UPROPERTY(Category = Material, AdvancedDisplay, EditAnywhere)
	FIntPoint EmissiveTextureSize;

	// Override opacity map size
	UPROPERTY()
	FIntPoint OpacityTextureSize;

	UPROPERTY()
	TEnumAsByte<EMaterialMergeType> MaterialMergeType;

	FMaterialProxySettings()
		: TextureSize(1024, 1024)
		, TextureSizingType(TextureSizingType_UseSingleTextureSize)
		, GutterSpace(4.0f)
		, bNormalMap(true)
		, bMetallicMap(false)
		, MetallicConstant(0.0f)
		, bRoughnessMap(false)
		, RoughnessConstant(0.5f)
		, bSpecularMap(false)
		, SpecularConstant(0.5f)
		, bEmissiveMap(false)
		, bOpacityMap(false)
		, DiffuseTextureSize(1024, 1024)
		, NormalTextureSize(1024, 1024)
		, MetallicTextureSize(1024, 1024)
		, RoughnessTextureSize(1024, 1024)
		, EmissiveTextureSize(1024, 1024)
		, OpacityTextureSize(1024, 1024)
		, MaterialMergeType( EMaterialMergeType::MaterialMergeType_Default )
	{
	}

	bool operator == (const FMaterialProxySettings& Other) const
	{
		return TextureSize == Other.TextureSize
			&& TextureSizingType == Other.TextureSizingType
			&& GutterSpace == Other.GutterSpace
			&& bNormalMap == Other.bNormalMap
			&& MetallicConstant == Other.MetallicConstant
			&& bMetallicMap == Other.bMetallicMap
			&& RoughnessConstant == Other.RoughnessConstant
			&& bRoughnessMap == Other.bRoughnessMap
			&& SpecularConstant == Other.SpecularConstant
			&& bSpecularMap == Other.bSpecularMap
			&& bEmissiveMap == Other.bEmissiveMap
			&& bOpacityMap == Other.bOpacityMap
			&& DiffuseTextureSize == Other.DiffuseTextureSize
			&& NormalTextureSize == Other.NormalTextureSize
			&& MetallicTextureSize == Other.MetallicTextureSize
			&& RoughnessTextureSize == Other.RoughnessTextureSize
			&& EmissiveTextureSize == Other.EmissiveTextureSize
			&& OpacityTextureSize == Other.OpacityTextureSize;
	}
};
