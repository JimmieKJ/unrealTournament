// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DesktopWidgetsPrivatePCH.h"
#include "ModuleInterface.h"


static const FName DesktopWidgetsTabName("DesktopWidgets");


/**
 * Implements the DesktopWidgets module.
 */
class FDesktopWidgetsModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface
	
	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
};


IMPLEMENT_MODULE(FDesktopWidgetsModule, DesktopWidgets);
