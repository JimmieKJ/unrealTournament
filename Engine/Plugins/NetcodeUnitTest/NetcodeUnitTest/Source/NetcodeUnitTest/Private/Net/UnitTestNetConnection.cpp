// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NetcodeUnitTestPCH.h"

#include "Engine.h"
#include "Net/UnrealNetwork.h"
#include "OnlineSubsystemUtilsClasses.h"

#include "NUTUtil.h"
#include "Net/NUTUtilNet.h"

#include "Net/UnitTestNetConnection.h"
#include "Net/UnitTestPackageMap.h"
#include "Net/UnitTestActorChannel.h"

/**
 * Default constructor
 */
UUnitTestNetConnection::UUnitTestNetConnection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LowLevelSendDel()
	, ReceivedRawPacketDel()
{
}

void UUnitTestNetConnection::InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState,
										int32 InMaxPacket/*=0*/, int32 InPacketOverhead/*=0*/)
{
	Super::InitBase(InDriver, InSocket, InURL, InState, InMaxPacket, InPacketOverhead);

	// Replace the package map
#if TARGET_UE4_CL < CL_DEPRECATENEW
	PackageMap = new(this) UUnitTestPackageMap(FObjectInitializer(), this, Driver->GuidCache);
#else
	PackageMap = NewObject<UUnitTestPackageMap>(this);
	Cast<UUnitTestPackageMap>(PackageMap)->Initialize(this, Driver->GuidCache);
#endif
}

void UUnitTestNetConnection::InitConnection(UNetDriver* InDriver, EConnectionState InState, const FURL& InURL,
											int32 InConnectionSpeed/*=0*/)
{
	Super::InitConnection(InDriver, InState, InURL, InConnectionSpeed);

	// Replace the package map
#if TARGET_UE4_CL < CL_DEPRECATENEW
	PackageMap = new(this) UUnitTestPackageMap(FObjectInitializer(), this, Driver->GuidCache);
#else
	PackageMap = NewObject<UUnitTestPackageMap>(this);
	Cast<UUnitTestPackageMap>(PackageMap)->Initialize(this, Driver->GuidCache);
#endif
}

void UUnitTestNetConnection::LowLevelSend(void* Data, int32 Count)
{
	LowLevelSendDel.ExecuteIfBound(Data, Count);

	Super::LowLevelSend(Data, Count);

	GSentBunch = true;
}

void UUnitTestNetConnection::ReceivedRawPacket(void* Data, int32 Count)
{
	GActiveReceiveUnitConnection = this;


	ReceivedRawPacketDel.ExecuteIfBound(Data, Count);


	// Selectively override the actor channel, for unit test net connections (client only creates actor channels, within this call)
	UClass* OrigClass = UChannel::ChannelClasses[CHTYPE_Actor];
	UChannel::ChannelClasses[CHTYPE_Actor] = UUnitTestActorChannel::StaticClass();

	Super::ReceivedRawPacket(Data, Count);

	UChannel::ChannelClasses[CHTYPE_Actor] = OrigClass;


	GActiveReceiveUnitConnection = NULL;
}

void UUnitTestNetConnection::HandleClientPlayer(class APlayerController* PC, class UNetConnection* NetConnection)
{
	// Implement only essential parts of the original function, as we want to block most of it (triggers level change code)
	PC->Role = ROLE_AutonomousProxy;
	PC->NetConnection = NetConnection;

	PlayerController = PC;
	OwningActor = PC;

	// @todo JohnB: This might cause undesirable behaviour, if - for example - HandleDisconnect gets called by
	//				RPC's, so may want to create a fake localplayer instead
	PC->Player = GEngine->GetFirstGamePlayer(NUTUtil::GetPrimaryWorld());

	// Sometimes, e.g. when executing in a commandlet, there is no available player, and one has to be created
	if (PC->Player == NULL)
	{
		// Do nothing, other than just create the raw object
		PC->Player = NewObject<ULocalPlayer>(GEngine, GEngine->LocalPlayerClass);
	}


	// Pass on notification
	UNetDriver* CurDriver = GetDriver();
	FNetworkNotifyHook* NotifyHook = (Driver != NULL ? (FNetworkNotifyHook*)CurDriver->Notify : NULL);

	if (NotifyHook != NULL)
	{
		NotifyHook->NotifyHandleClientPlayer(PC, NetConnection);
	}
}



