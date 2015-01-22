// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/* Private dependencies
 *****************************************************************************/

#include "Core.h"
#include "CoreUObject.h"
#include "Messaging.h"
#include "Networking.h"
#include "Sockets.h"
#include "SocketSubsystem.h"


/* Private constants
 *****************************************************************************/

/** Defines the default IP endpoint for multicast traffic. */
#define UDP_MESSAGING_DEFAULT_MULTICAST_ENDPOINT FIPv4Endpoint(FIPv4Address(230, 0, 0, 1), 6666)

/** Defines the maximum number of annotations a message can have. */
#define UDP_MESSAGING_MAX_ANNOTATIONS 128

/** Defines the maximum number of recipients a message can have. */
#define UDP_MESSAGING_MAX_RECIPIENTS 1024

/** Defines the desired size of socket receive buffers (in bytes). */
#define UDP_MESSAGING_RECEIVE_BUFFER_SIZE 2 * 1024 * 1024

/** Defines the protocol version of the UDP message transport. */
#define UDP_MESSAGING_TRANSPORT_PROTOCOL_VERSION 10


/* Private includes
 *****************************************************************************/

// shared
#include "UdpMessageSegment.h"
#include "UdpMessagingSettings.h"

// transport
#include "UdpMessageBeacon.h"
#include "UdpReassembledMessage.h"
#include "UdpMessageResequencer.h"
#include "UdpDeserializedMessage.h"
#include "UdpSerializedMessage.h"
#include "UdpMessageSegmenter.h"
#include "UdpMessageProcessor.h"
#include "UdpSerializeMessageTask.h"
#include "UdpMessageTransport.h"

// tunnel
#include "UdpMessageTunnelConnection.h"
#include "UdpMessageTunnel.h"
