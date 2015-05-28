// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LauncherServices.h"


/* Dependencies
 *****************************************************************************/

#include "Messaging.h"
#include "ModuleManager.h"
#include "SessionMessages.h"
#include "TargetDeviceServices.h"
#include "TargetPlatform.h"
#include "Ticker.h"
#include "UnrealEdMessages.h"


/* Private includes
 *****************************************************************************/

/** Defines the launcher simple profile file version. */
#define LAUNCHERSERVICES_SIMPLEPROFILEVERSION 1

/** Defines the launcher profile file version. */
#define LAUNCHERSERVICES_PROFILEVERSION 16 
#define LAUNCHERSERVICES_MINPROFILEVERSION 10


// profile manager
#include "LauncherProjectPath.h"
#include "LauncherDeviceGroup.h"
#include "LauncherProfileLaunchRole.h"
#include "LauncherProfile.h"
#include "LauncherProfileManager.h"

// launcher worker
#include "LauncherTaskChainState.h"
#include "LauncherTask.h"
#include "LauncherUATTask.h"
#include "LauncherVerifyProfileTask.h"
#include "LauncherWorker.h"
#include "Launcher.h"
