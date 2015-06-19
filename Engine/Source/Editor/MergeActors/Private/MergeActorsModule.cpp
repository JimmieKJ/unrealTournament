// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "MergeActorsPrivatePCH.h"
#include "SMergeActorsToolbar.h"
#include "ModuleManager.h"
#include "WorkspaceMenuStructureModule.h"
#include "MeshMergingTool.h"
#include "MeshProxyTool.h"
#include "SDockTab.h"

#define LOCTEXT_NAMESPACE "MergeActorsModule"


static const FName MergeActorsApp = FName("MergeActorsApp");

/**
 * Merge Actors module
 */
class FMergeActorsModule : public IMergeActorsModule
{
public:

	/**
	 * Called right after the module DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule() override;

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule() override;

	/**
	 * Register an IMergeActorsTool with the module, passing ownership to it
	 */
	virtual bool RegisterMergeActorsTool(TUniquePtr<IMergeActorsTool> Tool) override;

	/**
	 * Unregister an IMergeActorsTool with the module
	 */
	virtual bool UnregisterMergeActorsTool(IMergeActorsTool* Tool) override;


private:

	/** Creates the dock tab widget used by the tab manager */
	TSharedRef<SDockTab> CreateMergeActorsTab(const FSpawnTabArgs& Args);

private:

	/** Pointer to the toolbar widget */
	TWeakPtr<SMergeActorsToolbar> MergeActorsToolbarPtr;

	/** List of registered MergeActorsTool instances */
	TArray<TUniquePtr<IMergeActorsTool>> MergeActorsTools;

	/** Whether a nomad tab spawner was registered */
	bool bRegisteredTabSpawner;
};

IMPLEMENT_MODULE(FMergeActorsModule, MergeActors);


TSharedRef<SDockTab> FMergeActorsModule::CreateMergeActorsTab(const FSpawnTabArgs& Args)
{
	// Build array of MergeActorsTool raw pointers
	TArray<IMergeActorsTool*> ToolsToRegister;
	for (const auto& MergeActorTool : MergeActorsTools)
	{
		check(MergeActorTool.Get() != nullptr);
		ToolsToRegister.Add(MergeActorTool.Get());
	}

	// Construct toolbar widget
	TSharedRef<SMergeActorsToolbar> MergeActorsToolbar =
		SNew(SMergeActorsToolbar)
		.ToolsToRegister(ToolsToRegister);

	// Construct dock tab
	TSharedRef<SDockTab> DockTab =
		SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			MergeActorsToolbar
		];

	// Keep weak pointer to toolbar widget
	MergeActorsToolbarPtr = MergeActorsToolbar;

	return DockTab;
}


void FMergeActorsModule::StartupModule()
{
	bRegisteredTabSpawner = false;

	// This is still experimental in the editor, so it's invoked specifically in FMainMenu for now.
	// When no longer experimental, switch to the nomad spawner registration below
	FGlobalTabmanager::Get()->RegisterTabSpawner(MergeActorsApp, FOnSpawnTab::CreateRaw(this, &FMergeActorsModule::CreateMergeActorsTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Merge Actors"))
		.SetTooltipText(LOCTEXT("TooltipText", "Open the Merge Actors tab."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassViewer.TabIcon"));

	bRegisteredTabSpawner = true;

	// Register built-in merging tools straight away
	ensure(RegisterMergeActorsTool(MakeUnique<FMeshMergingTool>()));

	IMeshUtilities* MeshUtilities = FModuleManager::Get().LoadModulePtr<IMeshUtilities>("MeshUtilities");
	if (MeshUtilities != nullptr && MeshUtilities->GetMeshMergingInterface() != nullptr)
	{
		// Only register MeshProxyTool if Simplygon is available
		ensure(RegisterMergeActorsTool(MakeUnique<FMeshProxyTool>()));
	}
}


void FMergeActorsModule::ShutdownModule()
{
	if (!bRegisteredTabSpawner)
	{
		return;
	}

	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterTabSpawner(MergeActorsApp);
	}
}


bool FMergeActorsModule::RegisterMergeActorsTool(TUniquePtr<IMergeActorsTool> Tool)
{
	if (Tool.Get() != nullptr && !MergeActorsTools.Contains(MoveTemp(Tool)))
	{
		MergeActorsTools.Add(MoveTemp(Tool));
		
		// If a tool is added while the toolbar widget is active, update it accordingly
		TSharedPtr<SMergeActorsToolbar> MergeActorsToolbar = MergeActorsToolbarPtr.Pin();
		if (MergeActorsToolbar.IsValid())
		{
			MergeActorsToolbar->AddTool(Tool.Get());
		}

		return true;
	}

	return false;
}


bool FMergeActorsModule::UnregisterMergeActorsTool(IMergeActorsTool* Tool)
{
	if (Tool != nullptr)
	{
		if (MergeActorsTools.RemoveAll([=](const TUniquePtr<IMergeActorsTool>& Ptr) { return Ptr.Get() == Tool; }) > 0)
		{
			TSharedPtr<SMergeActorsToolbar> MergeActorsToolbar = MergeActorsToolbarPtr.Pin();
			if (MergeActorsToolbar.IsValid())
			{
				MergeActorsToolbar->RemoveTool(Tool);
			}

			return true;
		}
	}
	return false;
}


#undef LOCTEXT_NAMESPACE
