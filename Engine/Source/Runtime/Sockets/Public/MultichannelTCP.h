// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SocketSubsystem.h"
#include "Sockets.h"


SOCKETS_API DECLARE_LOG_CATEGORY_EXTERN(LogMultichannelTCP, Log, All);


/** Magic number used to verify packet header **/
enum
{
	MultichannelMagic= 0xa692339f
};


#include "NetworkMessage.h"
#include "MultichannelTcpReceiver.h"
#include "MultichannelTcpSender.h"
#include "MultichannelTcpSocket.h"