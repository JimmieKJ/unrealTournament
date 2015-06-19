// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NetworkingPrivatePCH.h"


/* FIpAddress4 interface
 *****************************************************************************/

FText FIPv4SubnetMask::ToText( ) const
{
	return FText::FromString( FString::Printf(TEXT("%i.%i.%i.%i"), Bytes[0], Bytes[1], Bytes[2], Bytes[3]) );
}


/* FIpAddress4 static interface
 *****************************************************************************/

bool FIPv4SubnetMask::Parse( const FString& MaskString, FIPv4SubnetMask& OutMask )
{
	TArray<FString> Tokens;

	if (MaskString.ParseIntoArray(Tokens, TEXT("."), false) == 4)
	{
		OutMask.Bytes[0] = FCString::Atoi(*Tokens[3]);
		OutMask.Bytes[1] = FCString::Atoi(*Tokens[2]);
		OutMask.Bytes[2] = FCString::Atoi(*Tokens[1]);
		OutMask.Bytes[3] = FCString::Atoi(*Tokens[0]);

		return true;
	}

	return false;
}
