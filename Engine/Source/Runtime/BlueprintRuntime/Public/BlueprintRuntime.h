// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

/**
 * The public interface to this module
 */
class IBlueprintRuntime : public IModuleInterface
{
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IBlueprintRuntime& Get()
	{
		return FModuleManager::LoadModuleChecked< IBlueprintRuntime >("BlueprintRuntime");
	}

	virtual void PropagateWarningSettings() = 0;
};

