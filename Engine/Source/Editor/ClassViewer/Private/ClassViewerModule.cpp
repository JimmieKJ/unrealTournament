// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ClassViewerPrivatePCH.h"
#include "Editor/ClassViewer/Private/SClassViewer.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"

#include "ModuleManager.h"
#include "SDockTab.h"

IMPLEMENT_MODULE( FClassViewerModule, ClassViewer );

namespace ClassViewerModule
{
	static const FName ClassViewerApp = FName("ClassViewerApp");
}

TSharedRef<SDockTab> CreateClassPickerTab( const FSpawnTabArgs& Args )
{
	FClassViewerInitializationOptions InitOptions;
	InitOptions.Mode = EClassViewerMode::ClassBrowsing;
	InitOptions.DisplayMode = EClassViewerDisplayMode::TreeView;

	return SNew(SDockTab)
		.TabRole( ETabRole::NomadTab )
		[
			SNew( SClassViewer, InitOptions )
			.OnClassPickedDelegate(FOnClassPicked())
		];
}


void FClassViewerModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner( ClassViewerModule::ClassViewerApp, FOnSpawnTab::CreateStatic( &CreateClassPickerTab ) )
		.SetDisplayName( NSLOCTEXT("ClassViewerApp", "TabTitle", "Class Viewer") )
		.SetTooltipText( NSLOCTEXT("ClassViewerApp", "TooltipText", "Displays all classes that exist within this project.") )
		.SetGroup( WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory() )
		.SetIcon( FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassViewer.TabIcon") );
}

void FClassViewerModule::ShutdownModule()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner( ClassViewerModule::ClassViewerApp );
	}

	SClassViewer::DestroyClassHierarchy();
}


/**
 * Creates a class viewer widget
 *
 * @param	InitOptions						Programmer-driven configuration for this widget instance
 * @param	OnClassPickedDelegate			Optional callback when a class is selected in 'class picking' mode
 *
 * @return	New class viewer widget
 */
TSharedRef<SWidget> FClassViewerModule::CreateClassViewer(const FClassViewerInitializationOptions& InitOptions, const FOnClassPicked& OnClassPickedDelegate )
{
	return SNew( SClassViewer, InitOptions )
			.OnClassPickedDelegate(OnClassPickedDelegate);
}