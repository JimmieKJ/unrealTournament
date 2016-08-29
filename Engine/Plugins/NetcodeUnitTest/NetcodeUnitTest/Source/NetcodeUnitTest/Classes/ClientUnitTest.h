// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ProcessUnitTest.h"

#include "ClientUnitTest.generated.h"


// Forward declarations
class FNetworkNotifyHook;
class ANUTActor;


// Enum definitions

/**
 * Flags for configuring how individual unit tests make use of the base client unit test framework.
 * NOTE: These are flags instead of bools, partly to group these settings together, as being essential for configuring each unit test
 *
 * Types of things these flags control:
 *	- Types of remote data which are accepted/denied (channel types, actors, RPC's)
 *		- IMPORTANT: This includes local setup of e.g. actor channels, and possibly execution of RPC's in local context,
 *			which risks undefined behaviour (which is why it is disabled by default - you have to know what you're doing)
 *	- The required prerequisites needed before executing the main unit-test/payload (need a valid PlayerController? A particular actor?)
 *	- Whether or not a server is automatically launched, and if so, whether or not to capture its log output
 *	- Enabling other miscellaneous events, such as capturing raw packet data
 *
 * WARNING: This enum is not compatible with UENUM, as it is uint32, and UENUM requires uint8
 */
enum class EUnitTestFlags : uint32 // NOTE: If you change from uint32, you need to modify the 32-bit-dependent handling in some code
{
	None					= 0x00000000,	// No flags

	/** Sub-process flags */
	LaunchServer			= 0x00000001,	// Whether or not to automatically launch a game server, for the unit test
	LaunchClient			= 0x00000002,	// Whether or not to automatically launch a full game client, which connects to the server

	/** Minimal-client netcode functionality */
	AcceptActors			= 0x00000004,	// Whether or not to accept actor channels (acts as whitelist-only with NotifyAllowNetActor)
	AcceptPlayerController	= 0x00000008,	// Whether or not to accept PlayerController creation
	AcceptRPCs				= 0x00000010,	// Whether or not to accept execution of any actor RPC's (they are all blocked by default)
	SendRPCs				= 0x00000020,	// Whether or not to allow RPC sending (NOT blocked by default, @todo JohnB: Add send hook)
	SkipControlJoin			= 0x00000040,	// Whether or not to skip sending NMT_Join upon connect (or NMT_BeaconJoin for beacons)
	BeaconConnect			= 0x00000080,	// Whether or not to connect to the servers beacon (greatly limits the connection)
	AutoReconnect			= 0x00000100,	// Whether or not to auto-reconnect on server disconnect (NOTE: Won't catch all disconnects)

	/** Unit test state-setup/requirements/prerequisites */
	RequirePlayerController	= 0x00000200,	// Whether or not to wait for the PlayerController, before triggering ExecuteClientUnitTest
	RequirePawn				= 0x00000400,	// Whether or not to wait for the PlayerController's pawn, before ExecuteClientUnitTest
	RequirePing				= 0x00000800,	// Whether or not to wait for a ping round-trip, before triggering ExecuteClientUnitTest
	RequireNUTActor			= 0x00001000,	// Whether or not to wait for the NUTActor, before triggering ExecuteClientUnitTest
	RequireBeacon			= 0x00002000,	// Whether or not to wait for beacon replication, before triggering ExecuteClientUnitTest
	RequireCustom			= 0x00004000,	// Whether or not ExecuteClientUnitTest will be executed manually, within the unit test

	RequirementsMask		= RequirePlayerController | RequirePawn | RequirePing | RequireNUTActor | RequireBeacon | RequireCustom,

	/** Unit test error/crash detection */
	ExpectServerCrash		= 0x00010000,	// Whether or not this unit test will intentionally crash the server

	/** Unit test error/crash detection debugging (NOTE: Don't use these in finalized unit tests, unit tests must handle all errors) */
	IgnoreServerCrash		= 0x00020000,	// Whether or not server crashes should be treated as a unit test failure
	IgnoreClientCrash		= 0x00040000,	// Whether or not client crashes should be treated as a unit test failure
	IgnoreDisconnect		= 0x00080000,	// Whether or not minimal/fake client disconnects, should be treated as a unit test failure

	/** Unit test events */
	NotifyNetActors			= 0x00200000,	// Whether or not to trigger a 'NotifyNetActor' event, AFTER creation of actor channel actor
	NotifyProcessEvent		= 0x00400000,	// Whether or not to trigger 'NotifyScriptProcessEvent' for every executed local function

	/** Debugging */
	CaptureReceivedRaw		= 0x01000000,	// Whether or not to capture raw (clientside) packet receives
	CaptureSendRaw			= 0x02000000,	// Whether or not to capture raw (clientside) packet sends
	DumpReceivedRaw			= 0x04000000,	// Whether or not to also hex-dump the raw packet receives to the log/log-window
	DumpSendRaw				= 0x08000000,	// Whether or not to also hex-dump the raw packet sends to the log/log-window
	DumpControlMessages		= 0x10000000,	// Whether or not to dump control channel messages, and their raw hex content
	DumpReceivedRPC			= 0x20000000,	// Whether or not to dump RPC receives (with LogNetTraffic, detects ProcessEvent RPC fail)
	DumpSendRPC				= 0x40000000	// Whether or not to dump RPC sends
};

// Required for bitwise operations with the above enum
ENUM_CLASS_FLAGS(EUnitTestFlags);

/**
 * Used to get name values for the above enum
 */
inline FString GetUnitTestFlagName(EUnitTestFlags Flag)
{
	#define EUTF_CASE(x) case EUnitTestFlags::x : return TEXT(#x)

	switch (Flag)
	{
		EUTF_CASE(None);
		EUTF_CASE(LaunchServer);
		EUTF_CASE(LaunchClient);
		EUTF_CASE(AcceptActors);
		EUTF_CASE(AcceptPlayerController);
		EUTF_CASE(AcceptRPCs);
		EUTF_CASE(SendRPCs);
		EUTF_CASE(SkipControlJoin);
		EUTF_CASE(BeaconConnect);
		EUTF_CASE(RequirePlayerController);
		EUTF_CASE(RequirePawn);
		EUTF_CASE(RequirePing);
		EUTF_CASE(RequireNUTActor);
		EUTF_CASE(RequireBeacon);
		EUTF_CASE(RequireCustom);
		EUTF_CASE(ExpectServerCrash);
		EUTF_CASE(IgnoreServerCrash);
		EUTF_CASE(IgnoreClientCrash);
		EUTF_CASE(IgnoreDisconnect);
		EUTF_CASE(NotifyNetActors);
		EUTF_CASE(NotifyProcessEvent);
		EUTF_CASE(CaptureReceivedRaw);
		EUTF_CASE(CaptureSendRaw);
		EUTF_CASE(DumpReceivedRaw);
		EUTF_CASE(DumpSendRaw);
		EUTF_CASE(DumpControlMessages);
		EUTF_CASE(DumpReceivedRPC);
		EUTF_CASE(DumpSendRPC);

	default:
		if (FMath::IsPowerOfTwo((uint32)Flag))
		{
			return FString::Printf(TEXT("Unknown 0x%08X"), (uint32)Flag);
		}
		else
		{
			return FString::Printf(TEXT("Bad/Multiple flags 0x%08X"), (uint32) Flag);
		}
	}

	#undef EUTF_CASE
}


/**
 * Base class for all unit tests depending upon a (fake/minimal) client connecting to a server.
 * Handles creation/cleanup of an entire new UWorld, UNetDriver and UNetConnection, for fast sequential unit testing.
 * 
 * In subclasses, implement the unit test within the ExecuteClientUnitTest function (remembering to call parent)
 */
UCLASS()
class NETCODEUNITTEST_API UClientUnitTest : public UProcessUnitTest
{
	GENERATED_UCLASS_BODY()


	friend class FScopedLog;
	friend class UUnitTestManager;
	friend class FUnitTestEnvironment;


	/** Variables which should be specified by every subclass (some depending upon flags) */
protected:
	/** All of the internal unit test parameters/flags, for controlling state and execution */
	EUnitTestFlags UnitTestFlags;


	/** The base URL the server should start with */
	FString BaseServerURL;

	/** The (non-URL) commandline parameters the server should be launched with */
	FString BaseServerParameters;

	/** If connecting to a beacon, the beacon type name we are connecting to */
	FString ServerBeaconType;

	/** The base URL clients should start with */
	FString BaseClientURL;

	/** The (non-URL) commandline parameters clients should be launched with */
	FString BaseClientParameters;

	/** Actors the server is allowed replicate to client (requires AllowActors flag). Use NotifyAllowNetActor for conditional allows. */
	TArray<UClass*> AllowedClientActors;

	/** Clientside RPC's that should be allowed to execute (requires NotifyProcessEvent flag; other flags also allow specific RPC's) */
	TArray<FString> AllowedClientRPCs;


	/** Runtime variables */
protected:
	/** Reference to the created server process handling struct */
	TWeakPtr<FUnitTestProcess> ServerHandle;

	/** The address of the launched server */
	FString ServerAddress;

	/** The address of the server beacon (if UnitTestFlags are set to connect to a beacon) */
	FString BeaconAddress;

	/** Reference to the created client process handling struct (if enabled) */
	TWeakPtr<FUnitTestProcess> ClientHandle;

	/** Whether or not there is a blocking event/process preventing setup of the server */
	bool bBlockingServerDelay;

	/** Whether or not there is a blocking event/process preventing setup of a client */
	bool bBlockingClientDelay;

	/** Whether or not there is a blocking event/process preventing the fake client from connecting */
	bool bBlockingFakeClientDelay;

	/** When a server is launched after a blocking event/process, this delays the launch of any clients, in case of more blockages */
	double NextBlockingTimeout;


	/** Stores a reference to the created fake world, for execution and later cleanup */
	UWorld* UnitWorld;

	/** Stores a reference to the created network notify, for later cleanup */
	FNetworkNotifyHook* UnitNotify;

	/** Stores a reference to the created unit test net driver, for execution and later cleanup (always a UUnitTestNetDriver) */
	UNetDriver* UnitNetDriver;

	/** Stores a reference to the server connection (always a 'UUnitTestNetConnection') */
	UNetConnection* UnitConn;

	/** Whether or not the initial connect of the fake client was triggered */
	bool bTriggerredInitialConnect;

	/** Stores a reference to the replicated PlayerController (if set to wait for this), after NotifyHandleClientPlayer */
	TWeakObjectPtr<APlayerController> UnitPC;

	/** Whether or not the UnitPC Pawn was fully setup (requires EUnitTestFlags::RequirePawn) */
	bool bUnitPawnSetup;

	/** If EUnitTestFlags::RequireNUTActor is set, stores a reference to the replicated NUTActor */
	TWeakObjectPtr<ANUTActor> UnitNUTActor;

	/** If EUnitTestFlags::RequireBeacon is set, stores a reference to the replicated beacon */
	TWeakObjectPtr<class AOnlineBeaconClient> UnitBeacon;

	/** If EUnitTestFlags::RequirePing is true, whether or not we have already received the pong */
	bool bReceivedPong;

	/** For the unit test control channel, this tracks the current bunch sequence */
	int32 ControlBunchSequence;

	/** If notifying of net actor creation, this keeps track of new actor channel indexes pending notification */
	TArray<int32> PendingNetActorChans;


#if TARGET_UE4_CL >= CL_DEPRECATEDEL
private:
	/** Handle to the registered InternalNotifyNetworkFailure delegate */
	FDelegateHandle InternalNotifyNetworkFailureDelegateHandle;
#endif


	/**
	 * Interface and hooked events for client unit tests
	 */
public:
	/**
	 * Override this, to implement the client unit test
	 * NOTE: Should be called last, in overridden functions
	 * IMPORTANT: EndUnitTest should be triggered, upon completion of the unit test (which may be delayed, for many unit tests)
	 */
	virtual void ExecuteClientUnitTest() PURE_VIRTUAL(UClientUnitTest::ExecuteClientUnitTest,);


	/**
	 * Override this, to receive notification of NMT_NUTControl messages, from the server
	 *
	 * @param CmdType		The ENUTControlCommand type
	 * @param Command		The command being received
	 */
	virtual void NotifyNUTControl(uint8 CmdType, FString Command)
	{
	}

	/**
	 * Override this, to receive notification of all other non-NMT_NUTControl control messages
	 *
	 * @param Bunch			The bunch containing the control message
	 * @param MessageType	The control message type
	 */
	virtual void NotifyControlMessage(FInBunch& Bunch, uint8 MessageType);


	/**
	 * Notification, that a new channel is being created and is pending accept/deny
	 *
	 * @param Channel		The new channel being created
	 * @return				Whether or not to accept the new channel
	 */
	virtual bool NotifyAcceptingChannel(UChannel* Channel);

	/**
	 * Notification that the local net connections PlayerController has been replicated and is being setup
	 *
	 * @param PC			The new local PlayerController
	 * @param Connection	The connection associated with the PlayerController
	 */
	virtual void NotifyHandleClientPlayer(APlayerController* PC, UNetConnection* Connection);

	/**
	 * Override this, to receive notification BEFORE a replicated actor has been created (allowing you to block, based on class)
	 *
	 * @param ActorClass	The actor class that is about to be created
	 * @param bActorChannel	Whether or not this actor is being created within an actor channel
	 * @return				Whether or not to allow creation of that actor
	 */
	virtual bool NotifyAllowNetActor(UClass* ActorClass, bool bActorChannel);

	/**
	 * Override this, to receive notification AFTER an actor channel actor has been created
	 *
	 * @param ActorChannel	The actor channel associated with the actor
	 * @param Actor			The actor that has just been created
	 */
	virtual void NotifyNetActor(UActorChannel* ActorChannel, AActor* Actor);

	/**
	 * Triggered upon a network connection failure
	 *
	 * @param FailureType	The reason for the net failure
	 * @param ErrorString	More detailed error information
	 */
	virtual void NotifyNetworkFailure(ENetworkFailure::Type FailureType, const FString& ErrorString);


	/**
	 * If EUnitTestFlags::CaptureReceiveRaw is set, this is triggered for every packet received from the server
	 * NOTE: Data is a uint8 array, of size 'NETWORK_MAX_PACKET', and elements can safely be modified
	 *
	 * @param Data		The raw data/packet being received (this data can safely be modified, up to length 'NETWORK_MAX_PACKET')
	 * @param Count		The amount of data received (if 'Data' is modified, this should be modified to reflect the new length)
	 */
	virtual void NotifyReceivedRawPacket(void* Data, int32& Count);

	/**
	 * If EUnitTestFlags::CaptureSendRaw is set, this is triggered for every packet sent to the server
	 * NOTE: Don't consider data safe to modify (will need to modify the implementation, if that is desired)
	 *
	 * @param Data			The raw data/packet being sent
	 * @param Count			The amount of data being sent
	 * @param bBlockSend	Whether or not to block the send (defaults to false)
	 */
	virtual void NotifySendRawPacket(void* Data, int32 Count, bool& bBlockSend);

	/**
	 * If EUnitTestFlags::CaptureSendRaw is set, this is triggered for every packet sent to the server
	 * IMPORTANT: This is like the above function, except this occurs AFTER PacketHandler's have had a chance to modify packet data
	 *
	 * NOTE: Don't consider data safe to modify (will need to modify the implementation, if that is desired)
	 *
	 * @param Data		The raw data/packet being sent
	 * @param Count		The amount of data being sent
	 * @param bBlockSend	Whether or not to block the send (defaults to false)
	 */
	virtual void NotifySocketSendRawPacket(void* Data, int32 Count, bool& bBlockSend)
	{
	}

	/**
	 * Bunches received on the control channel. These need to be parsed manually,
	 * because the control channel is intentionally disrupted (otherwise it can affect the main client state)
	 *
	 * @param Bunch			The bunch being received
	 */
	virtual void ReceivedControlBunch(FInBunch& Bunch);


#if !UE_BUILD_SHIPPING
	/**
	 * Overridable in subclasses - can be used to control/block ALL script events
	 *
	 * @param Actor			The actor the event is being executed on
	 * @param Function		The script function being executed
	 * @param Parameters	The raw unparsed parameters, being passed into the function
	 * @return				Whether or not to block the event from executing
	 */
	virtual bool NotifyScriptProcessEvent(AActor* Actor, UFunction* Function, void* Parameters);
#endif

	/**
	 * Overridable in subclasses - can be used to control/block sending of RPC's
	 *
	 * @param Actor			The actor the RPC will be called in
	 * @param Function		The RPC to call
	 * @param Parameters	The parameters data blob
	 * @param OutParms		Out parameter information (irrelevant for RPC's)
	 * @param Stack			The script stack
	 * @param SubObject		The sub-object the RPC is being called in (if applicable)
	 * @return				Whether or not to allow sending of the RPC
	 */
	virtual bool NotifySendRPC(AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack,
								UObject* SubObject);


	virtual void NotifyProcessLog(TWeakPtr<FUnitTestProcess> InProcess, const TArray<FString>& InLogLines) override;

	virtual void NotifyProcessFinished(TWeakPtr<FUnitTestProcess> InProcess) override;

	virtual void NotifyProcessSuspendState(TWeakPtr<FUnitTestProcess> InProcess, ESuspendState InSuspendState) override;


	virtual void NotifySuspendRequest() override;

	virtual bool NotifyConsoleCommandRequest(FString CommandContext, FString Command) override;

	virtual void GetCommandContextList(TArray<TSharedPtr<FString>>& OutList, FString& OutDefaultContext) override;


	/**
	 * Utility functions for use by subclasses
	 */
public:
	/**
	 * Sends the specified RPC for the specified actor, and verifies that the RPC was sent (triggering a unit test failure if not)
	 *
	 * @param Target				The actor which will send the RPC
	 * @param FunctionName			The name of the RPC
	 * @param Parms					The RPC parameters (same as would be specified to ProcessEvent)
	 * @param ParmsSize				The size of the RPC parameters, for verifying binary compatibility
	 * @param ParmsSizeCorrection	Some parameters are compressed to a different size. Verify Parms matches, and use this to correct.
	 * @return						Whether or not the RPC was sent successfully
	 */
	bool SendRPCChecked(AActor* Target, const TCHAR* FunctionName, void* Parms, int16 ParmsSize, int16 ParmsSizeCorrection=0);

	/**
	 * As above, except the RPC is called within a lambda
	 * NOTE: Only call a single RPC within the lambda
	 *
	 * @param RPCName	The name of the RPC, for logging purposes
	 * @param RPCCall	The lambda which executes the RPC
	 * @return			Whether or not the RPC was sent successfully
	 */
	template<typename Predicate>
	FORCEINLINE bool SendRPCChecked(FString RPCName, Predicate RPCCall)
	{
		bool bSuccess = false;

		PreSendRPC();
		RPCCall();
		bSuccess = PostSendRPC(RPCName);

		return bSuccess;
	}


	/**
	 * Internal function, for preparing for a checked RPC call
	 */
	void PreSendRPC();

	/**
	 * Internal function, for handling the aftermath of a checked RPC call
	 *
	 * @param RPCName	The name of the RPC, for logging purposes
	 * @param Target	The actor the RPC is being called on (optional - for internal logging/debugging)
	 * @return			Whether or not the RPC was sent successfully
	 */
	bool PostSendRPC(FString RPCName, AActor* Target=nullptr);

	/**
	 * Internal base implementation and utility functions for client unit tests
	 */
protected:
	virtual bool ValidateUnitTestSettings(bool bCDOCheck=false) override;

	virtual void ResetTimeout(FString ResetReason, bool bResetConnTimeout=false, uint32 MinDuration=0) override;

	/**
	 * Resets the net connection timeout
	 *
	 * @param Duration	The duration which the timeout reset should last
	 */
	void ResetConnTimeout(float Duration);


	/**
	 * Returns the requirements flags, that this unit test currently meets
	 */
	EUnitTestFlags GetMetRequirements();

	/**
	 * Whether or not all 'requirements' flag conditions have been met
	 *
	 * @param bIgnoreCustom		If true, checks all requirements other than custom requirements
	 * @return					Whether or not all requirements are met
	 */
	bool HasAllRequirements(bool bIgnoreCustom=false);

public:
	/**
	 * Optionally, if the 'RequireCustom' flag is set, this returns whether custom conditions have been met.
	 *
	 * Only use this if you are mixing multiple 'requirements' flags - if just using 'RequireCustom',
	 * trigger 'ExecuteClientUnitTest' manually
	 *
	 * NOTE: You still have to check 'HasAllRequirements' and trigger 'ExecuteClientUnitTest', at the time the custom conditions are set
	 */
	virtual bool HasAllCustomRequirements()
	{
		return false;
	}

	/**
	 * Returns the type of log entries that this unit expects to output, for setting up log window filters
	 * (only needs to return values which affect what tabs are shown)
	 *
	 * @return		The log type mask, representing the type of log entries this unit test expects to output
	 */
	virtual ELogType GetExpectedLogTypes() override;

protected:
	virtual bool ExecuteUnitTest() override;

	virtual void CleanupUnitTest() override;


	/**
	 * Connects a fake client, to the launched/launching server
	 *
	 * @param InNetID		The unique net id the player should use
	 * @return				Whether or not the connection kicked off successfully
	 */
	virtual bool ConnectFakeClient(FUniqueNetIdRepl* InNetID=nullptr);

	/**
	 * Cleans up the local/fake client
	 */
	virtual void CleanupFakeClient();

	/**
	 * Triggers an auto-reconnect (disconnect/reconnect) of the fake client
	 */
	void TriggerAutoReconnect();


	/**
	 * Starts the server process for a particular unit test
	 */
	virtual void StartUnitTestServer();

	/**
	 * Puts together the commandline parameters the server should use, based upon the unit test settings
	 *
	 * @return			The final server commandline
	 */
	virtual FString ConstructServerParameters();

	/**
	 * Starts a client process tied to the unit test, and connects to the specified server address
	 *
	 * @param ConnectIP		The IP address the client should connect to
	 * @param bMinimized	Starts the client with the window minimized (some unit tests need this off)
	 * @return				Returns a pointer to the client process handling struct
	 */
	virtual TWeakPtr<FUnitTestProcess> StartUnitTestClient(FString ConnectIP, bool bMinimized=true);

	/**
	 * Puts together the commandline parameters clients should use, based upon the unit test settings
	 *
	 * @param ConnectIP	The IP address the client will be connecting to
	 * @return			The final client commandline
	 */
	virtual FString ConstructClientParameters(FString ConnectIP);


	virtual void PrintUnitTestProcessErrors(TSharedPtr<FUnitTestProcess> InHandle) override;

	void InternalNotifyNetworkFailure(UWorld* InWorld, UNetDriver* InNetDriver, ENetworkFailure::Type FailureType,
										const FString& ErrorString);


#if !UE_BUILD_SHIPPING
	static bool InternalScriptProcessEvent(AActor* Actor, UFunction* Function, void* Parameters, void* HookOrigin);
#endif


	virtual void UnitTick(float DeltaTime) override;

	virtual bool IsTickable() const override;
};
