// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "FbxMeshImportData.generated.h"

UENUM()
enum EFBXNormalImportMethod
{
	FBXNIM_ComputeNormals UMETA(DisplayName="Compute Normals"),
	FBXNIM_ImportNormals UMETA(DisplayName="Import Normals"),
	FBXNIM_ImportNormalsAndTangents UMETA(DisplayName="Import Normals and Tangents"),
	FBXNIM_MAX,
};

/**
 * Import data and options used when importing any mesh from FBX
 */
UCLASS(config=EditorPerProjectUserSettings, configdonotcheckdefaults, abstract)
class UFbxMeshImportData : public UFbxAssetImportData
{
	GENERATED_UCLASS_BODY()

	/** Enables importing of mesh LODs from FBX LOD groups, if present in the FBX file */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category=ImportSettings, meta=(OBJRestrict="true", ImportType="Mesh", ToolTip="If enabled, creates LOD models for Unreal meshes from LODs in the import file; If not enabled, only the base mesh from the LOD group is imported"))
	uint32 bImportMeshLODs:1;

	/** Enabling this option will read the tangents(tangent,binormal,normal) from FBX file instead of generating them automatically. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category=ImportSettings, meta=(ImportType="Mesh"))
	TEnumAsByte<enum EFBXNormalImportMethod> NormalImportMethod;

	bool CanEditChange( const UProperty* InProperty ) const override;
};
