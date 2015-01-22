// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	GenericPlatformOutputDevices.h: Generic platform OutputDevices classes, mostly implemented with ANSI C++
==============================================================================================*/

#pragma once
#include "HAL/Platform.h"

class FFeedbackContext;
class FOutputDevice;
class FOutputDeviceConsole;
class FOutputDeviceError;
class FString;

/**
* Generic implementation for most platforms
**/
struct CORE_API FGenericPlatformOutputDevices
{
	/**
	 * Adds output devices which can vary depending on platform, configuration, command line parameters.
	 */
	static void							SetupOutputDevices();

	static FString						GetAbsoluteLogFilename();

	static FOutputDevice*				GetLog();
	static FOutputDevice*				GetEventLog()
	{
		return nullptr; // normally only used for dedicated servers
	}
	static FOutputDeviceConsole*		GetLogConsole()
	{
		return nullptr; // normally only used for PC
	}
	static FOutputDeviceError*			GetError();
	static FFeedbackContext*			GetWarn();
};

