// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "BlueprintEditor.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "WorkflowCentricApplication"

/////////////////////////////////////////////////////
// FWorkflowCentricApplication

TArray<FWorkflowApplicationModeExtender> FWorkflowCentricApplication::ModeExtenderList;

void FWorkflowCentricApplication::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	if (CurrentAppModePtr.IsValid())
	{
		CurrentAppModePtr->RegisterTabFactories(InTabManager);
	}
}

void FWorkflowCentricApplication::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	InTabManager->UnregisterAllTabSpawners();
}


FName FWorkflowCentricApplication::GetCurrentMode() const
{
	return CurrentAppModePtr.IsValid() ? CurrentAppModePtr->GetModeName() : NAME_None;
}

void FWorkflowCentricApplication::SetCurrentMode(FName NewMode)
{
	const bool bModeAlreadyActive = CurrentAppModePtr.IsValid() && (NewMode == CurrentAppModePtr->GetModeName());

	if (!bModeAlreadyActive)
	{
		check(TabManager.IsValid());

		TSharedPtr<FApplicationMode> NewModePtr = ApplicationModeList.FindRef(NewMode);

		if (NewModePtr.IsValid())
		{
			// Deactivate the old mode
			if (CurrentAppModePtr.IsValid())
			{
				check(TabManager.IsValid());
				CurrentAppModePtr->PreDeactivateMode();
				CurrentAppModePtr->DeactivateMode(TabManager);
				RemoveToolbarExtender(CurrentAppModePtr->GetToolbarExtender());
				RemoveAllToolbarWidgets();
			}

			// Unregister tab spawners
			TabManager->UnregisterAllTabSpawners();

			//@TODO: Should do some validation here
			CurrentAppModePtr = NewModePtr;

			// Establish the workspace menu category for the new mode
			TabManager->ClearLocalWorkspaceMenuCategories();
			TabManager->AddLocalWorkspaceMenuItem( CurrentAppModePtr->GetWorkspaceMenuCategory() );

			// Activate the new layout
			const TSharedRef<FTabManager::FLayout> NewLayout = CurrentAppModePtr->ActivateMode(TabManager);
			RestoreFromLayout(NewLayout);

			// Give the new mode a chance to do init
			CurrentAppModePtr->PostActivateMode();

			AddToolbarExtender(NewModePtr->GetToolbarExtender());
			RegenerateMenusAndToolbars();
		}
	}
}


TSharedRef<SDockTab> FWorkflowCentricApplication::CreatePanelTab(const FSpawnTabArgs& Args, TSharedPtr<FWorkflowTabFactory> TabFactory)
{
	FWorkflowTabSpawnInfo SpawnInfo;

	return TabFactory->SpawnTab(SpawnInfo);
}

void FWorkflowCentricApplication::PushTabFactories(FWorkflowAllowedTabSet& FactorySetToPush)
{
	FWorkflowTabSpawnInfo SpawnInfo;

	for (auto FactoryIt = FactorySetToPush.CreateIterator(); FactoryIt; ++FactoryIt)
	{
		TSharedPtr<FWorkflowTabFactory> SomeFactory = FactoryIt.Value();
		FTabSpawnerEntry& SpawnerEntry = TabManager->RegisterTabSpawner(SomeFactory->GetIdentifier(), FOnSpawnTab::CreateRaw(this, &FWorkflowCentricApplication::CreatePanelTab, SomeFactory))
			.SetDisplayName(SomeFactory->ConstructTabName(SpawnInfo).Get())
			.SetTooltipText(SomeFactory->GetTabToolTipText(SpawnInfo))
			.SetGroup(CurrentAppModePtr->GetWorkspaceMenuCategory());
		
		// Add the tab icon to the menu entry if one was provided
		const FSlateIcon& TabSpawnerIcon = SomeFactory->GetTabSpawnerIcon(SpawnInfo);
		if (TabSpawnerIcon.IsSet())
		{
			SpawnerEntry.SetIcon(TabSpawnerIcon);
		}
	} 
}

bool FWorkflowCentricApplication::OnRequestClose()
{
	if (!FSlateApplication::Get().IsNormalExecution())
	{
		return false;
	}

	if (CurrentAppModePtr.IsValid())
	{
		check(TabManager.IsValid());

		// Deactivate the old mode
		CurrentAppModePtr->PreDeactivateMode();
		CurrentAppModePtr->DeactivateMode(TabManager);
		RemoveToolbarExtender(CurrentAppModePtr->GetToolbarExtender());
		RemoveAllToolbarWidgets();

		// Unregister tab spawners
		TabManager->UnregisterAllTabSpawners();
	}

	return true;
}

void FWorkflowCentricApplication::AddApplicationMode(FName ModeName, TSharedRef<FApplicationMode> Mode)
{
	for (int32 Index = 0; Index < ModeExtenderList.Num(); ++Index)
	{
		Mode = ModeExtenderList[Index].Execute(ModeName, Mode);
	}

	ApplicationModeList.Add(ModeName, Mode);
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
