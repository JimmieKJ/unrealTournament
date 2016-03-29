// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAutomationAnalytics, Log, All);

#include "AutomationWorker.h"


/* Dependencies
 *****************************************************************************/

#include "AutomationMessages.h"
#include "ModuleManager.h"

#if WITH_ENGINE
	#include "Engine.h"
	#include "ImageUtils.h"
	#include "AutomationCommon.h"
#endif


/* Private includes
 *****************************************************************************/

#include "AutomationWorkerModule.h"
#include "AutomationAnalytics.h"
