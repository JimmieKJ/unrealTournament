// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "TextureEditorPrivatePCH.h"
#include "ISettingsModule.h"
#include "ModuleManager.h"
#include "Customizations/TextureDetailsCustomization.h"

#include "PropertyEditorModule.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "FTextureEditorModule"


const FName TextureEditorAppIdentifier = FName(TEXT("TextureEditorApp"));


/*-----------------------------------------------------------------------------
   FTextureEditorModule
-----------------------------------------------------------------------------*/

class FTextureEditorModule
	: public ITextureEditorModule
{
public:

	// ITextureEditorModule interface

	virtual TSharedRef<ITextureEditorToolkit> CreateTextureEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UTexture* Texture ) override
	{
		TSharedRef<FTextureEditorToolkit> NewTextureEditor(new FTextureEditorToolkit());
		NewTextureEditor->InitTextureEditor(Mode, InitToolkitHost, Texture);

		return NewTextureEditor;
	}

	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager( ) override
	{
		return MenuExtensibilityManager;
	}

	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager( ) override
	{
		return ToolBarExtensibilityManager;
	}

public:

	// IModuleInterface interface

	virtual void StartupModule( ) override
	{
		// register menu extensions
		MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
		ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

		// register settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Editor", "ContentEditors", "TextureEditor",
				LOCTEXT("TextureEditorSettingsName", "Texture Editor"),
				LOCTEXT("TextureEditorSettingsDescription", "Configure the look and feel of the Texture Editor."),
				GetMutableDefault<UTextureEditorSettings>()
			);
		}

		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout("Texture", FOnGetDetailCustomizationInstance::CreateStatic(&FTextureDetails::MakeInstance));
	}

	virtual void ShutdownModule( ) override
	{
		// unregister settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Editor", "ContentEditors", "TextureEditor");
		}

		// unregister menu extensions
		MenuExtensibilityManager.Reset();
		ToolBarExtensibilityManager.Reset();

		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("Texture2D");
	}

private:

	// Holds the menu extensibility manager.
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;

	// Holds the tool bar extensibility manager.
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
};


IMPLEMENT_MODULE(FTextureEditorModule, TextureEditor);


#undef LOCTEXT_NAMESPACE
