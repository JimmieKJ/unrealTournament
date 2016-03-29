// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/* Dependencies
 *****************************************************************************/

#include "Core.h"

#include "IMediaInfo.h"
#include "IMediaPlayer.h"
#include "IMediaSink.h"
#include "IMediaStream.h"
#include "IMediaAudioTrack.h"
#include "IMediaCaptionTrack.h"
#include "IMediaVideoTrack.h"

#include "AllowWindowsPlatformTypes.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <shlwapi.h>
#include "COMPointer.h"
#include "HideWindowsPlatformTypes.h"


/* Private macros
 *****************************************************************************/

DECLARE_LOG_CATEGORY_EXTERN(LogWmfMedia, Log, All);


/* Private includes
 *****************************************************************************/

#include "WmfMediaSampler.h"
#include "WmfMediaTrack.h"
#include "WmfMediaAudioTrack.h"
#include "WmfMediaCaptionTrack.h"
#include "WmfMediaVideoTrack.h"
#include "WmfMediaReadState.h"
#include "WmfMediaByteStream.h"
#include "WmfMediaSession.h"
#include "WmfMediaResolveState.h"
#include "WmfMediaResolver.h"
#include "WmfMediaPlayer.h"
