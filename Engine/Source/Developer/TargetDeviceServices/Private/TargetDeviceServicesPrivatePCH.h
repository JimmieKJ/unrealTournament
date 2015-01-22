// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "TargetDeviceServices.h"


/* Dependencies
 *****************************************************************************/

#include "CoreUObject.h"
#include "Messaging.h"
#include "ModuleManager.h"
#include "TargetPlatform.h"
#include "Ticker.h"


/* Private declarations
 *****************************************************************************/

/** Declares the module's log category. */
DECLARE_LOG_CATEGORY_EXTERN(TargetDeviceServicesLog, Log, All);

/** Defines the interval in seconds in which devices are being pinged by the proxy manager. */
#define TARGET_DEVICE_SERVICES_PING_INTERVAL 2.5f


/* Private includes
 *****************************************************************************/

#include "TargetDeviceServiceMessages.h"
#include "TargetDeviceProxy.h"
#include "TargetDeviceProxyManager.h"
#include "TargetDeviceService.h"
#include "TargetDeviceServiceManager.h"
