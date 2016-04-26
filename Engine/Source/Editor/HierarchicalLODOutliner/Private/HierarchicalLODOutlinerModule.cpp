// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "HierarchicalLODOutlinerPrivatePCH.h"
#include "HierarchicalLODOutlinerModule.h"
#include "HLODOutliner.h"
#include "HierarchicalLODUtilities.h"

void FHierarchicalLODOutlinerModule::StartupModule()
{
	check(GUnrealEd);
	ArrayChangedDelegate = GUnrealEd->OnHLODLevelsArrayChanged().AddRaw(this, &FHierarchicalLODOutlinerModule::OnHLODLevelsArrayChangedEvent);
}

void FHierarchicalLODOutlinerModule::ShutdownModule()
{
	if (GUnrealEd)
	{
		GUnrealEd->OnHLODLevelsArrayChanged().Remove(ArrayChangedDelegate);
	}
}

void FHierarchicalLODOutlinerModule::OnHLODLevelsArrayChangedEvent()
{
	UWorld* CurrentWorld = nullptr;
	
	// Get the correct UWorld instance
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE)
		{
			CurrentWorld = Context.World();
			break;
		}
		else if (Context.WorldType == EWorldType::Editor)
		{
			CurrentWorld = Context.World();
		}
	}

	AWorldSettings* WorldSettings = (CurrentWorld) ? CurrentWorld->GetWorldSettings() : nullptr;

	if (WorldSettings)
	{
		TArray<FHierarchicalSimplification>& HierarchicalLODSetup = WorldSettings->HierarchicalLODSetup;
		int32 NumHLODLevels = WorldSettings->NumHLODLevels;

		if (HierarchicalLODSetup.Num() > 1 && HierarchicalLODSetup.Num() > NumHLODLevels)
		{
			// HLOD level was added, use previous level settings to "guess" new settings
			FHierarchicalSimplification& NewLevelSetup = HierarchicalLODSetup.Last();
			const FHierarchicalSimplification& OldLastLevelSetup = HierarchicalLODSetup[HierarchicalLODSetup.Num() - 2];

			NewLevelSetup.bSimplifyMesh = OldLastLevelSetup.bSimplifyMesh;
			NewLevelSetup.MergeSetting = OldLastLevelSetup.MergeSetting;
			NewLevelSetup.ProxySetting = OldLastLevelSetup.ProxySetting;

			NewLevelSetup.DesiredBoundRadius = OldLastLevelSetup.DesiredBoundRadius * 2.5f;
			NewLevelSetup.DesiredFillingPercentage = FMath::Max(OldLastLevelSetup.DesiredFillingPercentage * 0.75f, 1.0f);
			NewLevelSetup.TransitionScreenSize = OldLastLevelSetup.TransitionScreenSize * 0.75f;
			NewLevelSetup.MinNumberOfActorsToBuild = OldLastLevelSetup.MinNumberOfActorsToBuild;
		}
		else if (HierarchicalLODSetup.Num() < NumHLODLevels)
		{
			// HLOD Level was removed, now remove all LODActors for this level
			FHierarchicalLODUtilities::DeleteLODActorsInHLODLevel(CurrentWorld, NumHLODLevels - 1);
		}
	}
}

TSharedRef<SWidget> FHierarchicalLODOutlinerModule::CreateHLODOutlinerWidget()
{
	SAssignNew(HLODWindow, HLODOutliner::SHLODOutliner);		
	return HLODWindow->AsShared();
}

IMPLEMENT_MODULE(FHierarchicalLODOutlinerModule, HLODPluginModule);