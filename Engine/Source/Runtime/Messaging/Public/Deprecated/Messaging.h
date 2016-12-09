// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/MonolithicHeaderBoilerplate.h"
MONOLITHIC_HEADER_BOILERPLATE()

#include "CoreUObject.h"

#include "IMessageAttachment.h"
#include "IMessageContext.h"
#include "IMessageInterceptor.h"
#include "IMessageHandler.h"
#include "IMessageReceiver.h"
#include "IMessageSender.h"
#include "IMessageSubscription.h"
#include "IAuthorizeMessageRecipients.h"
#include "IMessageTracerBreakpoint.h"
#include "IMessageTracer.h"
#include "IMessageBus.h"
#include "IMutableMessageContext.h"
#include "IMessageTransport.h"
#include "IMessageBridge.h"
#include "IMessagingModule.h"

#include "Helpers/FileMessageAttachment.h"
#include "Helpers/MessageBridgeBuilder.h"
#include "Helpers/MessageEndpoint.h"
#include "Helpers/MessageEndpointBuilder.h"
