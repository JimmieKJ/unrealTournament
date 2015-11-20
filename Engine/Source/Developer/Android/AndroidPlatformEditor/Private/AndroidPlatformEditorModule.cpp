// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AndroidPlatformEditorPrivatePCH.h"
#include "AndroidTargetSettingsCustomization.h"
#include "AndroidSDKSettingsCustomization.h"
#include "ModuleInterface.h"
#include "ISettingsModule.h"
#include "ModuleManager.h"
#include "MaterialShaderQualitySettingsCustomization.h"
#include "MaterialShaderQualitySettings.h"
#include "Materials/Material.h"
#include "RenderingThread.h"
#include "ComponentRecreateRenderStateContext.h"


#define LOCTEXT_NAMESPACE "FAndroidPlatformEditorModule"


/**
 * Module for Android platform editor utilities
 */
class FAndroidPlatformEditorModule
	: public IModuleInterface
{
	// IModuleInterface interface

	virtual void StartupModule() override
	{
		// register settings detail panel customization
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(
			UAndroidRuntimeSettings::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FAndroidTargetSettingsCustomization::MakeInstance)
		);

		PropertyModule.RegisterCustomClassLayout(
			UAndroidSDKSettings::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FAndroidSDKSettingsCustomization::MakeInstance)
			);

		FOnUpdateMaterialShaderQuality UpdateMaterials = FOnUpdateMaterialShaderQuality::CreateLambda([]()
		{
			FGlobalComponentRecreateRenderStateContext Recreate;
			FlushRenderingCommands();
			UMaterial::AllMaterialsCacheResourceShadersForRendering();
			UMaterialInstance::AllMaterialsCacheResourceShadersForRendering();
		});

		PropertyModule.RegisterCustomClassLayout(
			UShaderPlatformQualitySettings::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FMaterialShaderQualitySettingsCustomization::MakeInstance, UpdateMaterials)
			);

		PropertyModule.NotifyCustomizationModuleChanged();

		// register settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "Android",
				LOCTEXT("RuntimeSettingsName", "Android"),
				LOCTEXT("RuntimeSettingsDescription", "Project settings for Android apps"),
				GetMutableDefault<UAndroidRuntimeSettings>()
			);

 			SettingsModule->RegisterSettings("Project", "Platforms", "AndroidSDK",
 				LOCTEXT("SDKSettingsName", "Android SDK"),
 				LOCTEXT("SDKSettingsDescription", "Settings for Android SDK (for all projects)"),
 				GetMutableDefault<UAndroidSDKSettings>()
			);

			static FName NAME_OPENGL_ES2(TEXT("GLSL_ES2"));
			const UShaderPlatformQualitySettings* AndroidMaterialQualitySettings = UMaterialShaderQualitySettings::Get()->GetShaderPlatformQualitySettings(NAME_OPENGL_ES2);
			SettingsModule->RegisterSettings("Project", "Platforms", "AndroidES2Quality",
				LOCTEXT("AndroidES2QualitySettingsName", "Android ES2 Quality"),
				LOCTEXT("AndroidES2QualitySettingsDescription", "Settings for Android ES2 material quality"),
				AndroidMaterialQualitySettings
			);
		}

		// Force the SDK settings into a sane state initially so we can make use of them
		auto &TargetPlatformManagerModule = FModuleManager::LoadModuleChecked<ITargetPlatformManagerModule>("TargetPlatform");
		UAndroidSDKSettings * settings = GetMutableDefault<UAndroidSDKSettings>();
		settings->SetTargetModule(&TargetPlatformManagerModule);
		auto &AndroidDeviceDetection = FModuleManager::LoadModuleChecked<IAndroidDeviceDetectionModule>("AndroidDeviceDetection");
		settings->SetDeviceDetection(AndroidDeviceDetection.GetAndroidDeviceDetection());
		settings->UpdateTargetModulePaths();
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "Android");
			SettingsModule->UnregisterSettings("Project", "Platforms", "AndroidSDK");
			SettingsModule->UnregisterSettings("Project", "Platforms", "AndroidES2Quality");
		}
	}
};


IMPLEMENT_MODULE(FAndroidPlatformEditorModule, AndroidPlatformEditor);

#undef LOCTEXT_NAMESPACE
