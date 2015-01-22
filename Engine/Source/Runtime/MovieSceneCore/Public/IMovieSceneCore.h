// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "ModuleManager.h"		// For inline LoadModuleChecked()

/**
 * The public interface of the MovieSceneCore module
 */
class IMovieSceneCore : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to IMovieSceneCore
	 *
	 * @return Returns MovieSceneCore singleton instance, loading the module on demand if needed
	 */
	static inline IMovieSceneCore& Get()
	{
		return FModuleManager::LoadModuleChecked< IMovieSceneCore >( "MovieSceneCore" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "MovieSceneCore" );
	}


};

