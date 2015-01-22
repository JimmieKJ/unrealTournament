// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginsEditorPrivatePCH.h"
#include "SPluginsEditor.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "Editor/UnrealEd/Public/Features/EditorFeatures.h"
#include "PluginStyle.h"
#include "SDockTab.h"

#define LOCTEXT_NAMESPACE "PluginsEditor"

class FPluginsEditor : public IPluginsEditor
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** ID name for the plugins editor major tab */
	static const FName PluginsEditorTabName;
};

IMPLEMENT_MODULE( FPluginsEditor, PluginsEditor )

const FName FPluginsEditor::PluginsEditorTabName( TEXT( "PluginsEditor" ) );

void FPluginsEditor::StartupModule()
{
	FPluginStyle::Initialize();

	// Register ourselves as an editor feature
	IModularFeatures::Get().RegisterModularFeature( EditorFeatures::PluginsEditor, this );

	// @todo plugedit: Should we auto-watch plugin directories and refresh the plugins editor UI with newly-dropped plugins?

	struct Local
	{
		static TSharedRef<SDockTab> SpawnPluginsEditorTab( const FSpawnTabArgs& SpawnTabArgs )
		{
			const TSharedRef<SDockTab> MajorTab = 
				SNew( SDockTab )
				.Icon( FPluginStyle::Get()->GetBrush("Plugins.TabIcon") )
				.TabRole( ETabRole::MajorTab );

			MajorTab->SetContent( SNew( SPluginsEditor ) );

			return MajorTab;
		}
	};

	
	// Register a tab spawner so that our tab can be automatically restored from layout files
	FGlobalTabmanager::Get()->RegisterTabSpawner( PluginsEditorTabName, FOnSpawnTab::CreateStatic( &Local::SpawnPluginsEditorTab ) )
			.SetDisplayName( LOCTEXT( "PluginsEditorTabTitle", "Plugins" ) )
			.SetTooltipText( LOCTEXT( "PluginsEditorTooltipText", "Open the Plugins Browser tab." ) )
			.SetIcon(FSlateIcon(FPluginStyle::Get()->GetStyleSetName(), "Plugins.TabIcon"));
}

void FPluginsEditor::ShutdownModule()
{
	FPluginStyle::Shutdown();

	// Unregister the tab spawner
	FGlobalTabmanager::Get()->UnregisterTabSpawner( PluginsEditorTabName );

	// Unregister our feature
	IModularFeatures::Get().UnregisterModularFeature( EditorFeatures::PluginsEditor, this );
}

#undef LOCTEXT_NAMESPACE