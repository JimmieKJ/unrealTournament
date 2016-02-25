// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "ModuleManager.h"

#include "Http.h"
#include "Json.h"

#include "OnlineSubsystemFacebook.h"
#include "OnlineSubsystemFacebookModule.h"

#include "OnlineIdentityFacebook.h"
#include "OnlineFriendsFacebook.h"

/** pre-pended to all Facebook logging */
#undef ONLINE_LOG_PREFIX
#define ONLINE_LOG_PREFIX TEXT("Facebook: ")