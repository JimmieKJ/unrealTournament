// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/* Private dependencies
 *****************************************************************************/

#include "Core.h"
#include "CoreUObject.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"
#include "IMessageRpcClient.h"
#include "IMessageRpcServer.h"
#include "RpcMessage.h"
#include "Ticker.h"


/* Private constants
 *****************************************************************************/

/** Defines interval at which calls are being re-sent to the server (in seconds). */
#define MESSAGE_RPC_RETRY_INTERVAL 1.0

/** Defines the time after which calls time out (in seconds). */
#define MESSAGE_RPC_RETRY_TIMEOUT 3.0


/* Private includes
 *****************************************************************************/

#include "MessageRpcClient.h"
#include "MessageRpcMessages.h"
#include "MessageRpcServer.h"
