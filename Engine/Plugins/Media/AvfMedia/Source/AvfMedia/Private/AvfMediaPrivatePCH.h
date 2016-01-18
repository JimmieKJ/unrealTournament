// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/* Private dependencies
 *****************************************************************************/

#include "Core.h"
#include "IMediaSink.h"
#include "IMediaStream.h"
#include "IMediaAudioTrack.h"
#include "IMediaCaptionTrack.h"
#include "IMediaVideoTrack.h"

#import <AVFoundation/AVFoundation.h>


/* Private macros
 *****************************************************************************/

DECLARE_LOG_CATEGORY_EXTERN(LogAvfMedia, Log, All);


/* Private includes
 *****************************************************************************/

#include "Player/AvfMediaPlayer.h"
#include "Tracks/AvfMediaTrack.h"
#include "Tracks/AvfMediaAudioTrack.h"
#include "Tracks/AvfMediaCaptionTrack.h"
#include "Tracks/AvfMediaVideoTrack.h"
