// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"

#if PLATFORM_WINDOWS

#include "AllowWindowsPlatformTypes.h"

#include <Audiopolicy.h>
#include <Mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <audiodefs.h>
#include <dsound.h>

#include "HideWindowsPlatformTypes.h"

#endif // PLATFORM_WINDOWS

#define PLATFORM_SUPPORTS_VOICE_CAPTURE (PLATFORM_WINDOWS || PLATFORM_MAC)

// Module includes
#include "VoiceModule.h"