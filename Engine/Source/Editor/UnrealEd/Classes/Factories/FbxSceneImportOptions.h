// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "FbxSceneImportOptions.generated.h"

UENUM()
enum EFBXSceneOptionsCreateHierarchyType
{
	FBXSOCHT_CreateLevelActors UMETA(DisplayName = "Create Level Actors"),
	FBXSOCHT_CreateActorComponents UMETA(DisplayName = "Create one Actor with Components"),
	FBXSOCHT_CreateBlueprint UMETA(DisplayName = "Create one Blueprint asset"),
	FBXSOCHT_MAX
};
//	Fbx_AddToBlueprint UMETA(DisplayName = "Add to an existing Blueprint asset"),



UCLASS(config = EditorPerProjectUserSettings, HideCategories=Object, MinimalAPI)
class UFbxSceneImportOptions : public UObject
{
	GENERATED_UCLASS_BODY()
	
	//////////////////////////////////////////////////////////////////////////
	// Scene section, detail of a fbx node
	// TODO: Move the content folder selection here with a custom detail class
	// TODO: Allow the user to select an existing blueprint
	// TODO: Allow the user to specify a name for the root node

	/** TODO - The Content Path choose by the user */
	//UPROPERTY()
	//FString ContentPath;

	/** If check, a folders hierarchy will be create under the import asset path. All the create folders will match the actor hierarchy. In case there is more then one actor that link to an asset, the shared asset will be place at the first actor place.  */
	UPROPERTY(EditAnywhere, config, category = ImportOptions)
	uint32 bCreateContentFolderHierarchy : 1;

	/** If checked, the mobility of all actors or component will be dynamic. If not it will be static  */
	UPROPERTY(EditAnywhere, config, category = ImportOptions)
	uint32 bImportAsDynamic : 1;

	/** Choose if you want to generate the scene hierarchy with normal level actors or one actor with multiple components or one blueprint asset with multiple components.  */
	UPROPERTY(EditAnywhere, config, category = ImportOptions)
	TEnumAsByte<enum EFBXSceneOptionsCreateHierarchyType> HierarchyType;

	UPROPERTY(EditAnywhere, config, Category = Transform)
	FVector ImportTranslation;

	UPROPERTY(EditAnywhere, config, Category = Transform)
	FRotator ImportRotation;

	UPROPERTY(EditAnywhere, config, Category = Transform)
	float ImportUniformScale;

	/** If enabled, creates LOD models for Unreal static meshes from LODs in the import file; If not enabled, only the base static mesh from the LOD group is imported. */
	UPROPERTY(EditAnywhere, config, category = Meshes)
	uint32 bImportStaticMeshLODs : 1;

	/** If enabled, creates LOD models for Unreal skeletal meshes from LODs in the import file; If not enabled, only the base skeletal mesh from the LOD group is imported. */
	UPROPERTY(EditAnywhere, config, category = Meshes)
	uint32 bImportSkeletalMeshLODs : 1;

	/** TODO - Name of the parent actor or the blueprint. This is ignore if the user choose to create level actors or an existing blueprint */
	//UPROPERTY()
	//FString ParentName;

	/** TODO - If user choose existing blueprint, this is the path of the blueprint asset */
	//UPROPERTY()
	//FString ExistingBlueprintPath
};



