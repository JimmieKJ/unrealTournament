// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AutomationWindowPrivatePCH.h"


/**
 * Implements the AutomationWindow module.
 */
class FAutomationWindowModule
	: public IAutomationWindowModule
{
public:

	// IAutomationWindowModule interface

	virtual TSharedRef<class SWidget> CreateAutomationWindow( const IAutomationControllerManagerRef& AutomationController, const TSharedRef<ISessionManager>& SessionManager ) override
	{
		return SNew(SAutomationWindow, AutomationController, SessionManager);
	}

	virtual TWeakPtr<class SDockTab> GetAutomationWindowTab( ) override
	{
		return AutomationWindowTabPtr;
	}

	virtual FOnAutomationWindowModuleShutdown& OnShutdown( ) override
	{
		return ShutdownDelegate;
	}

	virtual void SetAutomationWindowTab(TWeakPtr<class SDockTab> AutomationWindowTab) override { AutomationWindowTabPtr = AutomationWindowTab; }

public:

	// IModuleInterface interface

	virtual void StartupModule( ) override
	{
	}

	virtual void ShutdownModule( ) override
	{
		ShutdownDelegate.ExecuteIfBound();	
	}

private:

	// Holds the DockTab for the AutomationWindow
	TWeakPtr<class SDockTab> AutomationWindowTabPtr;

	// Holds FAutomationWindowModuleShutdownCallback
	FOnAutomationWindowModuleShutdown ShutdownDelegate;
};


IMPLEMENT_MODULE(FAutomationWindowModule, AutomationWindow);
