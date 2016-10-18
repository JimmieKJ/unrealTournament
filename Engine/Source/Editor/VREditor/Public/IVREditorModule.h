// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "IVREditorMode.h"

/**
 * The public interface to this module
 */
class IVREditorModule : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IVREditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IVREditorModule >( "VREditor" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "VREditor" );
	}

	/**
	 * Gets the editor mode ID for the VREditor mode
	 *
	 * @return	Editor mode ID (for FEditorModeTools) of the VREditor mode
	 */
	virtual FEditorModeID GetVREditorModeID() const = 0;

	/**
	 * Checks whether or not editor VR features are enabled
	 *
	 * @return	True if VR mode is on
	 */
	virtual bool IsVREditorEnabled() const = 0;

	/**
	 * Checks to see whether its possible to use VR mode in this session.  Basically, this makes sure that you
	 * have the appropriate hardware connected
	 *
	 * @return	True if EnableVREditor() can be used to activate VR mode
	 */
	virtual bool IsVREditorAvailable() const = 0;

	/**
	 * Enables or disables editor VR features.  Calling this to active VR will turn on the HMD and setup
	 * the editor UI for VR interaction.
	 *
	 * @param	bEnable	True to enable VR, or false to turn it off
	 * @param	bForceWithoutHMD	If set to true, will enter VR mode without switching to HMD/stereo.  This can be useful for testing.
	 */
	virtual void EnableVREditor( const bool bEnable, const bool bForceWithoutHMD = false ) = 0;
};

