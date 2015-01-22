// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IAndroidDeviceDetectionModule.h: Declares the IAndroidDeviceDetectionModule interface.
=============================================================================*/

#pragma once

/**
 * Interface for AndroidDeviceDetection module.
 */
class IAndroidDeviceDetectionModule
	: public IModuleInterface
{
public:
	/**
	 * Returns the android device detection singleton.
	 */
	virtual IAndroidDeviceDetection* GetAndroidDeviceDetection() = 0;

protected:

	/**
	 * Virtual destructor
	 */
	virtual ~IAndroidDeviceDetectionModule( ) { }
};
