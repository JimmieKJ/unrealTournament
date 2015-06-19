// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "Toolkits/IToolkit.h"	// For EAssetEditorMode
#include "Toolkits/AssetEditorToolkit.h" // For FExtensibilityManager

class INiagaraEditor;
class INiagaraEffectEditor;

/** DataTable Editor module */
class FNiagaraEditorModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility
{

public:
	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Compile the specified script. */
	virtual void CompileScript(class UNiagaraScript* ScriptToCompile);

	/** Creates an instance of Niagara editor.  Only virtual so that it can be called across the DLL boundary. */
	virtual TSharedRef<INiagaraEditor> CreateNiagaraEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UNiagaraScript* Script );
	virtual TSharedRef<INiagaraEffectEditor> CreateNiagaraEffectEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UNiagaraEffect* Effect);

	/** Gets the extensibility managers for outside entities to extend static mesh editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override {return MenuExtensibilityManager;}
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override {return ToolBarExtensibilityManager;}

	/** Niagara Editor app identifier string */
	static const FName NiagaraEditorAppIdentifier;

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
};


