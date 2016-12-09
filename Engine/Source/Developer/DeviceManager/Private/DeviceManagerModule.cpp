// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Textures/SlateIcon.h"
#include "Widgets/SWidget.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"
#include "Interfaces/ITargetDeviceServicesModule.h"
#include "Interfaces/IDeviceManagerModule.h"
#include "Widgets/SDeviceManager.h"

#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"

static const FName DeviceManagerTabName("DeviceManager");


/**
 * Implements the DeviceManager module.
 */
class FDeviceManagerModule
	: public IDeviceManagerModule
{
public:

	// IModuleInterface interface

	virtual void StartupModule( ) override
	{
		// @todo gmp: implement an IoC container
		ITargetDeviceServicesModule& TargetDeviceServicesModule = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>(TEXT("TargetDeviceServices"));

		TargetDeviceServiceManager = TargetDeviceServicesModule.GetDeviceServiceManager();

		auto& TabSpawnerEntry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(DeviceManagerTabName, FOnSpawnTab::CreateRaw(this, &FDeviceManagerModule::SpawnDeviceManagerTab))
			.SetDisplayName(NSLOCTEXT("FDeviceManagerModule", "DeviceManagerTabTitle", "Device Manager"))
			.SetTooltipText(NSLOCTEXT("FDeviceManagerModule", "DeviceManagerTooltipText", "View and manage connected devices."))
			.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "DeviceDetails.TabIcon"));

#if WITH_EDITOR
		TabSpawnerEntry.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory());
#else
		TabSpawnerEntry.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
#endif
	}

	virtual void ShutdownModule( ) override
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DeviceManagerTabName);
	}

public:

	// IDeviceManagerModule interface

	virtual TSharedRef<SWidget> CreateDeviceManager( const ITargetDeviceServiceManagerRef& DeviceServiceManager, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow ) override
	{
		return SNew(SDeviceManager, DeviceServiceManager, ConstructUnderMajorTab, ConstructUnderWindow);
	}

private:

	/**
	 * Creates a new device manager tab.
	 *
	 * @param SpawnTabArgs The arguments for the tab to spawn.
	 * @return The spawned tab.
	 */
	TSharedRef<SDockTab> SpawnDeviceManagerTab( const FSpawnTabArgs& SpawnTabArgs )
	{
		const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
			.TabRole(ETabRole::MajorTab);

		DockTab->SetContent(CreateDeviceManager(TargetDeviceServiceManager.ToSharedRef(), DockTab, SpawnTabArgs.GetOwnerWindow()));

		return DockTab;
	}

private:

	// @todo gmp: implement an IoC container
	ITargetDeviceServiceManagerPtr TargetDeviceServiceManager;
};


IMPLEMENT_MODULE(FDeviceManagerModule, DeviceManager);
