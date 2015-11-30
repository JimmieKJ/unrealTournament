// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "FbxSceneImportOptionsMaterial.generated.h"

UCLASS(config=EditorPerProjectUserSettings, HideCategories=Object, MinimalAPI)
class UFbxSceneImportOptionsMaterial : public UObject
{
	GENERATED_UCLASS_BODY()
	
	//////////////////////////////////////////////////////////////////////////
	// Material section

	/** Whether to automatically create Unreal materials for materials found in the FBX scene */
	UPROPERTY(EditAnywhere, config, Category = Material)
	uint32 bImportMaterials : 1;

	/** The option works only when option "Import UMaterial" is OFF. If "Import UMaterial" is ON, textures are always imported. */
	UPROPERTY(EditAnywhere, config, Category = Material)
	uint32 bImportTextures : 1;

	/** If either importing of textures (or materials) is enabled, this option will cause normal map values to be inverted */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = Material)
	uint32 bInvertNormalMaps : 1;
};



