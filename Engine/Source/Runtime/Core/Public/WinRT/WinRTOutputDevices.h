// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	WinRTOutputDevices.h: Windows platform OutputDevices functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformOutputDevices.h"

struct CORE_API FWinRTOutputDevices : public FGenericPlatformOutputDevices
{
	static FOutputDevice*			GetEventLog();
	static FOutputDeviceConsole*	GetLogConsole();
	static FOutputDeviceError*		GetError();
	static FFeedbackContext*		GetWarn();
};

typedef FWinRTOutputDevices FPlatformOutputDevices;
