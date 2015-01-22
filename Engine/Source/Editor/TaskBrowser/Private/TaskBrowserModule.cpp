// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "TaskBrowserPrivatePCH.h"
#include "STaskBrowser.h"

#include "ModuleManager.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "SDockTab.h"

IMPLEMENT_MODULE( FTaskBrowserModule, TaskBrowser );

namespace TaskBrowserModule
{
	static const FName TaskBrowserApp = FName(TEXT("TaskBrowserApp"));
}

static TSharedRef<SDockTab> SpawnTaskBrowserTab( const FSpawnTabArgs& Args )
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(STaskBrowser)
		];
}


void FTaskBrowserModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner( TaskBrowserModule::TaskBrowserApp, FOnSpawnTab::CreateStatic( &SpawnTaskBrowserTab ) )
		.SetDisplayName(NSLOCTEXT("TaskBrowser", "TabTitle", "Task Browser"))
		.SetGroup( WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory() );
}


void FTaskBrowserModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TaskBrowserModule::TaskBrowserApp);
}

/**
 * Creates a tasks browser widget
 *
 * @return	New tasks browser widget
 */
TSharedRef<SWidget> FTaskBrowserModule::CreateTaskBrowser()
{
	return SNew( STaskBrowser );
}