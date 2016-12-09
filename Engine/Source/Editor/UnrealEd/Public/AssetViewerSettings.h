// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/AssetPtr.h"
#include "UObject/ObjectRedirector.h"
#include "Engine/Scene.h"
#include "Engine/TextureCube.h"

#include "AssetViewerSettings.generated.h"

/**
* Preview scene profile settings structure.
*/
USTRUCT()
struct FPreviewSceneProfile
{
	GENERATED_BODY()

	FPreviewSceneProfile()
	{		
		bShowFloor = true;
		bShowEnvironment = true;
		bRotateLightingRig = false;
		EnvironmentCubeMap = nullptr;
		DirectionalLightIntensity = 1.0f;
		DirectionalLightColor = FLinearColor::White;
		SkyLightIntensity = 1.0f;
		LightingRigRotation = 0.0f;
		RotationSpeed = 2.0f;
		// Set up default cube map texture from editor/engine textures
		EnvironmentCubeMap = nullptr;
#if WITH_EDITORONLY_DATA
		EnvironmentCubeMap = LoadObject<UTextureCube>(NULL, TEXT("/Engine/EditorMaterials/AssetViewer/EpicQuadPanorama_CC+EV1.EpicQuadPanorama_CC+EV1"));		
#endif
		bPostProcessingEnabled = true;
		DirectionalLightRotation = FRotator(-40.f, -67.5f, 0.f);
	}

	/** Name to identify the profile */
	UPROPERTY(EditAnywhere, config, Category=Profile)
	FString ProfileName;

	/** Manually set the directional light intensity (0.0 - 20.0) */
	UPROPERTY(EditAnywhere, config, Category = Lighting, meta = (UIMin = "0.0", UIMax = "20.0"))
	float DirectionalLightIntensity;

	/** Manually set the directional light colour */
	UPROPERTY(EditAnywhere, config, Category = Lighting)
	FLinearColor DirectionalLightColor;
	
	/** Manually set the sky light intensity (0.0 - 20.0) */
	UPROPERTY(EditAnywhere, config, Category = Lighting, meta = (UIMin = "0.0", UIMax = "20.0"))
	float SkyLightIntensity;

	/** Toggle rotating of the sky and directional lighting, press K and drag for manual rotating of Sky and L for Directional lighting */
	UPROPERTY(EditAnywhere, config, Category = Lighting, meta = (DisplayName = "Rotate Sky and Directional Lighting"))
	bool bRotateLightingRig;
	
	/** Toggle visibility of the environment sphere */
	UPROPERTY(EditAnywhere, config, Category = Environment)
	bool bShowEnvironment;

	/** Toggle visibility of the floor mesh */
	UPROPERTY(EditAnywhere, config, Category = Environment)
	bool bShowFloor;

	/** Sets environment cube map used for sky lighting and reflections */
	UPROPERTY(EditAnywhere, transient, Category = Environment)
	TAssetPtr<UTextureCube> EnvironmentCubeMap;

	/** Storing path to environment cube to prevent it from getting cooked */
	UPROPERTY(config)
	FString EnvironmentCubeMapPath;

	/** Manual set post processing settings */
	UPROPERTY(EditAnywhere, config, Category = PostProcessing, AdvancedDisplay)
	FPostProcessSettings PostProcessingSettings;

	/** Whether or not the Post Processing should influence the scene */
	UPROPERTY(EditAnywhere, config, Category = PostProcessing, AdvancedDisplay)
	bool bPostProcessingEnabled;

	/** Current rotation value of the sky in degrees (0 - 360) */
	UPROPERTY(EditAnywhere, config, Category = Lighting, meta = (UIMin = "0", UIMax = "360"), AdvancedDisplay)
	float LightingRigRotation;

	/** Speed at which the sky rotates when rotating is toggled */
	UPROPERTY(EditAnywhere, config, Category = Lighting, AdvancedDisplay)
	float RotationSpeed;

	/** Rotation for directional light */
	UPROPERTY(config)
	FRotator DirectionalLightRotation;

	/** Retrieve the environment map texture using the saved path */
	void LoadEnvironmentMap()
	{
		if (EnvironmentCubeMap == nullptr)
		{
			// Load cube map from stored path
			UObject* LoadedObject = nullptr;
			LoadedObject = LoadObject<UObject>(nullptr, *EnvironmentCubeMapPath);
			while (UObjectRedirector* Redirector = Cast<UObjectRedirector>(LoadedObject))
			{
				LoadedObject = Redirector->DestinationObject;
			}

			EnvironmentCubeMap = LoadedObject;
		}		
	}
};

/**
* Default asset viewer settings.
*/
UCLASS(config=Editor, defaultconfig, meta=(DisplayName = "Asset Viewer"))
class UNREALED_API UAssetViewerSettings : public UObject
{
	GENERATED_BODY()
public:
	UAssetViewerSettings();
	static UAssetViewerSettings* Get();
	
	/** Saves the config data out to the ini files */
	void Save();

	DECLARE_EVENT_OneParam(UAssetViewerSettings, FOnAssetViewerSettingsChangedEvent, const FName&);
	FOnAssetViewerSettingsChangedEvent& OnAssetViewerSettingsChanged() { return OnAssetViewerSettingsChangedEvent; }

	DECLARE_EVENT(UAssetViewerSettings, FOnAssetViewerProfileAddRemovedEvent);
	FOnAssetViewerProfileAddRemovedEvent& OnAssetViewerProfileAddRemoved() { return OnAssetViewerProfileAddRemovedEvent; }

	/** Begin UObject */
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostInitProperties() override;
	/** End UObject */

	/** Collection of scene profiles */
	UPROPERTY(EditAnywhere, config, Category = Settings, meta=(ShowOnlyInnerProperties))
	TArray<FPreviewSceneProfile> Profiles;
protected:
	/** Cached value to determine whether or not a profile was added or removed*/
	int32 NumProfiles;

	/** Broadcasts after an scene profile was added or deleted from the asset viewer singleton instance */
	FOnAssetViewerSettingsChangedEvent OnAssetViewerSettingsChangedEvent;

	FOnAssetViewerProfileAddRemovedEvent OnAssetViewerProfileAddRemovedEvent;
};
