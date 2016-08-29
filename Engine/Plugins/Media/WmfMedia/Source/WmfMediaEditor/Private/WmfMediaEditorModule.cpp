// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "WmfMediaEditorPCH.h"
#include "ModuleInterface.h"


/**
 * Implements the WmfMediaEditor module.
 */
class FWmfMediaEditorModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
};


IMPLEMENT_MODULE(FWmfMediaEditorModule, WmfMediaEditor);
