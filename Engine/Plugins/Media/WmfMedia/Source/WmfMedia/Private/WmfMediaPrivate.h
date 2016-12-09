// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#define WMFMEDIA_SUPPORTED_PLATFORM (PLATFORM_WINDOWS && WINVER >= _WIN32_WINNT_VISTA)


#include "Runtime/Core/Public/CoreMinimal.h"
#include "Runtime/Core/Public/Modules/ModuleManager.h"

#if WMFMEDIA_SUPPORTED_PLATFORM
	#if WITH_ENGINE
		#include "Runtime/HeadMountedDisplay/Public/IHeadMountedDisplayModule.h"
	#endif

	#include "Runtime/Media/Public/IMediaAudioSink.h"
	#include "Runtime/Media/Public/IMediaModule.h"
	#include "Runtime/Media/Public/IMediaOptions.h"
	#include "Runtime/Media/Public/IMediaOverlaySink.h"
	#include "Runtime/Media/Public/IMediaTextureSink.h"

	#include "../../WmfMediaFactory/Public/WmfMediaSettings.h"

	#include "WindowsHWrapper.h"
	#include "Runtime/Core/Public/Windows/AllowWindowsPlatformTypes.h"
	#include <windows.h>
	#include <mfapi.h>
	#include <mferror.h>
	#include <mfidl.h>
	#include <nserror.h>
	#include <shlwapi.h>
	#include "Runtime/Core/Public/Windows/COMPointer.h"
	#include "Runtime/Core/Public/Windows/HideWindowsPlatformTypes.h"

#endif //WMFMEDIA_SUPPORTED_PLATFORM


/** Log category for the WmfMedia module. */
DECLARE_LOG_CATEGORY_EXTERN(LogWmfMedia, Log, All);

