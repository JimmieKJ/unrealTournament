//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "SubstanceImportOptionsUi.generated.h"

UCLASS(config=EditorUserSettings, AutoExpandCategories=(General, Materials), HideCategories=Object)
class USubstanceImportOptionsUi : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Use the string in "Name" field as base name of factory instance and textures */
	UPROPERTY(EditAnywhere, config, Category=General)
	uint32 bOverrideFullName:1;
	
	UPROPERTY(EditAnywhere, config, Category=General)
	uint32 bOverrideInstancePath:1;

	UPROPERTY(EditAnywhere, config, Category=General)
	uint32 bOverrideMaterialPath:1;

	/** Whether to automatically create graph instances for every graph description present in package*/
	UPROPERTY(EditAnywhere, config, Category=General)
	uint32 bCreateInstance:1;

	uint32 bForceCreateInstance:1;

	/** Whether to automatically create Unreal materials for instances */
	UPROPERTY(EditAnywhere, config, Category=Materials)
	uint32 bCreateMaterial:1;

	/** Instance suggested name (based on filename) */
	UPROPERTY(EditAnywhere, config, Category=General)
	FString InstanceName;

	/** Instance suggested name (based on filename) */
	UPROPERTY(EditAnywhere, config, Category=General)
	FString MaterialName;

	/** Force the Graph Instance objects' path */
	UPROPERTY(EditAnywhere, config, Category=General)
	FString InstanceDestinationPath;
	
	/** Force the Texture Objects objects' path */
	UPROPERTY(EditAnywhere, config, Category=General)
	FString MaterialDestinationPath;
};
