// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/* Dependencies
 *****************************************************************************/

#include "Core.h"

#include "IMediaInfo.h"
#include "IMediaPlayer.h"
#include "IMediaSink.h"
#include "IMediaTrack.h"
#include "IMediaTrackAudioDetails.h"
#include "IMediaTrackCaptionDetails.h"
#include "IMediaTrackVideoDetails.h"

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
#include "WmfMediaPlayer.h"
