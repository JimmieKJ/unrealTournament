// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystem.h"
#include "OnlineSubsystemAmazon.h"

/** FName declaration of Amazon subsystem */
#define AMAZON_SUBSYSTEM FName(TEXT("Amazon"))

/** pre-pended to all Amazon logging */
#undef ONLINE_LOG_PREFIX
#define ONLINE_LOG_PREFIX TEXT("Amazon: ")


