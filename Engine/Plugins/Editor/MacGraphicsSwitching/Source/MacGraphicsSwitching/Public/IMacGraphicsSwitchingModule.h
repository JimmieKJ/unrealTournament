// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

/**
 * The public interface to this module
 */
class IMacGraphicsSwitchingModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IMacGraphicsSwitchingModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IMacGraphicsSwitchingModule >( "MacGraphicsSwitching" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "MacGraphicsSwitching" );
	}
	
	/**
	 * Tests whether this Mac can support automatic graphics switching.
	 * @return True if automatic graphics switching is allowed on this Mac.
	 */
	virtual bool AllowAutomaticGraphicsSwitching() { return false; }
	
	/**
	 * Tests whether this Mac can support rendering to displays via multiple GPUs.
	 * @return True if multiple GPU support is allowed on this Mac.
	 */
	virtual bool AllowMultipleGPUs() { return false; }
};

