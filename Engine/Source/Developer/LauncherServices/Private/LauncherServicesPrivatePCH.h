// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
