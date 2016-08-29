// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "IHeadMountedDisplayModule.h"

// Oculus support is not available on windows xp
#define OCULUS_RIFT_SUPPORTED_PLATFORMS (PLATFORM_WINDOWS && WINVER > 0x0502) // || PLATFORM_MAC

/**
 * The public interface to this module.  In most cases, this interface is only public to sibling modules 
 * within this plugin.
 */
class IOculusRiftPlugin : public IHeadMountedDisplayModule
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IOculusRiftPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< IOculusRiftPlugin >( "OculusRift" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "OculusRift" );
	}

#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	/**
	 * Takes an orientation and position in Oculus Rift coordinate space and converts it to 
	 * Unreal's coordinate system, also applying world scale and base position/orientation offsets
	 */
	virtual bool PoseToOrientationAndPosition( const struct ovrPosef_& Pose, FQuat& OutOrientation, FVector& OutPosition ) const = 0;

	/**
	 * Returns current ovrSession handle
	 */
	virtual class FOvrSessionShared* GetSession() = 0;

	/**
	 * Returns current ovrTrackingState
	 */
	virtual bool GetCurrentTrackingState(struct ovrTrackingState_* TrackingState) = 0;
#endif //OCULUS_RIFT_SUPPORTED_PLATFORMS
};

