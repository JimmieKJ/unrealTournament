// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CollisionAnalyzerPCH.h"
#include "WorkspaceMenuStructureModule.h"
#include "SDockTab.h"

namespace CollisionAnalyzerModule
{
	static const FName CollisionAnalyzerApp = FName(TEXT("CollisionAnalyzerApp"));
}

IMPLEMENT_MODULE(FCollisionAnalyzerModule, CollisionAnalyzer);
DEFINE_LOG_CATEGORY(LogCollisionAnalyzer);

void FCollisionAnalyzerModule::StartupModule() 
{
	FCollisionAnalyzerStyle::Initialize();

	CollisionAnalyzer = new FCollisionAnalyzer();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(CollisionAnalyzerModule::CollisionAnalyzerApp, FOnSpawnTab::CreateRaw(this, &FCollisionAnalyzerModule::SpawnCollisionAnalyzerTab))
		.SetDisplayName(NSLOCTEXT("CollisionAnalyzerModule", "TabTitle", "Collision Analyzer"))
		.SetTooltipText(NSLOCTEXT("CollisionAnalyzerModule", "TooltipText", "Open the Collision Analyzer tab."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "CollisionAnalyzer.TabIcon"));
}

void FCollisionAnalyzerModule::ShutdownModule() 
{
	if (CollisionAnalyzer != NULL)
	{
		delete CollisionAnalyzer;
		CollisionAnalyzer = NULL;
	}

	FCollisionAnalyzerStyle::Shutdown();
}

TSharedRef<SDockTab> FCollisionAnalyzerModule::SpawnCollisionAnalyzerTab(const FSpawnTabArgs& Args)
{
	check(CollisionAnalyzer);

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			CollisionAnalyzer->SummonUI().ToSharedRef()
		];
}
