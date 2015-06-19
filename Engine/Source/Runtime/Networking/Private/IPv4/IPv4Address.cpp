// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NetworkingPrivatePCH.h"


/* FIpAddress4 static initialization
 *****************************************************************************/

const FIPv4Address FIPv4Address::Any(0, 0, 0, 0);

const FIPv4Address FIPv4Address::InternalLoopback(127, 0, 0, 1);

const FIPv4Address FIPv4Address::LanBroadcast(255, 255, 255, 255);


/* FIpAddress4 interface
 *****************************************************************************/

FText FIPv4Address::ToText() const
{
	return FText::FromString( FString::Printf(TEXT("%i.%i.%i.%i"), Bytes[3], Bytes[2], Bytes[1], Bytes[0]) );
}


/* FIpAddress4 static interface
 *****************************************************************************/

bool FIPv4Address::Parse( const FString& AddressString, FIPv4Address& OutAddress )
{
	TArray<FString> Tokens;

	if (AddressString.ParseIntoArray(Tokens, TEXT("."), false) == 4)
	{
		OutAddress.Bytes[0] = FCString::Atoi(*Tokens[3]);
		OutAddress.Bytes[1] = FCString::Atoi(*Tokens[2]);
		OutAddress.Bytes[2] = FCString::Atoi(*Tokens[1]);
		OutAddress.Bytes[3] = FCString::Atoi(*Tokens[0]);

		return true;
	}

	return false;
}
