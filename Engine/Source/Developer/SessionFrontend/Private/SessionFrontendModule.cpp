// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SessionFrontendPrivatePCH.h"
#include "ISessionFrontendModule.h"
#include "SDockTab.h"
#include "WorkspaceMenuStructureModule.h"


static const FName SessionFrontendTabName("SessionFrontend");


/**
 * Implements the SessionFrontend module.
 */
class FSessionFrontendModule
	: public ISessionFrontendModule
{
public:

	// ISessionFrontendModule interface
	
	virtual TSharedRef<class SWidget> CreateSessionBrowser( const ISessionManagerRef& SessionManager ) override
	{
		return SNew(SSessionBrowser, SessionManager);
	}
	
	virtual TSharedRef<class SWidget> CreateSessionConsole( const ISessionManagerRef& SessionManager ) override
	{
		return SNew(SSessionConsole, SessionManager);
	}

	virtual void InvokeSessionFrontend(FName SubTabToActivate = NAME_None) override
	{
		FGlobalTabmanager::Get()->InvokeTab(SessionFrontendTabName);
		if ( WeakFrontend.IsValid() )
		{
			if ( SubTabToActivate != NAME_None )
			{
				WeakFrontend.Pin()->GetTabManager()->InvokeTab(SubTabToActivate);
			}
		}
	}
	
public:

	// IModuleInterface interface
	
	virtual void StartupModule() override
	{
		auto& TabSpawnerEntry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SessionFrontendTabName, FOnSpawnTab::CreateRaw(this, &FSessionFrontendModule::SpawnSessionFrontendTab))
			.SetDisplayName(NSLOCTEXT("FSessionFrontendModule", "FrontendTabTitle", "Session Frontend"))
			.SetTooltipText(NSLOCTEXT("FSessionFrontendModule", "FrontendTooltipText", "Open the Session Frontend tab."))
			.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "SessionFrontEnd.TabIcon"));

#if WITH_EDITOR
		TabSpawnerEntry.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory());
#else
		TabSpawnerEntry.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
#endif
	}

	virtual void ShutdownModule() override
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SessionFrontendTabName);
	}

private:

	/**
	 * Creates a new session front-end tab.
	 *
	 * @param SpawnTabArgs The arguments for the tab to spawn.
	 * @return The spawned tab.
	 */
	TSharedRef<SDockTab> SpawnSessionFrontendTab( const FSpawnTabArgs& SpawnTabArgs )
	{
		const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
			.TabRole(ETabRole::MajorTab);

		TSharedRef<SSessionFrontend> Frontend = SNew(SSessionFrontend, DockTab, SpawnTabArgs.GetOwnerWindow());
		WeakFrontend = Frontend;

		DockTab->SetContent(Frontend);

		return DockTab;
	}

private:
	TWeakPtr<SSessionFrontend> WeakFrontend;
};


IMPLEMENT_MODULE(FSessionFrontendModule, SessionFrontend);
