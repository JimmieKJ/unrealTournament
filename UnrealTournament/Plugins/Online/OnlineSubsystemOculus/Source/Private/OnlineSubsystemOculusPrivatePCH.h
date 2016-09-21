// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

#include "Engine.h"

#include "OnlineSubsystemOculus.h"
#include "OnlineSubsystemOculusModule.h"
#include "OnlineSubsystemModule.h"
#include "OnlineSubsystem.h"

#define INVALID_INDEX -1

/** FName declaration of Oculus subsystem */
#define OCULUS_SUBSYSTEM FName(TEXT("Oculus"))
/** URL suffix when using Oculus NetDriver */
#define OCULUS_URL_SUFFIX TEXT(".oculus")

/** pre-pended to all Oculus logging */
#undef ONLINE_LOG_PREFIX
#define ONLINE_LOG_PREFIX TEXT("Oculus: ")

/** Oculus Platform SDK header*/
#include "OVR_Platform.h"
