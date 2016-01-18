// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PluginBrowserPrivatePCH.h"
#include "SPluginBrowser.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "Editor/UnrealEd/Public/Features/EditorFeatures.h"
#include "PluginMetadataObject.h"
#include "PluginStyle.h"
#include "PropertyEditorModule.h"
#include "PluginBrowserModule.h"
#include "SDockTab.h"
#include "SNewPluginWizard.h"


#define LOCTEXT_NAMESPACE "PluginsEditor"

IMPLEMENT_MODULE( FPluginBrowserModule, PluginBrowserModule )

const FName FPluginBrowserModule::PluginsEditorTabName( TEXT( "PluginsEditor" ) );
const FName FPluginBrowserModule::PluginCreatorTabName( TEXT( "PluginCreator" ) );

void FPluginBrowserModule::StartupModule()
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

			MajorTab->SetContent( SNew( SPluginBrowser ) );

			return MajorTab;
		}
	};

	// Register the detail customization for the metadata object
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(UPluginMetadataObject::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FPluginMetadataCustomization::MakeInstance));
	
	// Register a tab spawner so that our tab can be automatically restored from layout files
	FGlobalTabmanager::Get()->RegisterTabSpawner( PluginsEditorTabName, FOnSpawnTab::CreateStatic( &Local::SpawnPluginsEditorTab ) )
			.SetDisplayName( LOCTEXT( "PluginsEditorTabTitle", "Plugins" ) )
			.SetTooltipText( LOCTEXT( "PluginsEditorTooltipText", "Open the Plugins Browser tab." ) )
			.SetIcon(FSlateIcon(FPluginStyle::Get()->GetStyleSetName(), "Plugins.TabIcon"));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		PluginCreatorTabName,
		FOnSpawnTab::CreateRaw(this, &FPluginBrowserModule::HandleSpawnPluginCreatorTab))
		.SetDisplayName(LOCTEXT("NewPluginTabHeader", "New Plugin"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FPluginBrowserModule::ShutdownModule()
{
	FPluginStyle::Shutdown();

	// Unregister the tab spawner
	FGlobalTabmanager::Get()->UnregisterTabSpawner( PluginsEditorTabName );
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner( PluginCreatorTabName );

	// Unregister our feature
	IModularFeatures::Get().UnregisterModularFeature( EditorFeatures::PluginsEditor, this );
}

void FPluginBrowserModule::SetPluginPendingEnableState(const FString& PluginName, bool bCurrentlyEnabled, bool bPendingEnabled)
{
	if (bCurrentlyEnabled == bPendingEnabled)
	{
		PendingEnablePlugins.Remove(PluginName);
	}
	else
	{
		PendingEnablePlugins.FindOrAdd(PluginName) = bPendingEnabled;
	}
}

bool FPluginBrowserModule::GetPluginPendingEnableState(const FString& PluginName) const
{
	check(PendingEnablePlugins.Contains(PluginName));

	return PendingEnablePlugins[PluginName];
}

bool FPluginBrowserModule::HasPluginPendingEnable(const FString& PluginName) const
{
	return PendingEnablePlugins.Contains(PluginName);
}

TSharedRef<SDockTab> FPluginBrowserModule::HandleSpawnPluginCreatorTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedRef<SDockTab> ResultTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab);

	TSharedRef<SWidget> TabContentWidget = SNew(SNewPluginWizard, ResultTab);
	ResultTab->SetContent(TabContentWidget);

	return ResultTab;
}

#undef LOCTEXT_NAMESPACE