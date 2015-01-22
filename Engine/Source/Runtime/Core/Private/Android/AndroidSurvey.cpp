// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidSurvey.cpp: HardwareSurvey implementation
=============================================================================*/
#include "CorePrivatePCH.h"
#include "AndroidWindow.h"

bool FAndroidPlatformSurvey::GetSurveyResults(FHardwareSurveyResults& OutResults, bool bWait)
{
	FCString::Strcpy(OutResults.Platform, *(FAndroidMisc::GetDeviceMake() + TEXT("-") + FAndroidMisc::GetDeviceModel()));
	FCString::Strcpy(OutResults.OSVersion, *FAndroidMisc::GetAndroidVersion());
#if PLATFORM_64BITS
	OutResults.OSBits = 64;
#else
	OutResults.OSBits = 32;
#endif
	FCString::Strcpy(OutResults.OSLanguage, *FPlatformMisc::GetDefaultLocale());
	FCString::Strcpy(OutResults.MultimediaAPI, *FAndroidMisc::GetGLVersion());
	OutResults.CPUCount = FPlatformMisc::NumberOfCores();


	// display 0 is max size
	int32 ScreenWidth, ScreenHeight;
	FAndroidWindow::CalculateSurfaceSize(FPlatformMisc::GetHardwareWindow(), ScreenWidth, ScreenHeight);

	OutResults.Displays[0].CurrentModeWidth = ScreenWidth;
	OutResults.Displays[0].CurrentModeHeight = ScreenHeight;
	FCString::Strcpy(OutResults.Displays[0].GPUCardName, *FAndroidMisc::GetGPUFamily());

	// display 1 is current size
	FPlatformRect ViewSize = FAndroidWindow::GetScreenRect();
	OutResults.Displays[1].CurrentModeWidth = ViewSize.Right - ViewSize.Left;
	OutResults.Displays[1].CurrentModeHeight = ViewSize.Bottom - ViewSize.Top;
	return true;
}
