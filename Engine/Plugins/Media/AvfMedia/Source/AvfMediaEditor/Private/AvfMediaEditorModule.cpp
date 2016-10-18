// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AvfMediaEditorPCH.h"
#include "ModuleInterface.h"


/**
 * Implements the AvfMediaEditor module.
 */
class FAvfMediaEditorModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
};


IMPLEMENT_MODULE(FAvfMediaEditorModule, AvfMediaEditor);
