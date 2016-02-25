// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

/**
 * Module for Hotfix base implementation
 */
class FHotfixModule : 
	public IModuleInterface
{
private:
// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
