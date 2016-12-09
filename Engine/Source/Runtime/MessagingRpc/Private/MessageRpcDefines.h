// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/* Private constants
 *****************************************************************************/

/** Defines interval at which calls are being re-sent to the server (in seconds). */
#define MESSAGE_RPC_RETRY_INTERVAL 1.0

/** Defines the time after which calls time out (in seconds). */
#define MESSAGE_RPC_RETRY_TIMEOUT 3.0
