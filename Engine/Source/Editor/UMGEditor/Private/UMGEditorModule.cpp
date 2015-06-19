// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"

#include "UMGEditorModule.h"
#include "ModuleManager.h"

#include "AssetToolsModule.h"
#include "AssetTypeActions_WidgetBlueprint.h"
#include "KismetCompilerModule.h"
#include "WidgetBlueprintCompiler.h"

#include "Editor/Sequencer/Public/ISequencerModule.h"
#include "Animation/MarginTrackEditor.h"
#include "Animation/Sequencer2DTransformTrackEditor.h"
#include "IUMGModule.h"
#include "ComponentReregisterContext.h"
#include "WidgetComponent.h"
#include "DesignerCommands.h"
#include "WidgetBlueprint.h"

#include "Settings/WidgetDesignerSettings.h"
#include "ISettingsModule.h"

#define LOCTEXT_NAMESPACE "UMG"

const FName UMGEditorAppIdentifier = FName(TEXT("UMGEditorApp"));

class FUMGEditorModule : public IUMGEditorModule, public IBlueprintCompiler
{
public:
	/** Constructor, set up console commands and variables **/
	FUMGEditorModule()
		: ReRegister(nullptr)
		, CompileCount(0)
	{
	}

	/** Called right after the module DLL has been loaded and the module object has been created */
	virtual void StartupModule() override
	{
		FModuleManager::LoadModuleChecked<IUMGModule>("UMG");

		if (GIsEditor)
		{
			FDesignerCommands::Register();
		}

		MenuExtensibilityManager = MakeShareable(new FExtensibilityManager());
		ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager());

		// Register widget blueprint compiler we do this no matter what.
		IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
		KismetCompilerModule.GetCompilers().Add(this);

		// Register asset types
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_WidgetBlueprint()));

		// Register with the sequencer module that we provide auto-key handlers.
		ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
		MarginTrackEditorCreateTrackEditorHandle    = SequencerModule.RegisterTrackEditor_Handle(FOnCreateTrackEditor::CreateStatic(&FMarginTrackEditor::CreateTrackEditor));
		TransformTrackEditorCreateTrackEditorHandle = SequencerModule.RegisterTrackEditor_Handle(FOnCreateTrackEditor::CreateStatic(&F2DTransformTrackEditor::CreateTrackEditor));

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if ( SettingsModule != nullptr )
		{
			// Designer settings
			SettingsModule->RegisterSettings("Editor", "ContentEditors", "WidgetDesigner",
				LOCTEXT("WidgetDesignerSettingsName", "Widget Designer"),
				LOCTEXT("WidgetDesignerSettingsDescription", "Configure options for the Widget Designer."),
				GetMutableDefault<UWidgetDesignerSettings>()
				);
		}
	}

	/** Called before the module is unloaded, right before the module object is destroyed. */
	virtual void ShutdownModule() override
	{
		MenuExtensibilityManager.Reset();
		ToolBarExtensibilityManager.Reset();

		// Unregister all the asset types that we registered
		if ( FModuleManager::Get().IsModuleLoaded("AssetTools") )
		{
			IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
			for ( int32 Index = 0; Index < CreatedAssetTypeActions.Num(); ++Index )
			{
				AssetTools.UnregisterAssetTypeActions(CreatedAssetTypeActions[Index].ToSharedRef());
			}
		}
		CreatedAssetTypeActions.Empty();

		// Unregister the setting
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if ( SettingsModule != nullptr )
		{
			SettingsModule->UnregisterSettings("Editor", "ContentEditors", "WidgetDesigner");
		}
	}

	bool CanCompile(const UBlueprint* Blueprint) override
	{
		return Cast<UWidgetBlueprint>(Blueprint) != nullptr;
	}

	void PreCompile(UBlueprint* Blueprint) override
	{
		if (!CanCompile(Blueprint))
		{
			return;
		}

		if ( CompileCount == 0 )
		{
			ReRegister = new TComponentReregisterContext<UWidgetComponent>();
		}

		CompileCount++;
	}

	void Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results, TArray<UObject*>* ObjLoaded) override
	{
		if ( UWidgetBlueprint* WidgetBlueprint = CastChecked<UWidgetBlueprint>(Blueprint) )
		{
			FWidgetBlueprintCompiler Compiler(WidgetBlueprint, Results, CompileOptions, ObjLoaded);
			Compiler.Compile();
			check(Compiler.NewClass);
		}
	}

	void PostCompile(UBlueprint* Blueprint) override
	{
		CompileCount--;

		if ( ReRegister && CompileCount == 0 )
		{
			delete ReRegister;
			ReRegister = nullptr;
		}
		
		if ( GIsEditor && GEditor )
		{
			GEditor->RedrawAllViewports(true);
		}
	}

	/** Gets the extensibility managers for outside entities to extend gui page editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override { return ToolBarExtensibilityManager; }

private:
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
	{
		AssetTools.RegisterAssetTypeActions(Action);
		CreatedAssetTypeActions.Add(Action);
	}

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	FDelegateHandle MarginTrackEditorCreateTrackEditorHandle;
	FDelegateHandle TransformTrackEditorCreateTrackEditorHandle;

	/** All created asset type actions.  Cached here so that we can unregister it during shutdown. */
	TArray< TSharedPtr<IAssetTypeActions> > CreatedAssetTypeActions;

	/** The temporary variable that captures and reinstances components after compiling finishes. */
	TComponentReregisterContext<UWidgetComponent>* ReRegister;

	/**
	 * The current count on the number of compiles that have occurred.  We don't want to re-register components until all
	 * compiling has stopped.
	 */
	int32 CompileCount;
};

IMPLEMENT_MODULE(FUMGEditorModule, UMGEditor);

#undef LOCTEXT_NAMESPACE
