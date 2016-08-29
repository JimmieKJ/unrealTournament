// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/Core/Public/Core.h"
#include "Runtime/Core/Public/Containers/TripleBuffer.h"
#include "Runtime/Core/Public/Modules/ModuleManager.h"
#include "Runtime/CoreUObject/Public/CoreUObject.h"
#include "Runtime/Engine/Classes/DeviceProfiles/DeviceProfile.h"
#include "Runtime/Engine/Public/Engine.h"
#include "Runtime/Engine/Public/GlobalShader.h"
#include "Runtime/Media/Public/IMediaControls.h"
#include "Runtime/Media/Public/IMediaModule.h"
#include "Runtime/Media/Public/IMediaOptions.h"
#include "Runtime/Media/Public/IMediaOutput.h"
#include "Runtime/Media/Public/IMediaPlayer.h"
#include "Runtime/Media/Public/IMediaPlayerFactory.h"
#include "Runtime/Media/Public/IMediaTracks.h"
#include "Runtime/RenderCore/Public/RenderingThread.h"
#include "Runtime/UtilityShaders/Public/MediaShaders.h"

#if WITH_EDITORONLY_DATA
	#include "Developer/TargetPlatform/Public/TargetPlatform.h"
#endif


/** Declares a log category for this module. */
DECLARE_LOG_CATEGORY_EXTERN(LogMediaAssets, Log, All);
