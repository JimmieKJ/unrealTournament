// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AutomationControllerModule.cpp: Implements the FAutomationControllerModule class.
=============================================================================*/

#include "AutomationControllerPrivatePCH.h"

#include "ModuleManager.h"

IMPLEMENT_MODULE(FAutomationControllerModule, AutomationController);

IAutomationControllerManagerRef FAutomationControllerModule::GetAutomationController( )
{
	if (!AutomationControllerSingleton.IsValid())
	{
		AutomationControllerSingleton = MakeShareable(new FAutomationControllerManager());
	}

	return AutomationControllerSingleton.ToSharedRef();
}

void FAutomationControllerModule::Init()
{
	GetAutomationController()->Init();
}

void FAutomationControllerModule::Tick()
{
	GetAutomationController()->Tick();
}

void FAutomationControllerModule::ShutdownModule()
{
	GetAutomationController()->Shutdown();
	AutomationControllerSingleton.Reset();
}

void FAutomationControllerModule::StartupModule()
{
	GetAutomationController()->Startup();
}

bool FAutomationControllerModule::SupportsDynamicReloading()
{
	return true;
}

/* Static initialization
 *****************************************************************************/

IAutomationControllerManagerPtr FAutomationControllerModule::AutomationControllerSingleton = NULL;
