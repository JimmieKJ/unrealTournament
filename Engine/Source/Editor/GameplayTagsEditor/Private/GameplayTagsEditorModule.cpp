// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsEditorModulePrivatePCH.h"
#include "GameplayTagsGraphPanelPinFactory.h"
#include "GameplayTagsGraphPanelNodeFactory.h"
#include "GameplayTagContainerCustomization.h"
#include "GameplayTagCustomization.h"
#include "GameplayTagsSettings.h"
#include "ISettingsModule.h"
#include "ModuleManager.h"


#define LOCTEXT_NAMESPACE "GameplayTagEditor"


class FGameplayTagsEditorModule
	: public IGameplayTagsEditorModule
{
public:

	// IModuleInterface

	virtual void StartupModule() override
	{
		// Register the details customizer
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomPropertyTypeLayout("GameplayTagContainer", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayTagContainerCustomization::MakeInstance));
		PropertyModule.RegisterCustomPropertyTypeLayout("GameplayTag", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayTagCustomization::MakeInstance));

		TSharedPtr<FGameplayTagsGraphPanelPinFactory> GameplayTagsGraphPanelPinFactory = MakeShareable( new FGameplayTagsGraphPanelPinFactory() );
		FEdGraphUtilities::RegisterVisualPinFactory(GameplayTagsGraphPanelPinFactory);

		TSharedPtr<FGameplayTagsGraphPanelNodeFactory> GameplayTagsGraphPanelNodeFactory = MakeShareable(new FGameplayTagsGraphPanelNodeFactory());
		FEdGraphUtilities::RegisterVisualNodeFactory(GameplayTagsGraphPanelNodeFactory);

		if (UGameplayTagsManager::ShouldImportTagsFromINI())
		{
			if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
			{
				SettingsModule->RegisterSettings("Project", "Project", "GameplayTags",
					LOCTEXT("GameplayTagSettingsName", "GameplayTags"),
					LOCTEXT("GameplayTagSettingsNameDesc", "GameplayTag Settings"),
					GetMutableDefault<UGameplayTagsSettings>()
					);
			}
		}
	}

	virtual void ShutdownModule() override
	{
		// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
		// we call this function before unloading the module.
	
	
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings("Project", "Project", "GameplayTags");
		}
	}
};


IMPLEMENT_MODULE(FGameplayTagsEditorModule, GameplayTagsEditor)

#undef LOCTEXT_NAMESPACE