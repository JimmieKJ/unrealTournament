/// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
#include "Lws/LwsWebSocket.h"
typedef FLwsWebSocket FPlatformWebSocket;
#else
#error "IWebSocket is not implemented on this platform yet"
#endif
