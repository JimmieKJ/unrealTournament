// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once


/* Private dependencies
 *****************************************************************************/

#include "Core.h"
#include "CoreUObject.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"
#include "Ticker.h"


/* Private constants
 *****************************************************************************/

/** Defines interval at which the Portal RPC server is being located (in seconds). */
#define PORTAL_RPC_LOCATE_INTERVAL 5.0

/** Defines the time after which Portal RPC servers time out (in seconds). */
#define PORTAL_RPC_LOCATE_TIMEOUT 15.0


/* Private includes
 *****************************************************************************/

