// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/* Public Dependencies
 *****************************************************************************/

#include "CoreUObject.h"


/* Public Includes
 *****************************************************************************/

#include "IMessageAttachment.h"
#include "IMessageContext.h"
#include "IMessageInterceptor.h"
#include "IMessageHandler.h"
#include "IReceiveMessages.h"
#include "ISendMessages.h"
#include "IMessageSubscription.h"
#include "IAuthorizeMessageRecipients.h"
#include "IMessageTracerBreakpoint.h"
#include "IMessageTracer.h"
#include "IMessageBus.h"
#include "IMutableMessageContext.h"
#include "IMessageTransport.h"
#include "IMessageBridge.h"
#include "IMessagingModule.h"


/* Common helpers
 *****************************************************************************/

#include "FileMessageAttachment.h"
#include "MessageBridgeBuilder.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"
