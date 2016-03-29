// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AppFrameworkPrivatePCH.h"
#include "SPerfSuite.h"

#if !UE_BUILD_SHIPPING

#include "SDockTab.h"
#include "STableViewTesting.h"
#include "ISlateReflectorModule.h"

void SummonPerfTestSuite()
{
	// Need to load this module so we have the widget reflector tab available
	FModuleManager::LoadModuleChecked<ISlateReflectorModule>("SlateReflector");

	auto SpawnTableViewTesting = [](const FSpawnTabArgs&) -> TSharedRef<SDockTab>
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				MakeTableViewTesting()
			];
	};

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner("TableViewTesting", FOnSpawnTab::CreateLambda(SpawnTableViewTesting));
	
	TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout( "PerfTestSuite_Layout" )
	->AddArea
	(
		FTabManager::NewArea(1920,1200)
		->Split
		(
			FTabManager::NewStack()
			->AddTab("TableViewTesting", ETabState::OpenedTab)
		)
	)
	->AddArea
	(
		FTabManager::NewArea(640,800)
		->Split
		(
			FTabManager::NewStack()->AddTab("WidgetReflector", ETabState::OpenedTab)
		)
	);

	FGlobalTabmanager::Get()->RestoreFrom(Layout, TSharedPtr<SWindow>());
}

#endif // #if !UE_BUILD_SHIPPING
