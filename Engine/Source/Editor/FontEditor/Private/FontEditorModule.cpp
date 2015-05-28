// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "FontEditorModule.h"
#include "ModuleManager.h"
#include "Factories.h"
#include "SFontEditorViewport.h"
#include "SCompositeFontEditor.h"
#include "FontEditor.h"

const FName FontEditorAppIdentifier = FName(TEXT("FontEditorApp"));


/*-----------------------------------------------------------------------------
   FFontEditorModule
-----------------------------------------------------------------------------*/

class FFontEditorModule : public IFontEditorModule
{
public:
	/** Constructor, set up console commands and variables **/
	FFontEditorModule()
	{
	}

	/** Called right after the module DLL has been loaded and the module object has been created */
	virtual void StartupModule() override
	{
		MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
		ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);
	}

	/** Called before the module is unloaded, right before the module object is destroyed */
	virtual void ShutdownModule() override
	{
		MenuExtensibilityManager.Reset();
		ToolBarExtensibilityManager.Reset();
	}

	/** Creates a new Font editor */
	virtual TSharedRef<IFontEditor> CreateFontEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UFont* Font ) override
	{
		TSharedRef<FFontEditor> NewFontEditor(new FFontEditor());
		NewFontEditor->InitFontEditor(Mode, InitToolkitHost, Font);
		return NewFontEditor;
	}

	/** Gets the extensibility managers for outside entities to extend static mesh editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override { return ToolBarExtensibilityManager; }

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
};

IMPLEMENT_MODULE( FFontEditorModule, FontEditor );
