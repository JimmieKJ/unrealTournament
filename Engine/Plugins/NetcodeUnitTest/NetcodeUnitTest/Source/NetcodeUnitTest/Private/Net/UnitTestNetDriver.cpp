// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NetcodeUnitTestPCH.h"

#include "Net/UnitTestNetDriver.h"
#include "Net/UnitTestNetConnection.h"
#include "Net/UnitTestChannel.h"

/**
 * Default constructor
 */
UUnitTestNetDriver::UUnitTestNetDriver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/**
 * @todo JohnB
 */
bool UUnitTestNetDriver::InitConnectionClass()
{
	bool bReturnVal = false;

	if (NetConnectionClass == NULL)
	{
		NetConnectionClass = UUnitTestNetConnection::StaticClass();
		bReturnVal = true;
	}

	if (NetConnectionClass == NULL)
	{
		bReturnVal = Super::InitConnectionClass();
	}

	return bReturnVal;
}

/**
 * @todo JohnB
 */
bool UUnitTestNetDriver::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	bool bReturnVal = false;

	// Hack: Replace the control channel with a stripped down unit test channel
	UClass* OrigClass = UChannel::ChannelClasses[CHTYPE_Control];
	UChannel::ChannelClasses[CHTYPE_Control] = UUnitTestChannel::StaticClass();

	bReturnVal = Super::InitConnect(InNotify, ConnectURL, Error);

	UChannel::ChannelClasses[CHTYPE_Control] = OrigClass;

	return bReturnVal;
}

void UUnitTestNetDriver::ProcessRemoteFunction(AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms,
												FFrame* Stack, UObject* SubObject)
{
	bool bAllowRPC = true;

	if (SendRPCDel.IsBound())
	{
		bAllowRPC = SendRPCDel.Execute(Actor, Function, Parameters, OutParms, Stack, SubObject);
	}

	if (bAllowRPC)
	{
		Super::ProcessRemoteFunction(Actor, Function, Parameters, OutParms, Stack, SubObject);
	}
}




