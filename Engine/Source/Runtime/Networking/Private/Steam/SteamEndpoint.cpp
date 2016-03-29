// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NetworkingPrivatePCH.h"


/* FSteamEndpoint interface
 *****************************************************************************/

FString FSteamEndpoint::ToString() const
{
	return FString::Printf(TEXT("0x%llX:%i"), UniqueNetId, SteamChannel);
}
