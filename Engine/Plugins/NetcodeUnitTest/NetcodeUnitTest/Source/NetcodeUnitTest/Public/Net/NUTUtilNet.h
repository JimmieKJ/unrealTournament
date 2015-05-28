// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


// Forward declarations
class UUnitTestChannel;
class UUnitTestNetDriver;

/**
 * A delegate network notify class, to allow for easy inline-hooking.
 * 
 * NOTE: This will memory leak upon level change and re-hooking (if used as a hook),
 * because there is no consistent way to handle deleting it.
 */
class FNetworkNotifyHook : public FNetworkNotify
{
public:
	/**
	 * Base constructor
	 */
	FNetworkNotifyHook()
		: FNetworkNotify()
		, NotifyHandleClientPlayerDelegate()
		, NotifyAcceptingConnectionDelegate()
		, NotifyAcceptedConnectionDelegate()
		, NotifyAcceptingChannelDelegate()
		, NotifyControlMessageDelegate()
		, HookedNotify(NULL)
	{
	}

	/**
	 * Constructor which hooks an existing network notify
	 */
	FNetworkNotifyHook(FNetworkNotify* InHookNotify)
		: FNetworkNotify()
		, HookedNotify(InHookNotify)
	{
	}

	/**
	 * Virtual destructor
	 */
	virtual ~FNetworkNotifyHook()
	{
	}


	/**
	 * New notifications specifically added for unit testing
	 */

	/**
	 * Catches opening of the PlayerController actor channel (good place for immediate triggering of PC RPC's)
	 */
	void NotifyHandleClientPlayer(APlayerController* PC, UNetConnection* Connection);


protected:
	/**
	 * Old/original notifications
	 */

	virtual EAcceptConnection::Type NotifyAcceptingConnection() override;

	virtual void NotifyAcceptedConnection(UNetConnection* Connection) override;

	virtual bool NotifyAcceptingChannel(UChannel* Channel) override;

	virtual void NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, FInBunch& Bunch) override;


	DECLARE_DELEGATE_TwoParams(FNotifyHandleClientPlayerDelegate, APlayerController* /*PC*/, UNetConnection* /*Connection*/);

	DECLARE_DELEGATE_RetVal(EAcceptConnection::Type, FNotifyAcceptingConnectionDelegate);
	DECLARE_DELEGATE_OneParam(FNotifyAcceptedConnectionDelegate, UNetConnection* /*Connection*/);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FNotifyAcceptingChannelDelegate, UChannel* /*Channel*/);
	DECLARE_DELEGATE_RetVal_ThreeParams(bool, FNotifyControlMessageDelegate, UNetConnection* /*Connection*/, uint8 /*MessageType*/,
										FInBunch& /*Bunch*/);

public:
	FNotifyHandleClientPlayerDelegate	NotifyHandleClientPlayerDelegate;

	FNotifyAcceptingConnectionDelegate	NotifyAcceptingConnectionDelegate;
	FNotifyAcceptedConnectionDelegate	NotifyAcceptedConnectionDelegate;
	FNotifyAcceptingChannelDelegate		NotifyAcceptingChannelDelegate;
	FNotifyControlMessageDelegate		NotifyControlMessageDelegate;

	/** If this is hooking an existing network notify, save a reference */
	FNetworkNotify* HookedNotify;
};


/**
 * Class for hooking world tick events, and setting globals for log hooking
 */
class FWorldTickHook
{
public:
	FWorldTickHook(UWorld* InWorld)
		: AttachedWorld(InWorld)
	{
	}

	void Init()
	{
		if (AttachedWorld != NULL)
		{
#ifdef DELEGATE_DEPRECATED
			TickDispatchDelegateHandle  = AttachedWorld->OnTickDispatch().AddRaw(this, &FWorldTickHook::TickDispatch);
			PostTickFlushDelegateHandle = AttachedWorld->OnPostTickFlush().AddRaw(this, &FWorldTickHook::PostTickFlush);
#else
			AttachedWorld->OnTickDispatch().AddRaw(this, &FWorldTickHook::TickDispatch);
			AttachedWorld->OnPostTickFlush().AddRaw(this, &FWorldTickHook::PostTickFlush);
#endif
		}
	}

	void Cleanup()
	{
		if (AttachedWorld != NULL)
		{
#ifdef DELEGATE_DEPRECATED
			AttachedWorld->OnPostTickFlush().Remove(PostTickFlushDelegateHandle);
			AttachedWorld->OnTickDispatch().Remove(TickDispatchDelegateHandle);
#else
			AttachedWorld->OnPostTickFlush().RemoveRaw(this, &FWorldTickHook::PostTickFlush);
			AttachedWorld->OnTickDispatch().RemoveRaw(this, &FWorldTickHook::TickDispatch);
#endif
		}

		AttachedWorld = NULL;
	}

private:
	void TickDispatch(float DeltaTime)
	{
		GActiveLogWorld = AttachedWorld;
	}

	void PostTickFlush()
	{
		GActiveLogWorld = NULL;
	}

public:
	/** The world this is attached to */
	UWorld* AttachedWorld;

#ifdef DELEGATE_DEPRECATED
private:
	/** Handle for Tick dispatch delegate */
	FDelegateHandle TickDispatchDelegateHandle;

	/** Handle for PostTick dispatch delegate */
	FDelegateHandle PostTickFlushDelegateHandle;
#endif
};

/**
 * Netcode based utility functions
 */
struct NUTNet
{
	/**
	 * Gets the index of the first free/unused net channel, on the specified net connection
	 *
	 * @param InConnection	The net connection to get the index from
	 * @param InChIndex		Optionally, specifies the channel index at which to begin searching
	 * @return				Returns the first free channel index
	 */
	static int32 GetFreeChannelIndex(UNetConnection* InConnection, int32 InChIndex=INDEX_NONE);

	/**
	 * Creates a unit test channel, for capturing incoming data on a particular channel index,
	 * and routing it to a delegate for hooking or other purposes
	 *
	 * @param InConnection	The connection to create the channel in
	 * @param InType		The type of channel to create
	 * @param InChIndex		Optionally, specify the channel index
	 * @param bInVerifyOpen	Optionally, make the channel verify that it has been opened (keep sending initial packet, until ack)
	 * @return				Returns the created unit test channel
	 */
	static UUnitTestChannel* CreateUnitTestChannel(UNetConnection* InConnection, EChannelType InType, int32 InChIndex=INDEX_NONE,
													bool bInVerifyOpen=false);

	/**
	 * Outputs a bunch for the specified channel, with the ability to create the channel as well
	 * NOTE: BunchSequence should start at 0 for new channels, and should be tracked per-channel
	 *
	 * @param BunchSequence		Specify a variable for keeping track of the bunch sequence on this channel (may need to initialize as 0)
	 * @param InConnection		Specify the connection the bunch will be sent on
	 * @param InChType			Specify the type of the channel the bunch will be sent on
	 * @param InChIndex			Optionally, specify the index of the channel to send the bunch on
	 * @param bGetNextFreeChan	Whether or not to pick the next free/unused channel, for this bunch
	 */
	static NETCODEUNITTEST_API FOutBunch* CreateChannelBunch(int32& BunchSequence, UNetConnection* InConnection, EChannelType InChType,
												int32 InChIndex=INDEX_NONE, bool bGetNextFreeChan=false);

	/**
	 * Sends a control channel bunch over a UUnitTestChannel based control channel
	 *
	 * @param InConnection		The connection to send the bunch over
	 * @param ControlChanBunch	The bunch to send over the connections control channel
	 */
	static NETCODEUNITTEST_API void SendControlBunch(UNetConnection* InConnection, FOutBunch& ControlChanBunch);


	/**
	 * Creates an instance of the unit test net driver (can be used to create multiple such drivers, for creating multiple connections)
	 *
	 * @param InWorld	The world that the net driver should be associated with
	 * @return			Returns the created net driver instance
	 */
	static UUnitTestNetDriver* CreateUnitTestNetDriver(UWorld* InWorld);


	/**
	 * Creates a fake player, connecting to the specified IP
	 * NOTE: If the passed in net driver is NULL, the value is automatically initialized
	 * NOTE: Should be accompanied, by a call to DisconnectFakePlayer
	 *
	 * @param InWorld			The world that the fake player should be associated with
	 * @param InNetDriver		The net driver to create the fake player on (if NULL, is automatically initialized)
	 * @param ServerIP			The address the fake player should connect to
	 * @param InNotify			Optionally, specifies an FNetworkNotify for hooking/handling low-level netcode events
	 * @param bSkipJoin			Optionally, skip NMT_Join, which triggers PlayerController creation (or NMT_BeaconJoin, for beacons)
	 * @param InNetID			Optionally, specifies the unique net id the player should send
	 * @param bBeaconConnect	Specifies whether or not you're connecting to a beacon, rather than a normal connection
	 * @param InBeaconType		If connecting to a beacon, specifies the type name of the beacon
	 * @return					Whether or not the fake player was successfully created
	 */
	static bool CreateFakePlayer(UWorld* InWorld, UNetDriver*& InNetDriver, FString ServerIP, FNetworkNotify* InNotify=NULL,
									bool bSkipJoin=false, FUniqueNetIdRepl* InNetID=NULL, bool bBeaconConnect=false,
									FString InBeaconType=TEXT(""));

	/**
	 * Disconnects the specified fake player - including destructing the net driver and such (tears down the whole connection)
	 * NOTE: Based upon the HandleDisconnect function, except removing parts that are undesired (e.g. which trigger level switch)
	 *
	 * @param PlayerWorld		The world the fake player inhabits
	 * @param InNetDriver		The associated net driver, which will be torn down
	 */
	static void DisconnectFakePlayer(UWorld* PlayerWorld, UNetDriver* IntNetDriver);

	/**
	 * Handles setting up the client beacon once it is replicated, so that it can properly send RPC's
	 * (normally the serverside client beacon links up with the pre-existing beacon on the clientside,
	 * but with unit tests there is no pre-existing clientside beacon)
	 *
	 * @param InBeacon		The beacon that should be setup
	 * @param InConnection	The connection associated with the beacon
	 */
	static void HandleBeaconReplicate(class AOnlineBeaconClient* InBeacon, UNetConnection* InConnection);


	/**
	 * Creates a barebones/minimal UWorld, for setting up minimal fake player connections,
	 * and as a container for objects in the unit test commandlet
	 *
	 * @return	Returns the created UWorld object
	 */
	static UWorld* CreateUnitTestWorld(bool bHookTick=true);

	/**
	 * Marks the specified unit test world for cleanup
	 *
	 * @param CleanupWorld	The unit test world to be marked for cleanup
	 * @param bImmediate	If true, all unit test worlds pending cleanup, are immediately cleaned up
	 */
	static void MarkUnitTestWorldForCleanup(UWorld* CleanupWorld, bool bImmediate=false);

	/**
	 * Cleans up unit test worlds queued for cleanup
	 */
	static void CleanupUnitTestWorlds();

	/**
	 * Returns true, if the specified world is a unit test world
	 */
	static bool IsUnitTestWorld(UWorld* InWorld);

	/**
	 * Returns true, if the Steam net driver (and thus Steam support) is enabled/available
	 */
	static bool IsSteamNetDriverAvailable();
};

