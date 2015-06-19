// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "SoundCueEditorModule.h"
#include "ModuleManager.h"
#include "SoundCueEditor.h"

DEFINE_LOG_CATEGORY(LogSoundCueEditor);

const FName SoundCueEditorAppIdentifier = FName(TEXT("SoundCueEditorApp"));


/*-----------------------------------------------------------------------------
   FSoundCueEditorModule
-----------------------------------------------------------------------------*/
#include "SoundDefinitions.h"
#include "Sound/SoundCue.h"

class FSoundCueEditorModule : public ISoundCueEditorModule
{
public:
	/** Constructor, set up console commands and variables **/
	FSoundCueEditorModule()
	{
	}

	/** Called right after the module DLL has been loaded and the module object has been created */
	virtual void StartupModule() override
	{
		MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
		ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);
	}

	/** Called before the module is unloaded, right before the module object is destroyed. */
	virtual void ShutdownModule() override
	{
		MenuExtensibilityManager.Reset();
		ToolBarExtensibilityManager.Reset();
	}

	/** Creates a new material editor, either for a material or a material function */
	virtual TSharedRef<ISoundCueEditor> CreateSoundCueEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, USoundCue* SoundCue) override
	{
		TSharedRef<FSoundCueEditor> NewSoundCueEditor(new FSoundCueEditor());
		NewSoundCueEditor->InitSoundCueEditor(Mode, InitToolkitHost, SoundCue);
		return NewSoundCueEditor;
	}

	/** Gets the extensibility managers for outside entities to extend static mesh editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override { return ToolBarExtensibilityManager; }

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
};

IMPLEMENT_MODULE(FSoundCueEditorModule, SoundCueEditor);
