// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "IOSPlatformEditorPrivatePCH.h"
#include "IOSTargetSettingsCustomization.h"
#include "ISettingsModule.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"
#include "MaterialShaderQualitySettings.h"
#include "MaterialShaderQualitySettingsCustomization.h"
#include "RenderingThread.h"
#include "ComponentRecreateRenderStateContext.h"

#define LOCTEXT_NAMESPACE "FIOSPlatformEditorModule"


/**
 * Module for iOS as a target platform
 */
class FIOSPlatformEditorModule
	: public IModuleInterface
{
	// IModuleInterface interface

	virtual void StartupModule() override
	{
		// register settings detail panel customization
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(
			"IOSRuntimeSettings",
			FOnGetDetailCustomizationInstance::CreateStatic(&FIOSTargetSettingsCustomization::MakeInstance)
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
			SettingsModule->RegisterSettings("Project", "Platforms", "iOS",
				LOCTEXT("RuntimeSettingsName", "iOS"),
				LOCTEXT("RuntimeSettingsDescription", "Settings and resources for the iOS platform"),
				GetMutableDefault<UIOSRuntimeSettings>()
			);

			static FName NAME_OPENGL_ES2_IOS(TEXT("GLSL_ES2_IOS"));
			const UShaderPlatformQualitySettings* IOSES2MaterialQualitySettings = UMaterialShaderQualitySettings::Get()->GetShaderPlatformQualitySettings(NAME_OPENGL_ES2_IOS);
			SettingsModule->RegisterSettings("Project", "Platforms", "iOSES2Quality",
				LOCTEXT("IOSES2QualitySettingsName", "iOS ES2 Quality"),
				LOCTEXT("IOSES2QualitySettingsDescription", "Settings for iOS ES2 material quality"),
				IOSES2MaterialQualitySettings
				);
		}
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "iOS");
			SettingsModule->UnregisterSettings("Project", "Platforms", "iOSES2Quality");
		}
	}
};


IMPLEMENT_MODULE(FIOSPlatformEditorModule, IOSPlatformEditor);

#undef LOCTEXT_NAMESPACE
