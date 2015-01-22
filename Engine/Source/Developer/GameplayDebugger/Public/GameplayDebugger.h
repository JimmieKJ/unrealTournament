// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

/**
 * The public interface to this module
 */
class IGameplayDebugger : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IGameplayDebugger& Get()
	{
		static FName GamePlayDebuggerModuleName("GameplayDebugger");
		return FModuleManager::LoadModuleChecked< IGameplayDebugger >(GamePlayDebuggerModuleName);
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		static FName GamePlayDebuggerModuleName("GameplayDebugger");
		return FModuleManager::Get().IsModuleLoaded( GamePlayDebuggerModuleName );
	}

	// Each player controller that wants to use gameplay debugging must call this function
	// (generally OnPostInitProperties, but not on a client) in order to create the actor that handles the debugging
	// functionality in a network-replicated (if necessary) fashion.  NOTE: creates an AGameplayDebuggingReplicator
	// in the same World as PlayerController.
	virtual bool CreateGameplayDebuggerForPlayerController(APlayerController* PlayerController) = 0;
};

