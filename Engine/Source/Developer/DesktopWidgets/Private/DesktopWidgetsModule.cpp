// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UObject/NameTypes.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


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
