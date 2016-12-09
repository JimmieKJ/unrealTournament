// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_IOS
	#include "IOS/IOSPlatformTextField.h"
#elif PLATFORM_ANDROID
	#include "Android/AndroidPlatformTextField.h"
#elif PLATFORM_PS4
	#include "PS4/PS4PlatformTextField.h"
#elif PLATFORM_XBOXONE
	#include "XboxOne/XboxOnePlatformTextField.h"
#else
	#include "Framework/Text/GenericPlatformTextField.h"
#endif
