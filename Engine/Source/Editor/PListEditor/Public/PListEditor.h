// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#ifndef __PListEditor_h__
#define __PListEditor_h__

#include "Engine.h"
#include "ModuleInterface.h"

/**
 * Key binding editor module
 */
class FPListEditor : public IModuleInterface
{
public:

	/**
	 * Called right after the module DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule();

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule();

private:


};

#endif
