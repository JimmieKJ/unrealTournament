// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnitTest.h"

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

	/** Unit test state-setup/requirements/prerequisites */
	RequirePlayerController	= 0x00000100,	// Whether or not to wait for the PlayerController, before triggering ExecuteClientUnitTest
	RequirePawn				= 0x00000200,	// Whether or not to wait for the PlayerController's pawn, before ExecuteClientUnitTest
	RequirePing				= 0x00000400,	// Whether or not to wait for a ping round-trip, before triggering ExecuteClientUnitTest
	RequireNUTActor			= 0x00000800,	// Whether or not to wait for the NUTActor, before triggering ExecuteClientUnitTest
	RequireBeacon			= 0x00001000,	// Whether or not to wait for beacon replication, before triggering ExecuteClientUnitTest
	RequireCustom			= 0x00002000,	// Whether or not ExecuteClientUnitTest will be executed manually, within the unit test

	RequirementsMask		= RequirePlayerController | RequirePawn | RequirePing | RequireNUTActor | RequireBeacon | RequireCustom,

	/** Unit test error/crash detection */
	ExpectServerCrash		= 0x00010000,	// Whether or not this unit test will intentionally crash the server

	/** Unit test error/crash detection debugging (NOTE: Don't use these in finalized unit tests, unit tests must handle all errors) */
	IgnoreServerCrash		= 0x00020000,	// Whether or not server crashes should be treated as a unit test failure
	IgnoreClientCrash		= 0x00040000,	// Whether or not client crashes should be treated as a unit test failure
	IgnoreDisconnect		= 0x00080000,	// Whether or not minimal/fake client disconnects, should be treated as a unit test failure

	/** Unit test events */
	NotifyAllowNetActor		= 0x00100000,	// Whether or not to trigger 'NotifyAllowNetActor', which whitelist actor channels by class
	NotifyNetActors			= 0x00200000,	// Whether or not to trigger a 'NotifyNetActor' event, AFTER creation of actor channel actor

	/** Debugging */
	CaptureReceivedRaw		= 0x01000000,	// Whether or not to capture raw (clientside) packet receives
	CaptureSendRaw			= 0x02000000,	// Whether or not to capture raw (clientside) packet sends
	DumpReceivedRaw			= 0x04000000,	// Whether or not to also hex-dump the raw packet receives to the log/log-window
	DumpSendRaw				= 0x08000000,	// Whether or not to also hex-dump the raw packet sends to the log/log-window
	DumpControlMessages		= 0x10000000,	// Whether or not to dump control channel messages, and their raw hex content
	DumpReceivedRPC			= 0x20000000,	// Whether or not to dump RPC receives
	DumpSendRPC				= 0x40000000,	// Whether or not to dump RPC sends
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
		EUTF_CASE(NotifyAllowNetActor);
		EUTF_CASE(NotifyNetActors);
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
 * Enum for different stages of error log parsing
 */
enum class EErrorLogStage : uint8
{
	ELS_NoError,		// No error logs have been received/parsed yet
	ELS_ErrorStart,		// The text indicating the start of an error log is being parsed
	ELS_ErrorDesc,		// The text describing the error is being parsed
	ELS_ErrorCallstack,	// The callstack for the error is being parsed
	ELS_ErrorExit		// The post-error exit message is being parsed (error parsing is effectively complete)
};

/**
 * Enum for specifying the suspend state of a process (typically the server)
 */
enum class ESuspendState : uint8
{
	Active,				// Process is currently active/not-suspended
	Suspended,			// Process is currently suspended
};


// Delegate definitions

/**
 * Delegate notifying that the server suspend state has changed
 *
 * @param NewSuspendState	The new server suspend state
 */
DECLARE_DELEGATE_OneParam(FOnSuspendStateChange, ESuspendState /*NewSuspendState*/);


// Struct definitions

/**
 * Struct used for storing and classifying each log error line
 */
struct FErrorLog
{
	/** The stage of this error log line */
	EErrorLogStage Stage;

	/** The error log line */
	FString Line;


	/**
	 * Base constructor
	 */
	FErrorLog()
		: Stage(EErrorLogStage::ELS_NoError)
		, Line()
	{
	}

	FErrorLog(EErrorLogStage InStage, const FString& InLine)
		: Stage(InStage)
		, Line(InLine)
	{
	}
};

/**
 * Struct used for handling a launched UE4 client/server process
 */
struct FUnitTestProcess
{
	friend class UClientUnitTest;

	/** Process handle for the launched process */
	FProcHandle ProcessHandle;

	/** The process ID */
	uint32 ProcessID;

	/** The suspend state of the process (implemented as a part of unit test code, does not relate to OS API) */
	ESuspendState SuspendState;

	/** Human-readable tag given to this process */
	FString ProcessTag;


	/** Handle to StdOut for the launched process */
	void* ReadPipe;

	/** Handle to StdIn for the launched process (unused) */
	void* WritePipe;


	/** The base log type for this process (client? server? process?) */
	ELogType BaseLogType;

	/** The prefix to use for StdOut log output */
	FString LogPrefix;

	/** The OutputDeviceColor string, to use for setting the log output color */
	const TCHAR* MainLogColor;

	/** The log output color to use in the slate log window */
	FSlateColor SlateLogColor;


	/** If this process is outputting an error log, this is the current stage of error parsing (or ELS_NoError if not parsing) */
	EErrorLogStage ErrorLogStage;

	/** Gathered error log text */
	TArray<FErrorLog> ErrorText;


	/**
	 * Base constructor
	 */
	FUnitTestProcess()
		: ProcessHandle()
		, ProcessID(0)
		, SuspendState(ESuspendState::Active)
		, ProcessTag(TEXT(""))
		, ReadPipe(NULL)
		, WritePipe(NULL)
		, BaseLogType(ELogType::None)
		, LogPrefix(TEXT(""))
		, MainLogColor(COLOR_NONE)
		, SlateLogColor(FSlateColor::UseForeground())
		, ErrorLogStage(EErrorLogStage::ELS_NoError)
		, ErrorText()
	{
	}
};


/**
 * Base class for all unit tests depending upon a (fake/minimal) client connecting to a server.
 * Handles creation/cleanup of an entire new UWorld, UNetDriver and UNetConnection, for fast sequential unit testing.
 * 
 * In subclasses, implement the unit test within the ExecuteClientUnitTest function (remembering to call parent)
 */
UCLASS()
class NETCODEUNITTEST_API UClientUnitTest : public UUnitTest
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


	/** Runtime variables */
protected:
	/** Stores a reference to all running child processes tied to this unit test, for housekeeping */
	TArray<TSharedPtr<FUnitTestProcess>> ActiveProcesses;

	/** Reference to the created server process handling struct */
	TWeakPtr<FUnitTestProcess> ServerHandle;

	/** The address of the launched server */
	FString ServerAddress;

	/** The address of the server beacon (if UnitTestFlags are set to connect to a beacon) */
	FString BeaconAddress;

	/** Reference to the created client process handling struct (if enabled) */
	TWeakPtr<FUnitTestProcess> ClientHandle;


	/** Stores a reference to the created fake world, for execution and later cleanup */
	UWorld* UnitWorld;

	/** Stores a reference to the created network notify, for later cleanup */
	FNetworkNotifyHook* UnitNotify;

	/** Stores a reference to the created unit test net driver, for execution and later cleanup (always a UUnitTestNetDriver) */
	UNetDriver* UnitNetDriver;

	/** Stores a reference to the server connection (always a 'UUnitTestNetConnection') */
	UNetConnection* UnitConn;

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


public:
	/** Delegate for notifying the UI, of a change in the server suspend state */
	FOnSuspendStateChange OnServerSuspendState;


#ifdef DELEGATE_DEPRECATED
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
	 * Override this, to receive notification BEFORE an actor channel actor has been created (allowing you to block, based on class)
	 *
	 * @param ActorClass	The actor class that is about to be created
	 * @return				Whether or not to allow creation of that actor
	 */
	virtual bool NotifyAllowNetActor(UClass* ActorClass);

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
	 * @param Data		The raw data/packet being sent
	 * @param Count		The amount of data being sent
	 */
	virtual void NotifySendRawPacket(void* Data, int32 Count);

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
	 * @param HookOrigin	Reference to the unit test that the event is associated with
	 * @return				Whether or not to block the event from executing
	 */
	virtual bool NotifyScriptProcessEvent(AActor* Actor, UFunction* Function, void* Parameters, void* HookOrigin);
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

	/**
	 * For implementation in subclasses, for helping to verify success/fail upon completion of unit tests
	 * NOTE: Not called again once VerificationState is set
	 * WARNING: Be careful when iterating InLogLines in multiple different for loops, if the sequence of detected logs is important
	 *
	 * @param InProcess		The process the log lines are from
	 * @param InLogLines	The current log lines being received
	 */
	virtual void NotifyProcessLog(TWeakPtr<FUnitTestProcess> InProcess, const TArray<FString>& InLogLines);

	/**
	 * Notifies that there was a request to suspend/resume the unit test server
	 */
	void NotifySuspendRequest();

	/**
	 * Notifies when the suspend state of a process changes
	 *
	 * @param InProcess			The process whose suspend state has changed
	 * @param InSuspendState	The new suspend state of the process
	 */
	void NotifyProcessSuspendState(TWeakPtr<FUnitTestProcess> InProcess, ESuspendState InSuspendState);


	/**
	 * Notifies that there was a request to enable/disable developer mode
	 *
	 * @param bInDeveloperMode	Whether or not developer mode is being enabled/disabled
	 */
	void NotifyDeveloperModeRequest(bool bInDeveloperMode);

	/**
	 * Notifies that there was a request to execute a console command for the unit test, which can occur in a specific context,
	 * e.g. for the unit test server, for the local minimal-client (within the unit test), or for the separate unit test client process
	 *
	 * @param CommandContext	The context (local/server/client?) for the console command
	 * @param Command			The command to be executed
	 * @return					Whether or not the command was handled
	 */
	virtual bool NotifyConsoleCommandRequest(FString CommandContext, FString Command);


	/**
	 * Outputs the list of console command contexts, that this unit test supports (which can include custom contexts in subclasses)
	 *
	 * @param OutList				Outputs the list of supported console command contexts
	 * @param OutDefaultContext		Outputs the context which should be auto-selected/defaulted-to
	 */
	virtual void GetCommandContextList(TArray<TSharedPtr<FString>>& OutList, FString& OutDefaultContext);


	/**
	 * Utility functions for use by subclasses
	 */
public:
	/**
	 * Sends the specified RPC for the specified actor, and verifies that the RPC was sent (triggering a unit test failure if not)
	 *
	 * @param Target		The actor which will send the RPC
	 * @param FunctionName	The name of the RPC
	 * @param Parms			The RPC parameters (same as would be specified to ProcessEvent)
	 * @return				Whether or not the RPC was sent successfully
	 */
	bool SendRPCChecked(AActor* Target, const TCHAR* FunctionName, void* Parms);

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
	bool PostSendRPC(FString RPCName, AActor* Target=NULL);

	/**
	 * Internal base implementation and utility functions for client unit tests
	 */
protected:
	virtual bool ValidateUnitTestSettings(bool bCDOCheck=false) override;

	virtual void ResetTimeout(FString ResetReason, bool bResetConnTimeout=false, uint32 MinDuration=0) override;


	/**
	 * Returns the requirements flags, that this unit test currently meets
	 */
	EUnitTestFlags GetMetRequirements();

	/**
	 * Whether or not all 'requirements' flag conditions have been met
	 *
	 * @return	Whether or not all requirements are met
	 */
	bool HasAllRequirements();

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


	virtual bool ExecuteUnitTest() override;

	virtual void CleanupUnitTest() override;


	/**
	 * Connects a fake client, to the launched/launching server
	 *
	 * @param InNetID		The unique net id the player should use
	 * @return				Whether or not the connection kicked off successfully
	 */
	virtual bool ConnectFakeClient(FUniqueNetIdRepl* InNetID=NULL);

	/**
	 * Cleans up the local/fake client
	 */
	virtual void CleanupFakeClient();


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


	/**
	 * Starts a child UE4 process, tied to the unit test, with the specified commandline
	 *
	 * @param InCommandline		The commandline that the child process should use
	 * @param bMinimized		Starts the process with the window minimized
	 * @return					Returns a pointer to the new processes handling struct
	 */
	virtual TWeakPtr<FUnitTestProcess> StartUnitTestProcess(FString InCommandline, bool bMinimized=true);

	/**
	 * Shuts-down/cleans-up a child process tied to the unit test
	 *
	 * @param InHandle	The handling struct for the process
	 */
	virtual void ShutdownUnitTestProcess(TSharedPtr<FUnitTestProcess> InHandle);

	/**
	 * Processes the standard output (i.e. log output) for processes
	 */
	void PollProcessOutput();

	/**
	 * Updates (and if necessary, saves) the memory stats for processes
	 */
	void UpdateProcessStats();


	/**
	 * Checks incoming process logs, for any indication of a UE4 crash/error
	 *
	 * @param InProcess		The process the log output originates from
	 * @param InLines		The incoming log lines
	 */
	void CheckOutputForError(TSharedPtr<FUnitTestProcess> InProcess, const TArray<FString>& InLines);


	void InternalNotifyNetworkFailure(UWorld* InWorld, UNetDriver* InNetDriver, ENetworkFailure::Type FailureType,
										const FString& ErrorString);


#if !UE_BUILD_SHIPPING
	static bool InternalScriptProcessEvent(AActor* Actor, UFunction* Function, void* Parameters, void* HookOrigin);
#endif


	// Tick standard output of the server, if the process is launched
	virtual void UnitTick(float DeltaTime) override;

	virtual void PostUnitTick(float DeltaTime) override;

	virtual bool IsTickable() const override;


	virtual void FinishDestroy() override;

	virtual void ShutdownAfterError() override;
};
