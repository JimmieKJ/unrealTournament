// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "IHeadMountedDisplayModule.h"

#define GEARVR_SUPPORTED_PLATFORMS (PLATFORM_ANDROID && PLATFORM_ANDROID_ARM) || (PLATFORM_WINDOWS && WINVER > 0x0502)

/**
 * The public interface to this module.  In most cases, this interface is only public to sibling modules 
 * within this plugin.
 */
class IGearVRPlugin : public IHeadMountedDisplayModule
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IGearVRPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< IGearVRPlugin >( "GearVR" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "GearVR" );
	}

	/**
	 * Executes Oculus global menu
	 */
	virtual void StartOVRGlobalMenu() const = 0;

	/**
	 * Executes Oculus quit menu
	 */
	virtual void StartOVRQuitMenu() const = 0;

	/**
	 * Sets CPU/GPU levels. Both change in the range [0..3].
	 */
	virtual void SetCPUAndGPULevels(int32 CPULevel, int32 GPULevel) const = 0;

	/**
	 * Returns true if power level state is on minimum
	 */
	virtual bool IsPowerLevelStateMinimum() const = 0;

	/**
	 * Returns true if power level is throttled.
	 */
	virtual bool IsPowerLevelStateThrottled() const = 0;
	
	/**
	 * Returns device temperature, in degrees of Celsius. 
	 */
	virtual float GetTemperatureInCelsius() const = 0;

	/**
	 * Returns battery level, in the range [0..1]
	 */
	virtual float GetBatteryLevel() const = 0;

	/**
	 * Returns true, if head phones are plugged in
	 */
	virtual bool AreHeadPhonesPluggedIn() const = 0;

	/**
	 * Set loading icon texture. When the texture is set it is necessary to call SetLoadingIconMode method, 
	 * or force loading icon rendering by calling RenderLoadingIcon_RenderThread.
	 * To reset to default icon, call this method will 'nullptr' as parameter.
	 */
	virtual void SetLoadingIconTexture(FTextureRHIRef InTexture) = 0;

	/**
	 * Sets/resets 'loading icon mode'. If the 'SetLoadingIconTexture' was called with non-nullptr parameter
	 * then the custom texture will be used; otherwise the default icon is used.
	 */
	virtual void SetLoadingIconMode(bool bActiveLoadingIcon) = 0;

	/**
	 * Enforces loading icon rendering. This method will render it even if loading icon mode wasn't set.
	 * Must be called on render thread only.
	 */
	virtual void RenderLoadingIcon_RenderThread() = 0;

	/**
	 * Returns 'true' if the 'loading icon mode' is on.
	 */
	virtual bool IsInLoadingIconMode() const = 0;
};

