// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	WinRTSurvey.h: WinRT platform hardware-survey classes
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformSurvey.h"

/**
* WinRT implementation of FGenericPlatformSurvey
**/
struct FWinRTPlatformSurvey : public FGenericPlatformSurvey
{
	// default implementation for now
};

typedef FWinRTPlatformSurvey FPlatformSurvey;