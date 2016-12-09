// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleManager.h"

#include "Http.h"

#include "OnlineSubsystemFacebook.h"
#include "OnlineSubsystemFacebookModule.h"

#if PLATFORM_WINDOWS
#include "Windows/OnlineIdentityFacebook.h"
#include "Windows/OnlineFriendsFacebook.h"
#elif PLATFORM_IOS
#include "IOS/OnlineIdentityFacebook.h"
#include "IOS/OnlineFriendsFacebook.h"
#endif

/** pre-pended to all Facebook logging */
#undef ONLINE_LOG_PREFIX
#define ONLINE_LOG_PREFIX TEXT("Facebook: ")