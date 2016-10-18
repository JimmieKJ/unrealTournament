// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NetcodeUnitTestPCH.h"

#include "Net/UnitTestChannel.h"
#include "Net/UnitTestNetDriver.h"

#include "NUTUtil.h"
#include "UnitTestEnvironment.h"
#include "Net/NUTUtilNet.h"

#include "UnitTest.h"

#include "UnrealNetwork.h"
#include "EngineVersion.h"
#include "DataChannel.h"
#include "OnlineBeaconClient.h"
#include "NetworkVersion.h"
#include "OnlineSubsystemTypes.h"

// Forward declarations
class FWorldTickHook;


/** Active unit test worlds */
TArray<UWorld*> UnitTestWorlds;

/** Unit testing worlds, pending cleanup */
TArray<UWorld*> PendingUnitWorldCleanup;

/** Active world tick hooks */
TArray<FWorldTickHook*> ActiveTickHooks;


/**
 * FSocketHook
 */

bool FSocketHook::Close()
{
	return HookedSocket->Close();
}

bool FSocketHook::Bind(const FInternetAddr& Addr)
{
	return HookedSocket->Bind(Addr);
}

bool FSocketHook::Connect(const FInternetAddr& Addr)
{
	return HookedSocket->Connect(Addr);
}

bool FSocketHook::Listen(int32 MaxBacklog)
{
	return HookedSocket->Listen(MaxBacklog);
}

bool FSocketHook::HasPendingConnection(bool& bHasPendingConnection)
{
	return HookedSocket->HasPendingConnection(bHasPendingConnection);
}

bool FSocketHook::HasPendingData(uint32& PendingDataSize)
{
	return HookedSocket->HasPendingData(PendingDataSize);
}

class FSocket* FSocketHook::Accept(const FString& InSocketDescription)
{
	return HookedSocket->Accept(InSocketDescription);
}

class FSocket* FSocketHook::Accept(FInternetAddr& OutAddr, const FString& InSocketDescription)
{
	return HookedSocket->Accept(OutAddr, InSocketDescription);
}

bool FSocketHook::SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination)
{
	bool bReturnVal = false;
	bool bBlockSend = false;

	SendToDel.ExecuteIfBound((void*)Data, Count, bBlockSend);

	if (!bBlockSend)
	{
		bReturnVal = HookedSocket->SendTo(Data, Count, BytesSent, Destination);
	}

	bLastSendToBlocked = bBlockSend;

	return bReturnVal;
}

bool FSocketHook::Send(const uint8* Data, int32 Count, int32& BytesSent)
{
	return HookedSocket->Send(Data, Count, BytesSent);
}

bool FSocketHook::RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags)
{
	return HookedSocket->RecvFrom(Data, BufferSize, BytesRead, Source, Flags);
}

bool FSocketHook::Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags)
{
	return HookedSocket->Recv(Data, BufferSize, BytesRead, Flags);
}

bool FSocketHook::Wait(ESocketWaitConditions::Type Condition, FTimespan WaitTime)
{
	return HookedSocket->Wait(Condition, WaitTime);
}

ESocketConnectionState FSocketHook::GetConnectionState()
{
	return HookedSocket->GetConnectionState();
}

void FSocketHook::GetAddress(FInternetAddr& OutAddr)
{
	HookedSocket->GetAddress(OutAddr);
}

bool FSocketHook::GetPeerAddress(FInternetAddr& OutAddr)
{
	return HookedSocket->GetPeerAddress(OutAddr);
}

bool FSocketHook::SetNonBlocking(bool bIsNonBlocking)
{
	return HookedSocket->SetNonBlocking(bIsNonBlocking);
}

bool FSocketHook::SetBroadcast(bool bAllowBroadcast)
{
	return HookedSocket->SetBroadcast(bAllowBroadcast);
}

bool FSocketHook::JoinMulticastGroup(const FInternetAddr& GroupAddress)
{
	return HookedSocket->JoinMulticastGroup(GroupAddress);
}

bool FSocketHook::LeaveMulticastGroup(const FInternetAddr& GroupAddress)
{
	return HookedSocket->LeaveMulticastGroup(GroupAddress);
}

bool FSocketHook::SetMulticastLoopback(bool bLoopback)
{
	return HookedSocket->SetMulticastLoopback(bLoopback);
}

bool FSocketHook::SetMulticastTtl(uint8 TimeToLive)
{
	return HookedSocket->SetMulticastTtl(TimeToLive);
}

bool FSocketHook::SetReuseAddr(bool bAllowReuse)
{
	return HookedSocket->SetReuseAddr(bAllowReuse);
}

bool FSocketHook::SetLinger(bool bShouldLinger, int32 Timeout)
{
	return HookedSocket->SetLinger(bShouldLinger, Timeout);
}

bool FSocketHook::SetRecvErr(bool bUseErrorQueue)
{
	return HookedSocket->SetRecvErr(bUseErrorQueue);
}

bool FSocketHook::SetSendBufferSize(int32 Size, int32& NewSize)
{
	return HookedSocket->SetSendBufferSize(Size, NewSize);
}

bool FSocketHook::SetReceiveBufferSize(int32 Size, int32& NewSize)
{
	return HookedSocket->SetReceiveBufferSize(Size, NewSize);
}

int32 FSocketHook::GetPortNo()
{
	return HookedSocket->GetPortNo();
}


/**
 * FNetworkNotifyHook
 */

void FNetworkNotifyHook::NotifyHandleClientPlayer(APlayerController* PC, UNetConnection* Connection)
{
	NotifyHandleClientPlayerDelegate.ExecuteIfBound(PC, Connection);
}

EAcceptConnection::Type FNetworkNotifyHook::NotifyAcceptingConnection()
{
	EAcceptConnection::Type ReturnVal = EAcceptConnection::Ignore;

	if (NotifyAcceptingConnectionDelegate.IsBound())
	{
		ReturnVal = NotifyAcceptingConnectionDelegate.Execute();
	}

	// Until I have a use-case for doing otherwise, make the original authoritative
	if (HookedNotify != NULL)
	{
		ReturnVal = HookedNotify->NotifyAcceptingConnection();
	}

	return ReturnVal;
}

void FNetworkNotifyHook::NotifyAcceptedConnection(UNetConnection* Connection)
{
	NotifyAcceptedConnectionDelegate.ExecuteIfBound(Connection);

	if (HookedNotify != NULL)
	{
		HookedNotify->NotifyAcceptedConnection(Connection);
	}
}

bool FNetworkNotifyHook::NotifyAcceptingChannel(UChannel* Channel)
{
	bool bReturnVal = false;

	if (NotifyAcceptingChannelDelegate.IsBound())
	{
		bReturnVal = NotifyAcceptingChannelDelegate.Execute(Channel);
	}

	// Until I have a use-case for doing otherwise, make the original authoritative
	if (HookedNotify != NULL)
	{
		bReturnVal = HookedNotify->NotifyAcceptingChannel(Channel);
	}

	return bReturnVal;
}

void FNetworkNotifyHook::NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, FInBunch& Bunch)
{
	bool bHandled = false;

	if (NotifyControlMessageDelegate.IsBound())
	{
		bHandled = NotifyControlMessageDelegate.Execute(Connection, MessageType, Bunch);
	}

	// Only pass on to original, if the control message was not handled
	if (!bHandled && HookedNotify != NULL)
	{
		HookedNotify->NotifyControlMessage(Connection, MessageType, Bunch);
	}
}


/**
 * NUTNet
 */

int32 NUTNet::GetFreeChannelIndex(UNetConnection* InConnection, int32 InChIndex/*=INDEX_NONE*/)
{
	int32 ReturnVal = INDEX_NONE;

	for (int i=(InChIndex == INDEX_NONE ? 0 : InChIndex); i<ARRAY_COUNT(InConnection->Channels); i++)
	{
		if (InConnection->Channels[i] == NULL)
		{
			ReturnVal = i;
			break;
		}
	}

	return ReturnVal;
}

UUnitTestChannel* NUTNet::CreateUnitTestChannel(UNetConnection* InConnection, EChannelType InType, int32 InChIndex/*=INDEX_NONE*/,
												bool bInVerifyOpen/*=false*/)
{
	UUnitTestChannel* ReturnVal = NULL;

	if (InChIndex == INDEX_NONE)
	{
		InChIndex = GetFreeChannelIndex(InConnection);
	}

	if (InConnection != NULL && InChIndex >= 0 && InChIndex < ARRAY_COUNT(InConnection->Channels) &&
		InConnection->Channels[InChIndex] == NULL)
	{
		ReturnVal = NewObject<UUnitTestChannel>();
		ReturnVal->ChType = InType;

		// Always treat as not opened locally
		ReturnVal->Init(InConnection, InChIndex, false);
		ReturnVal->bVerifyOpen = bInVerifyOpen;

		InConnection->Channels[InChIndex] = ReturnVal;
		InConnection->OpenChannels.Add(ReturnVal);

		UE_LOG(LogUnitTest, Log, TEXT("Created unit test channel of type '%i' on ChIndex '%i'"), (int)InType, InChIndex);
	}
	else if (InConnection == NULL)
	{
		UE_LOG(LogUnitTest, Log, TEXT("Failed to create unit test channel: InConnection == NULL"));
	}
	else if (InChIndex < 0 || InChIndex >= ARRAY_COUNT(InConnection->Channels))
	{
		UE_LOG(LogUnitTest, Log, TEXT("Failed to create unit test channel: InChIndex <= 0 || >= channel count (InchIndex: %i)"),
				InChIndex);
	}
	else if (InConnection->Channels[InChIndex] != NULL)
	{
		UE_LOG(LogUnitTest, Log, TEXT("Failed to create unit test channel: Channel index already set"));
	}

	return ReturnVal;
}

FOutBunch* NUTNet::CreateChannelBunch(int32& BunchSequence, UNetConnection* InConnection, EChannelType InChType,
											int32 InChIndex/*=INDEX_NONE*/, bool bGetNextFreeChan/*=false*/)
{
	FOutBunch* ReturnVal = NULL;

	BunchSequence++;

	// If the channel already exists, the input sequence needs to be overridden, and the channel sequence incremented
	if (InChIndex != INDEX_NONE && InConnection != NULL && InConnection->Channels[InChIndex] != NULL)
	{
		BunchSequence = ++InConnection->OutReliable[InChIndex];
	}

	if (InConnection != NULL && InConnection->Channels[0] != NULL)
	{
		ReturnVal = new FOutBunch(InConnection->Channels[0], false);

		// Fix for uninitialized members (just one is causing a problem, but may as well do them all)
		ReturnVal->Next = NULL;
		ReturnVal->Time = 0.0;
		ReturnVal->ReceivedAck = false; // This one was the problem - was triggering an assert
		ReturnVal->ChSequence = 0;
		ReturnVal->PacketId = 0;
		ReturnVal->bDormant = false;
	}


	if (ReturnVal != NULL)
	{
		if (InChIndex == INDEX_NONE || bGetNextFreeChan)
		{
			InChIndex = GetFreeChannelIndex(InConnection, InChIndex);
		}

		ReturnVal->Channel = NULL;
		ReturnVal->ChIndex = InChIndex;
		ReturnVal->ChType = InChType;


		// NOTE: Might not cover all bOpen or 'channel already open' cases
		ReturnVal->bOpen = (uint8)(InConnection == NULL || InConnection->Channels[InChIndex] == NULL ||
							InConnection->Channels[InChIndex]->OpenPacketId.First == INDEX_NONE);

		if (ReturnVal->bOpen && InConnection != NULL && InConnection->Channels[InChIndex] != NULL)
		{
			InConnection->Channels[InChIndex]->OpenPacketId.First = BunchSequence;
			InConnection->Channels[InChIndex]->OpenPacketId.Last = BunchSequence;
		}


		ReturnVal->bReliable = 1;
		ReturnVal->ChSequence = BunchSequence;
	}

	return ReturnVal;
}

void NUTNet::SendControlBunch(UNetConnection* InConnection, FOutBunch& ControlChanBunch)
{
	if (InConnection != NULL && InConnection->Channels[0] != NULL)
	{
		// Since it's the unit test control channel, sending the packet abnormally, append to OutRec manually
		if (ControlChanBunch.bReliable)
		{
			for (FOutBunch* CurOut=InConnection->Channels[0]->OutRec; CurOut!=NULL; CurOut=CurOut->Next)
			{
				if (CurOut->Next == NULL)
				{
					CurOut->Next = &ControlChanBunch;
					break;
				}
			}
		}

		InConnection->SendRawBunch(ControlChanBunch, true);
	}
}

UUnitTestNetDriver* NUTNet::CreateUnitTestNetDriver(UWorld* InWorld)
{
	UUnitTestNetDriver* ReturnVal = NULL;
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);

	if (GameEngine != NULL)
	{
		static int UnitTestNetDriverCount = 0;

		// Setup a new driver name entry
		bool bFoundDef = false;
		FName UnitDefName = TEXT("UnitTestNetDriver");

		for (int i=0; i<GameEngine->NetDriverDefinitions.Num(); i++)
		{
			if (GameEngine->NetDriverDefinitions[i].DefName == UnitDefName)
			{
				bFoundDef = true;
				break;
			}
		}

		if (!bFoundDef)
		{
			FNetDriverDefinition NewDriverEntry;

			NewDriverEntry.DefName = UnitDefName;
			NewDriverEntry.DriverClassName = *UUnitTestNetDriver::StaticClass()->GetPathName();
			NewDriverEntry.DriverClassNameFallback = *UIpNetDriver::StaticClass()->GetPathName();

			GameEngine->NetDriverDefinitions.Add(NewDriverEntry);
		}


		FName NewDriverName = *FString::Printf(TEXT("UnitTestNetDriver_%i"), UnitTestNetDriverCount++);

		// Now create a reference to the driver
		if (GameEngine->CreateNamedNetDriver(InWorld, NewDriverName, UnitDefName))
		{
			ReturnVal = Cast<UUnitTestNetDriver>(GameEngine->FindNamedNetDriver(InWorld, NewDriverName));
		}


		if (ReturnVal != NULL)
		{
			ReturnVal->SetWorld(InWorld);
			InWorld->SetNetDriver(ReturnVal);

			UE_LOG(LogUnitTest, Log, TEXT("CreateUnitTestNetDriver: Created named net driver: %s, NetDriverName: %s"),
					*ReturnVal->GetFullName(), *ReturnVal->NetDriverName.ToString());
		}
		else
		{
			UE_LOG(LogUnitTest, Log, TEXT("CreateUnitTestNetDriver: CreateNamedNetDriver failed"));
		}
	}
	else
	{
		UE_LOG(LogUnitTest, Log, TEXT("CreateUnitTestNetDriver: GameEngine is NULL"));
	}

	return ReturnVal;
}

bool NUTNet::CreateFakePlayer(UWorld* InWorld, UNetDriver*& InNetDriver, FString ServerIP, FNetworkNotify* InNotify/*=NULL*/,
								bool bSkipJoin/*=false*/, FUniqueNetIdRepl* InNetID/*=NULL*/, bool bBeaconConnect/*=false*/,
								FString InBeaconType/*=TEXT("")*/)
{
	bool bSuccess = false;

	if (InNetDriver == NULL)
	{
		InNetDriver = (InWorld != NULL ? NUTNet::CreateUnitTestNetDriver(InWorld) : NULL);
	}

	if (InNetDriver != NULL)
	{
		if (InNotify == NULL)
		{
			FNetworkNotifyHook* NotifyHook = new FNetworkNotifyHook();
			InNotify = NotifyHook;

			auto DefaultRejectChan = [](UChannel* Channel)
				{
					UE_LOG(LogUnitTest, Log, TEXT("UnitTestNetDriver: NotifyAcceptingChannel: %s"), *Channel->Describe());

					return false;
				};

			NotifyHook->NotifyAcceptingChannelDelegate.BindLambda(DefaultRejectChan);
		}

		FURL DefaultURL;
		FURL TravelURL(&DefaultURL, *ServerIP, TRAVEL_Absolute);
		FString ConnectionError;

		if (InNetDriver->InitConnect(InNotify, TravelURL, ConnectionError))
		{
			UNetConnection* TargetConn = InNetDriver->ServerConnection;

			UE_LOG(LogUnitTest, Log, TEXT("Successfully kicked off connect to IP '%s'"), *ServerIP);

#if TARGET_UE4_CL >= CL_STATELESSCONNECT
			if (TargetConn->StatelessConnectComponent.IsValid())
			{
				TargetConn->StatelessConnectComponent.Pin()->SendInitialConnect();
			}
#endif


			int ControlBunchSequence = 0;

			FOutBunch* ControlChanBunch = NUTNet::CreateChannelBunch(ControlBunchSequence, TargetConn, CHTYPE_Control, 0);

			// Need to send 'NMT_Hello' to start off the connection (the challenge is not replied to)
			uint8 IsLittleEndian = uint8(PLATFORM_LITTLE_ENDIAN);

			// We need to construct the NMT_Hello packet manually, for the initial connection
			uint8 MessageType = NMT_Hello;

			*ControlChanBunch << MessageType;
			*ControlChanBunch << IsLittleEndian;

			uint32 LocalNetworkVersion = FNetworkVersion::GetLocalNetworkVersion();
			*ControlChanBunch << LocalNetworkVersion;

			if (bBeaconConnect)
			{
				if (!bSkipJoin)
				{
					MessageType = NMT_BeaconJoin;
					*ControlChanBunch << MessageType;
					*ControlChanBunch << InBeaconType;

					// Also immediately ack the beacon GUID setup; we're just going to let the server setup the client beacon,
					// through the actor channel
					MessageType = NMT_BeaconNetGUIDAck;
					*ControlChanBunch << MessageType;
					*ControlChanBunch << InBeaconType;
				}
			}
			else
			{
				// Then send NMT_Login
#if TARGET_UE4_CL < CL_CONSTUNIQUEID
#else
				TSharedPtr<const FUniqueNetId> DudPtr = MakeShareable(new FUniqueNetIdString(TEXT("Dud")));
#endif

				FUniqueNetIdRepl PlayerUID(DudPtr);
				FString BlankStr = TEXT("");
				FString ConnectURL = UUnitTest::UnitEnv->GetDefaultClientConnectURL();

				if (InNetID != NULL)
				{
					PlayerUID = *InNetID;
				}

				MessageType = NMT_Login;
				*ControlChanBunch << MessageType;
				*ControlChanBunch << BlankStr;
				*ControlChanBunch << ConnectURL;
				*ControlChanBunch << PlayerUID;


				// Now send NMT_Join, to trigger a fake player, which should then trigger replication of basic actor channels
				if (!bSkipJoin)
				{
					MessageType = NMT_Join;
					*ControlChanBunch << MessageType;
				}
			}


			// Big hack: Store OutRec value on the unit test control channel, to enable 'retry-send' code
			TargetConn->Channels[0]->OutRec = ControlChanBunch;

			TargetConn->SendRawBunch(*ControlChanBunch, true);


			bSuccess = true;

		}
		else
		{
			UE_LOG(LogUnitTest, Log, TEXT("Failed to kickoff connect to IP '%s', error: %s"), *ServerIP, *ConnectionError);
		}
	}
	else if (InNetDriver == NULL)
	{
		UE_LOG(LogUnitTest, Log, TEXT("Failed to create an instance of the unit test net driver"));
	}
	else
	{
		UE_LOG(LogUnitTest, Log, TEXT("Failed to get a reference to WorldContext"));
	}

	return bSuccess;
}

void NUTNet::DisconnectFakePlayer(UWorld* PlayerWorld, UNetDriver* InNetDriver)
{
	GEngine->DestroyNamedNetDriver(PlayerWorld, InNetDriver->NetDriverName);
}

void NUTNet::HandleBeaconReplicate(AOnlineBeaconClient* InBeacon, UNetConnection* InConnection)
{
	// Due to the way the beacon is created in unit tests (replicated, instead of taking over a local beacon client),
	// the NetDriver and BeaconConnection values have to be hack-set, to enable sending of RPC's
	InBeacon->SetNetDriverName(InConnection->Driver->NetDriverName);
	InBeacon->SetNetConnection(InConnection);
}


UWorld* NUTNet::CreateUnitTestWorld(bool bHookTick/*=true*/)
{
	UWorld* ReturnVal = NULL;

	// Unfortunately, this hack is needed, to avoid a crash when running as commandlet
	// NOTE: Sometimes, depending upon build settings, PRIVATE_GIsRunningCommandlet is a define instead of a global
#ifndef PRIVATE_GIsRunningCommandlet
	bool bIsCommandlet = PRIVATE_GIsRunningCommandlet;

	PRIVATE_GIsRunningCommandlet = false;
#endif

	ReturnVal = UWorld::CreateWorld(EWorldType::None, false);

#ifndef PRIVATE_GIsRunningCommandlet
	PRIVATE_GIsRunningCommandlet = bIsCommandlet;
#endif


	if (ReturnVal != NULL)
	{
		UnitTestWorlds.Add(ReturnVal);

		// Hook the new worlds 'tick' event, so that we can capture logging
		if (bHookTick)
		{
			FWorldTickHook* TickHook = new FWorldTickHook(ReturnVal);

			ActiveTickHooks.Add(TickHook);

			TickHook->Init();
		}


		// Hack-mark the world as having begun play (when it has not)
		ReturnVal->bBegunPlay = true;

		// Hack-mark the world as having initialized actors (to allow RPC hooks)
		ReturnVal->bActorsInitialized = true;

		// Enable pause, using the PlayerController of the primary world (unless we're in the editor)
		if (!GIsEditor)
		{
			AWorldSettings* CurSettings = ReturnVal->GetWorldSettings();

			if (CurSettings != NULL)
			{
				ULocalPlayer* PrimLocPlayer = GEngine->GetFirstGamePlayer(NUTUtil::GetPrimaryWorld());
				APlayerController* PrimPC = (PrimLocPlayer != NULL ? PrimLocPlayer->PlayerController : NULL);
				APlayerState* PrimState = (PrimPC != NULL ? PrimPC->PlayerState : NULL);

				if (PrimState != NULL)
				{
					CurSettings->Pauser = PrimState;
				}
			}
		}

		// Create a blank world context, to prevent crashes
		FWorldContext& CurContext = GEngine->CreateNewWorldContext(EWorldType::None);
		CurContext.SetCurrentWorld(ReturnVal);
	}

	return ReturnVal;
}

void NUTNet::MarkUnitTestWorldForCleanup(UWorld* CleanupWorld, bool bImmediate/*=false*/)
{
	UnitTestWorlds.Remove(CleanupWorld);
	PendingUnitWorldCleanup.Add(CleanupWorld);

	if (!bImmediate)
	{
		GEngine->DeferredCommands.AddUnique(TEXT("CleanupUnitTestWorlds"));
	}
	else
	{
		CleanupUnitTestWorlds();
	}
}

void NUTNet::CleanupUnitTestWorlds()
{
	for (auto CleanupIt=PendingUnitWorldCleanup.CreateIterator(); CleanupIt; ++CleanupIt)
	{
		UWorld* CurWorld = *CleanupIt;

		// Iterate all ActorComponents in this world, and unmark them as having begun play - to prevent a crash during GC
		for (TActorIterator<AActor> ActorIt(CurWorld); ActorIt; ++ActorIt)
		{
			for (UActorComponent* CurComp : ActorIt->GetComponents())
			{
				if (CurComp->HasBegunPlay())
				{
					// Big hack - call only the parent class UActorComponent::EndPlay function, such that only bHasBegunPlay is unset
					bool bBeginDestroyed = CurComp->HasAnyFlags(RF_BeginDestroyed);

					CurComp->SetFlags(RF_BeginDestroyed);

					CurComp->UActorComponent::EndPlay(EEndPlayReason::Quit);

					if (!bBeginDestroyed)
					{
						CurComp->ClearFlags(RF_BeginDestroyed);
					}
				}
			}
		}

		// Remove the tick-hook, for this world
		int32 TickHookIdx = ActiveTickHooks.IndexOfByPredicate(
			[&CurWorld](const FWorldTickHook* CurTickHook)
			{
				return CurTickHook != NULL && CurTickHook->AttachedWorld == CurWorld;
			});

		if (TickHookIdx != INDEX_NONE)
		{
			ActiveTickHooks.RemoveAt(TickHookIdx);
		}

		GEngine->DestroyWorldContext(CurWorld);
		CurWorld->DestroyWorld(false);
	}

	PendingUnitWorldCleanup.Empty();


	// Immediately garbage collect remaining objects, to finish net driver cleanup
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
}

bool NUTNet::IsUnitTestWorld(UWorld* InWorld)
{
	return UnitTestWorlds.Contains(InWorld);
}

bool NUTNet::IsSteamNetDriverAvailable()
{
	bool bReturnVal = false;
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);

	if (GameEngine != nullptr)
	{
		bool bFoundSteamDriver = false;
		const TCHAR* SteamDriverClassName = TEXT("OnlineSubsystemSteam.SteamNetDriver");

		for (int i=0; i<GameEngine->NetDriverDefinitions.Num(); i++)
		{
			if (GameEngine->NetDriverDefinitions[i].DefName == NAME_GameNetDriver)
			{
				if (GameEngine->NetDriverDefinitions[i].DriverClassName == SteamDriverClassName)
				{
					bFoundSteamDriver = true;
				}

				break;
			}
		}

		if (bFoundSteamDriver)
		{
			UClass* SteamNetDriverClass = StaticLoadClass(UNetDriver::StaticClass(), nullptr, SteamDriverClassName, nullptr,
															LOAD_Quiet);

			if (SteamDriverClassName != nullptr && SteamNetDriverClass != nullptr)
			{
				UNetDriver* SteamNetDriverDef = Cast<UNetDriver>(SteamNetDriverClass->GetDefaultObject());

				bReturnVal = SteamNetDriverDef != nullptr && SteamNetDriverDef->IsAvailable();
			}
		}
	}

	return bReturnVal;
}
