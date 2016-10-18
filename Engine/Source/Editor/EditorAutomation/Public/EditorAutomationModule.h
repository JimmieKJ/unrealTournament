// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ModuleInterface.h"


/**
 * A module for adding automation exposure in the editor
 */
class IEditorAutomationModule : public IModuleInterface
{
public:
	/**
	* Called right after the module DLL has been loaded and the module object has been created
	*/
	virtual void StartupModule() = 0;

	/**
	* Called before the module is unloaded, right before the module object is destroyed.
	*/
	virtual void ShutdownModule() = 0;
};