// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	LinuxPlatformSurvey.h: Linux platform hardware-survey classes
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformSurvey.h"

/**
* Android implementation of FGenericPlatformSurvey
**/
struct FLinuxPlatformSurvey : public FGenericPlatformSurvey
{
	// default implementation for now
};

typedef FLinuxPlatformSurvey FPlatformSurvey;