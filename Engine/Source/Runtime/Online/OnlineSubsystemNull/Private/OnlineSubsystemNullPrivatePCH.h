// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine.h"
#include "OnlineSubsystemNullModule.h"
#include "OnlineSubsystemModule.h"
#include "OnlineSubsystem.h"
#include "ModuleManager.h"

#define INVALID_INDEX -1

/** URL Prefix when using Null socket connection */
#define NULL_URL_PREFIX TEXT("Null.")

/** pre-pended to all NULL logging */
#undef ONLINE_LOG_PREFIX
#define ONLINE_LOG_PREFIX TEXT("NULL: ")


