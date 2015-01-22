// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "CrashDebugHelper.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCrashDebugHelper, Log, All);

#if PLATFORM_WINDOWS
	#include "Windows/CrashDebugHelperWindows.h"
#endif

#if PLATFORM_LINUX
	#include "Linux/CrashDebugHelperLinux.h"
#endif

#if PLATFORM_MAC
	#include "Mac/CrashDebugHelperMac.h"
#endif
