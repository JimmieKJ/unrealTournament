// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "ModuleManager.h"		// For inline LoadModuleChecked()
#include "Runtime/Core/Public/Features/IModularFeatures.h"


/**
 * The public interface of the InputDeviceModule
 */
class IInputDeviceModule : public IModuleInterface, public IModularFeature
{
public:
	static FName GetModularFeatureName()
	{
		static FName FeatureName = FName(TEXT("InputDevice"));
		return FeatureName;
	}

	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature( GetModularFeatureName(), this );
	}

	/**
	 * Singleton-like access to IMovieSceneCore
	 *
	 * @return Returns IInputDeviceModule singleton instance, loading the module on demand if needed
	 */
	static inline IInputDeviceModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IInputDeviceModule >( "InputDevice" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "InputDevice" );
	}

	/**
	 * Attempts to create a new head tracking device interface
	 *
	 * @return	Interface to the new head tracking device, if we were able to successfully create one
	 */
	virtual TSharedPtr< class IInputDevice > CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) = 0;

};
