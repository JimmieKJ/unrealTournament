// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


/**
 * Implements the Slate module.
 */
class FSlateModule
	: public IModuleInterface
{ };


IMPLEMENT_MODULE(FSlateModule, Slate);
