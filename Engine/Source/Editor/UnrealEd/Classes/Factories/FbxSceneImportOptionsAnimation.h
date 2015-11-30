// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "FbxSceneImportOptionsAnimation.generated.h"

UCLASS(config=EditorPerProjectUserSettings, HideCategories=Object, MinimalAPI)
class UFbxSceneImportOptionsAnimation : public UObject
{
	GENERATED_UCLASS_BODY()
	
	//////////////////////////////////////////////////////////////////////////
	// Animation section

	/** True to import animations from the FBX File */
	UPROPERTY(EditAnywhere, config, Category = Animation)
	uint32 bImportAnimations : 1;

	/** Type of asset to import from the FBX file */
	UPROPERTY(EditAnywhere, Category = Animation, config, meta = (DisplayName = "Animation Length"))
	TEnumAsByte<enum EFBXAnimationLengthImportType> AnimationLength;

	/** Type of asset to import from the FBX file */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Animation, meta = (DisplayName = "Start Frame"))
	int32	StartFrame;

	/** Type of asset to import from the FBX file */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Animation, meta = (DisplayName = "End Frame"))
	int32	EndFrame;

	/** Enable this option to use default sample rate for the imported animation at 30 frames per second */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = Animation, meta = (ToolTip = "If enabled, samples all animation curves to 30 FPS"))
	bool bUseDefaultSampleRate;

	/** Name of source animation that was imported, used to reimport correct animation from the FBX file*/
	UPROPERTY()
	FString SourceAnimationName;

	/** Import if custom attribute as a curve within the animation **/
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = Animation)
	bool bImportCustomAttribute;

	/** Type of asset to import from the FBX file */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = Animation)
	bool bPreserveLocalTransform;

	/** Type of asset to import from the FBX file */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category = Animation)
	bool bDeleteExistingMorphTargetCurves;
};



