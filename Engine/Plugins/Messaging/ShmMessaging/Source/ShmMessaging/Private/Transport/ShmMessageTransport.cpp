// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ShmMessagingPrivatePCH.h"


/* FShmMessageTransport structors
 *****************************************************************************/

FShmMessageTransport::FShmMessageTransport()
{ }


FShmMessageTransport::~FShmMessageTransport()
{
	StopTransport();
}


/* IMessageTransport interface
 *****************************************************************************/

bool FShmMessageTransport::StartTransport()
{
	return false;
}


void FShmMessageTransport::StopTransport()
{
}


bool FShmMessageTransport::TransportMessage(const IMessageContextRef& Context, const TArray<FGuid>& Recipients)
{
	return false;
}
