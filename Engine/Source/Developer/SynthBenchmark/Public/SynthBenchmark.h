// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"



/**
 * The public interface to this module
 */
class ISynthBenchmark : public IModuleInterface
{

public:

	// @param >0, WorkScale 10 for normal precision and runtime of less than a second
	virtual void Run(FSynthBenchmarkResults& Out, bool bGPUBenchmark = false, float WorkScale = 10.0f) const = 0;

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline ISynthBenchmark& Get()
	{
		return FModuleManager::LoadModuleChecked< ISynthBenchmark >( "SynthBenchmark" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "SynthBenchmark" );
	}
};

