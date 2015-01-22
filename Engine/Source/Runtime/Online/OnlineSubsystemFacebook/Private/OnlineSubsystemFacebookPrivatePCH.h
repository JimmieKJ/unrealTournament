// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "ModuleManager.h"

#include "Http.h"
#include "Json.h"

#include "OnlineSubsystemFacebook.h"
#include "OnlineSubsystemFacebookModule.h"

#include "OnlineIdentityFacebook.h"
#include "OnlineFriendsFacebook.h"

/** FName declaration of Facebook subsystem */
#define FACEBOOK_SUBSYSTEM FName(TEXT("Facebook"))

/** pre-pended to all Facebook logging */
#undef ONLINE_LOG_PREFIX
#define ONLINE_LOG_PREFIX TEXT("Facebook: ")