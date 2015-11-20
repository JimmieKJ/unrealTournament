// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLeapMotion, All, All);

/**
 * The public interface to the Leap Motion Controller plugin module.
 */
class FLeapMotionControllerPlugin : public IModuleInterface, public IModularFeature
{

public:

	// IModuleInterface implementation.
	virtual void StartupModule();
	virtual void ShutdownModule();

	/**
	 * Singleton-like access to this module's interface.
	 * Beware of calling this during the shutdown phase.  The module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FLeapMotionControllerPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< FLeapMotionControllerPlugin >( "LeapMotionController" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "LeapMotionController" );
	}

	FORCEINLINE TSharedPtr<class FLeapMotionDevice> GetLeapDevice() const
	{
		return LeapDevice;
	}

	/**
	 * Simple helper function to get the device currently active.
	 * @returns	Pointer to the LeapMotionDevice, or nullptr if Device is not available.
	 */
	static FLeapMotionDevice* GetLeapDeviceSafe()
	{
#if WITH_EDITOR
		FLeapMotionDevice* LeapDevice = FLeapMotionControllerPlugin::IsAvailable() ? FLeapMotionControllerPlugin::Get().GetLeapDevice().Get() : nullptr;
#else
		FLeapMotionDevice* LeapDevice = FLeapMotionControllerPlugin::Get().GetLeapDevice().Get();
#endif
		return LeapDevice;
	}

protected:
	/**
	 * Reference to the actual Leap Device, grabbed through the GetLeapDevice() interface, and created and destroyed in Startup/ShutdownModule
	 */
	TSharedPtr<class FLeapMotionDevice> LeapDevice;
};

