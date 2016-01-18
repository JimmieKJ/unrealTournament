// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

/** Logging related to parties */
Presence_API DECLARE_LOG_CATEGORY_EXTERN(LogPresence, Display, All);

/**
 * Module for presence related implementations
 */
class FPresenceModule : 
	public IModuleInterface, public FSelfRegisteringExec
{

public:

	// FSelfRegisteringExec
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FPresenceModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FPresenceModule>("Presence");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("Presence");
	}

private:

	// IModuleInterface

	/**
	 * Called when presence module is loaded
	 * Initialize platform specific parts of Presence handling
	 */
	virtual void StartupModule() override;
	
	/**
	 * Called when presence module is unloaded
	 * Shutdown platform specific parts of Presence handling
	 */
	virtual void ShutdownModule() override;
};

