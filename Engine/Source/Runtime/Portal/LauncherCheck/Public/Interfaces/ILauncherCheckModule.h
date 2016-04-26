// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

/** Log categories */
DECLARE_LOG_CATEGORY_EXTERN(LogLauncherCheck, Display, All);

/**
 * Interface for the Launcher checking system
 */
class ILauncherCheckModule
	: public IModuleInterface
{
public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline ILauncherCheckModule& Get()
	{
		return FModuleManager::LoadModuleChecked<ILauncherCheckModule>("LauncherCheck");
	}

	/**
	 * Checks to see if the current app was ran from the Launcher
	 *
	 * @return true, if it was
	 */
	virtual bool WasRanFromLauncher() const = 0;

	/**
	 * Opens the launcher, appending our identifier to the cmdline
	 *
	 * @return true, if it was successful
	 */
	virtual bool RunLauncher() const = 0;

public:

	/** Virtual destructor. */
	virtual ~ILauncherCheckModule() { }
};
