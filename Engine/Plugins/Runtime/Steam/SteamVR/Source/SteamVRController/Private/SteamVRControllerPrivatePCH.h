// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

// for STEAMVRCONTROLLER_SUPPORTED_PLATFORMS, keep at top
#include "ISteamVRControllerPlugin.h"

#include "Engine.h"
#include "ISteamVRPlugin.h"
#include "InputDevice.h"
#include "IHapticDevice.h"

#if STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
#include <openvr.h>
#endif // STEAMVRCONTROLLER_SUPPORTED_PLATFORMS
