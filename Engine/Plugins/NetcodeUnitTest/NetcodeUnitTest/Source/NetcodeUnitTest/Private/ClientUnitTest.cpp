// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NetcodeUnitTestPCH.h"

#include "ClientUnitTest.h"
#include "UnitTestManager.h"
#include "NUTActor.h"

#include "Net/UnitTestNetDriver.h"
#include "Net/UnitTestNetConnection.h"
#include "Net/UnitTestChannel.h"

#include "NUTUtil.h"
#include "UnitTestEnvironment.h"
#include "Net/NUTUtilNet.h"
#include "NUTUtilDebug.h"

#include "LogWindowManager.h"
#include "SLogWindow.h"
#include "SLogWidget.h"


#include "Engine/ActorChannel.h"
#include "Regex.h"


/**
 * UClientUnitTest
 */

UClientUnitTest::UClientUnitTest(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, UnitTestFlags(EUnitTestFlags::None)
	, BaseServerURL(TEXT(""))
	, BaseServerParameters(TEXT(""))
	, BaseClientURL(TEXT(""))
	, BaseClientParameters(TEXT(""))
	, ActiveProcesses()
	, ServerHandle(NULL)
	, ServerAddress(TEXT(""))
	, BeaconAddress(TEXT(""))
	, ClientHandle(NULL)
	, UnitWorld(NULL)
	, UnitNotify(NULL)
	, UnitNetDriver(NULL)
	, UnitConn(NULL)
	, UnitPC(NULL)
	, bUnitPawnSetup(false)
	, UnitNUTActor(NULL)
	, UnitBeacon(NULL)
	, bReceivedPong(false)
	, ControlBunchSequence(0)
	, PendingNetActorChans()
	, OnServerSuspendState()
{
}


void UClientUnitTest::NotifyControlMessage(FInBunch& Bunch, uint8 MessageType)
{
	if (!!(UnitTestFlags & EUnitTestFlags::DumpControlMessages))
	{
		UNIT_LOG(ELogType::StatusDebug, TEXT("NotifyControlMessage: MessageType: %i (%s), Data Length: %i (%i), Raw Data:"), MessageType,
					(FNetControlMessageInfo::IsRegistered(MessageType) ? FNetControlMessageInfo::GetName(MessageType) :
					TEXT("UNKNOWN")), Bunch.GetBytesLeft(), Bunch.GetBitsLeft());

		if (!Bunch.IsError() && Bunch.GetBitsLeft() > 0)
		{
			UNIT_LOG_BEGIN(this, ELogType::StatusDebug | ELogType::StyleMonospace);
			NUTDebug::LogHexDump(Bunch.GetDataPosChecked(), Bunch.GetBytesLeft(), true, true);
			UNIT_LOG_END();
		}
	}
}

bool UClientUnitTest::NotifyAcceptingChannel(UChannel* Channel)
{
	bool bAccepted = false;

	if (Channel->ChType == CHTYPE_Actor)
	{
		bAccepted = !!(UnitTestFlags & EUnitTestFlags::AcceptActors);

		if (!!(UnitTestFlags & EUnitTestFlags::NotifyNetActors))
		{
			PendingNetActorChans.Add(Channel->ChIndex);
		}
	}

	return bAccepted;
}

void UClientUnitTest::NotifyHandleClientPlayer(APlayerController* PC, UNetConnection* Connection)
{
	UnitPC = PC;

	UnitEnv->HandleClientPlayer(UnitTestFlags, PC);

	ResetTimeout(TEXT("NotifyHandleClientPlayer"));

	if (!!(UnitTestFlags & EUnitTestFlags::RequirePlayerController) && HasAllRequirements())
	{
		ResetTimeout(TEXT("ExecuteClientUnitTest (NotifyHandleClientPlayer)"));
		ExecuteClientUnitTest();
	}
}

bool UClientUnitTest::NotifyAllowNetActor(UClass* ActorClass)
{
	bool bAllow = false;

	if (!!(UnitTestFlags & EUnitTestFlags::RequireNUTActor) && ActorClass == ANUTActor::StaticClass() && UnitNUTActor == NULL)
	{
		bAllow = true;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::AcceptPlayerController) && ActorClass->IsChildOf(APlayerController::StaticClass()) &&
		UnitPC == NULL)
	{
		bAllow = true;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequirePawn) && ActorClass->IsChildOf(ACharacter::StaticClass()) &&
		(!UnitPC.IsValid() || UnitPC->GetCharacter() == NULL))
	{
		bAllow = true;
	}

	return bAllow;
}

void UClientUnitTest::NotifyNetActor(UActorChannel* ActorChannel, AActor* Actor)
{
	if (!UnitNUTActor.IsValid())
	{
		UnitNUTActor = Cast<ANUTActor>(Actor);

		ResetTimeout(TEXT("NotifyNetActor - UnitNUTActor"));

		if (UnitNUTActor.IsValid() && !!(UnitTestFlags & EUnitTestFlags::RequireNUTActor) && HasAllRequirements())
		{
			ResetTimeout(TEXT("ExecuteClientUnitTest (NotifyNetActor - UnitNUTActor)"));
			ExecuteClientUnitTest();
		}
	}

	if (!!(UnitTestFlags & EUnitTestFlags::BeaconConnect) && !UnitBeacon.IsValid())
	{
		UnitBeacon = Cast<AOnlineBeaconClient>(Actor);

		if (UnitBeacon.IsValid())
		{
			NUTNet::HandleBeaconReplicate(UnitBeacon.Get(), UnitConn);

			if (!!(UnitTestFlags & EUnitTestFlags::RequireBeacon) && HasAllRequirements())
			{
				ResetTimeout(TEXT("ExecuteClientUnitTest (NotifyNetActor - UnitBeacon)"));
				ExecuteClientUnitTest();
			}
		}
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequirePawn) && !bUnitPawnSetup && UnitPC.IsValid() && Cast<ACharacter>(Actor) != NULL &&
		UnitPC->GetCharacter() != NULL)
	{
		bUnitPawnSetup = true;

		ResetTimeout(TEXT("NotifyNetActor - bUnitPawnSetup"));

		if (HasAllRequirements())
		{
			ResetTimeout(TEXT("ExecuteClientUnitTest (NotifyNetActor - bUnitPawnSetup)"));
			ExecuteClientUnitTest();
		}
	}
}

void UClientUnitTest::NotifyNetworkFailure(ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// Only process this error, if a result has not already been returned
	if (VerificationState == EUnitTestVerification::Unverified)
	{
		FString LogMsg = FString::Printf(TEXT("Got network failure of type '%s' (%s)"), ENetworkFailure::ToString(FailureType),
											*ErrorString);

		if (!(UnitTestFlags & EUnitTestFlags::IgnoreDisconnect))
		{
			LogMsg += TEXT(", marking unit test as needing update.");

			UNIT_LOG(ELogType::StatusFailure | ELogType::StyleBold, TEXT("%s"), *LogMsg);
			UNIT_STATUS_LOG(ELogType::StatusFailure | ELogType::StatusVerbose | ELogType::StyleBold, TEXT("%s"), *LogMsg);

			VerificationState = EUnitTestVerification::VerifiedNeedsUpdate;
		}
		else
		{
			LogMsg += TEXT(".");

			UNIT_LOG(ELogType::StatusWarning, TEXT("%s"), *LogMsg);
			UNIT_STATUS_LOG(ELogType::StatusWarning | ELogType::StatusVerbose, TEXT("%s"), *LogMsg);
		}
	}

	// Shut down the fake client now (relevant for developer mode)
	if (VerificationState != EUnitTestVerification::Unverified)
	{
		CleanupFakeClient();
	}
}

void UClientUnitTest::NotifyReceivedRawPacket(void* Data, int32& Count)
{
	if (!!(UnitTestFlags & EUnitTestFlags::DumpReceivedRaw))
	{
		UNIT_LOG(ELogType::StatusDebug, TEXT("NotifyReceivedRawPacket: Packet dump:"));

		UNIT_LOG_BEGIN(this, ELogType::StatusDebug | ELogType::StyleMonospace);
		NUTDebug::LogHexDump((const uint8*)Data, Count, true, true);
		UNIT_LOG_END();
	}
}

void UClientUnitTest::NotifySendRawPacket(void* Data, int32 Count)
{
	if (!!(UnitTestFlags & EUnitTestFlags::DumpSendRaw))
	{
		UNIT_LOG(ELogType::StatusDebug, TEXT("NotifySendRawPacket: Packet dump:"));

		UNIT_LOG_BEGIN(this, ELogType::StatusDebug | ELogType::StyleMonospace);
		NUTDebug::LogHexDump((const uint8*)Data, Count, true, true);
		UNIT_LOG_END();
	}
}

void UClientUnitTest::ReceivedControlBunch(FInBunch& Bunch)
{
	if (!Bunch.AtEnd())
	{
		uint8 MessageType = 0;
		Bunch << MessageType;

		if (!Bunch.IsError())
		{
			if (MessageType == NMT_NUTControl)
			{
				uint8 CmdType = 0;
				FString Command;
				FNetControlMessage<NMT_NUTControl>::Receive(Bunch, CmdType, Command);

				if (!!(UnitTestFlags & EUnitTestFlags::RequirePing) && !bReceivedPong && CmdType == ENUTControlCommand::Pong)
				{
					bReceivedPong = true;

					ResetTimeout(TEXT("ReceivedControlBunch - Ping"));

					if (HasAllRequirements())
					{
						ResetTimeout(TEXT("ExecuteClientUnitTest (ReceivedControlBunch - Ping)"));
						ExecuteClientUnitTest();
					}
				}
				else
				{
					NotifyNUTControl(CmdType, Command);
				}
			}
			else
			{
				NotifyControlMessage(Bunch, MessageType);
			}
		}
	}
}

#if !UE_BUILD_SHIPPING
bool UClientUnitTest::NotifyScriptProcessEvent(AActor* Actor, UFunction* Function, void* Parameters, void* HookOrigin)
{
	bool bBlockEvent = false;

	// Handle UnitTestFlags that require RPC monitoring
	if (!(UnitTestFlags & EUnitTestFlags::AcceptRPCs) || !!(UnitTestFlags & EUnitTestFlags::DumpReceivedRPC) ||
		!!(UnitTestFlags & EUnitTestFlags::RequirePawn))
	{
		bool bNetClientRPC = !!(Function->FunctionFlags & FUNC_Net) && !!(Function->FunctionFlags & FUNC_NetClient);

		if (bNetClientRPC)
		{
			// Whether or not to force acceptance of this RPC
			bool bForceAccept = false;

			// Handle detection and proper setup of the PlayerController's pawn
			if (!!(UnitTestFlags & EUnitTestFlags::RequirePawn) && !bUnitPawnSetup && UnitPC != NULL)
			{
				FString FuncName = Function->GetName();

				if (FuncName == TEXT("ClientRestart"))
				{
					UNIT_LOG(ELogType::StatusImportant, TEXT("Got ClientRestart"));

					// Trigger the event directly here, and block execution in the original code, so that we can execute code post-ProcessEvent
					Actor->UObject::ProcessEvent(Function, Parameters);


					// If the pawn is set, now execute the exploit
					if (UnitPC->GetCharacter())
					{
						bUnitPawnSetup = true;

						ResetTimeout(TEXT("bUnitPawnSetup"));

						if (HasAllRequirements())
						{
							ResetTimeout(TEXT("ExecuteClientUnitTest (bUnitPawnSetup)"));
							ExecuteClientUnitTest();
						}
					}
					// If the pawn was not set, get the server to check again
					else
					{
						FString LogMsg = TEXT("Pawn was not set, sending ServerCheckClientPossession request");

						ResetTimeout(LogMsg);
						UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);

						UnitPC->ServerCheckClientPossession();
					}


					bBlockEvent = true;
				}
				// Retries setting the pawn, which will trigger ClientRestart locally, and enters into the above code with the Pawn set
				else if (FuncName == TEXT("ClientRetryClientRestart"))
				{
					bBlockEvent = false;
					bForceAccept = true;
				}
			}


			// Block RPC's, if they are not accepted
			if (!(UnitTestFlags & EUnitTestFlags::AcceptRPCs) && !bForceAccept)
			{
				UNIT_LOG(, TEXT("Blocking receive RPC '%s' for actor '%s'"), *Function->GetName(), *Actor->GetFullName());

				bBlockEvent = true;
			}

			if (!!(UnitTestFlags & EUnitTestFlags::DumpReceivedRPC) && !bBlockEvent)
			{
				UNIT_LOG(ELogType::StatusDebug, TEXT("Received RPC '%s' for actor '%s'"), *Function->GetName(), *Actor->GetFullName());
			}
		}
	}

	return bBlockEvent;
}
#endif

bool UClientUnitTest::NotifySendRPC(AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack,
									UObject* SubObject)
{
	bool bAllowRPC = true;

	if (!(UnitTestFlags & EUnitTestFlags::SendRPCs))
	{
		UNIT_LOG(, TEXT("Blocking send RPC '%s' in actor '%s' (SubObject '%s')"), *Function->GetName(), *Actor->GetFullName(),
					(SubObject != NULL ? *SubObject->GetFullName() : TEXT("NULL")));

		bAllowRPC = false;
	}
	else if (!!(UnitTestFlags & EUnitTestFlags::DumpSendRPC))
	{
		UNIT_LOG(ELogType::StatusDebug, TEXT("Send RPC '%s' for actor '%s' (SubObject '%s')"), *Function->GetName(),
					*Actor->GetFullName(), (SubObject != NULL ? *SubObject->GetFullName() : TEXT("NULL")));
	}

	return bAllowRPC;
}

void UClientUnitTest::NotifyProcessLog(TWeakPtr<FUnitTestProcess> InProcess, const TArray<FString>& InLogLines)
{
	// Get partial log messages that indicate startup progress/completion
	const TArray<FString>* ServerStartProgressLogs = NULL;
	const TArray<FString>* ServerReadyLogs = NULL;
	const TArray<FString>* ServerTimeoutResetLogs = NULL;

	UnitEnv->GetServerProgressLogs(ServerStartProgressLogs, ServerReadyLogs, ServerTimeoutResetLogs);


	// If launching a server, delay joining by the fake client, until the server has fully setup, and reset the unit test timeout,
	// each time there is a server log event, that indicates progress in starting up
	if (!!(UnitTestFlags & EUnitTestFlags::LaunchServer) && ServerHandle.IsValid() && InProcess.HasSameObject(ServerHandle.Pin().Get()))
	{
		// Using 'ContainsByPredicate' as an iterator
		FString MatchedLine;

		const auto SearchInLogLine =
			[&](const FString& ProgressLine)
			{
				bool bFound = false;

				for (auto CurLine : InLogLines)
				{
					if (CurLine.Contains(ProgressLine))
					{
						MatchedLine = CurLine;
						bFound = true;

						break;
					}
				}

				return bFound;
			};

		if (UnitConn == NULL || UnitConn->State == EConnectionState::USOCK_Pending)
		{
			if (ServerReadyLogs->ContainsByPredicate(SearchInLogLine))
			{
				// Fire off fake client connection
				if (UnitConn == NULL)
				{
					FString LogMsg = FString::Printf(TEXT("Detected successful server startup, launching fake client."));

					UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
					UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

					ConnectFakeClient();
				}

				ResetTimeout(FString(TEXT("ServerReady: ")) + MatchedLine);
			}
			else if (ServerStartProgressLogs->ContainsByPredicate(SearchInLogLine))
			{
				ResetTimeout(FString(TEXT("ServerStartProgress: ")) + MatchedLine);
			}
		}

		if (ServerTimeoutResetLogs->Num() > 0)
		{
			if (ServerTimeoutResetLogs->ContainsByPredicate(SearchInLogLine))
			{
				ResetTimeout(FString(TEXT("ServerTimeoutReset: ")) + MatchedLine, true);
			}
		}
	}

	// @todo JohnB: Consider adding a progress-checker for launched clients as well

	// @todo JohnB: Consider also, adding a way to communicate with launched clients,
	//				to reset their connection timeout upon server progress, if they fully startup before the server does
}

void UClientUnitTest::NotifySuspendRequest()
{
	TSharedPtr<FUnitTestProcess> CurProcess = (ServerHandle.IsValid() ? ServerHandle.Pin() : NULL);

	if (CurProcess.IsValid())
	{
		// Suspend request
		if (CurProcess->SuspendState == ESuspendState::Active)
		{
			if (UnitConn != NULL)
			{
				UUnitTestChannel* UnitControlChan = Cast<UUnitTestChannel>(UnitConn->Channels[0]);

				if (UnitControlChan != NULL)
				{
					FOutBunch* ControlChanBunch = NUTNet::CreateChannelBunch(ControlBunchSequence, UnitConn, CHTYPE_Control, 0);

					uint8 ControlMsg = NMT_NUTControl;
					uint8 PingCmd = ENUTControlCommand::SuspendProcess;
					FString Dud = TEXT("");

					*ControlChanBunch << ControlMsg;
					*ControlChanBunch << PingCmd;
					*ControlChanBunch << Dud;

					NUTNet::SendControlBunch(UnitConn, *ControlChanBunch);


					NotifyProcessSuspendState(ServerHandle, ESuspendState::Suspended);

					UNIT_LOG(, TEXT("Sent suspend request to server (may take time to execute, if server is still starting)."));
				}
				else
				{
					UNIT_LOG(, TEXT("No control channel, can't suspend (server still starting?)."))
				}
			}
			else
			{
				UNIT_LOG(, TEXT("Not connected to server, can't suspend."));
			}
		}
		// Resume request
		else if (CurProcess->SuspendState == ESuspendState::Suspended)
		{
			// Send the resume request over a named pipe - this is the only line of communication once suspended
			FString ResumePipeName = FString::Printf(TEXT("%s%i"), NUT_SUSPEND_PIPE, CurProcess->ProcessID);
			FPlatformNamedPipe ResumePipe;

			if (ResumePipe.Create(ResumePipeName, false, false))
			{
				if (ResumePipe.IsReadyForRW())
				{
					int32 ResumeVal = 1;
					ResumePipe.WriteInt32(ResumeVal);

					UNIT_LOG(, TEXT("Sent resume request to server."));

					NotifyProcessSuspendState(ServerHandle, ESuspendState::Active);
				}
				else
				{
					UNIT_LOG(, TEXT("WARNING: Resume pipe not ready for read/write (server still starting?)."));
				}

				ResumePipe.Destroy();
			}
			else
			{
				UNIT_LOG(, TEXT("Failed to create named pipe, for sending resume request (server still starting?)."));
			}
		}
	}
}

void UClientUnitTest::NotifyProcessSuspendState(TWeakPtr<FUnitTestProcess> InProcess, ESuspendState InSuspendState)
{
	InProcess.Pin()->SuspendState = InSuspendState;

	if (InProcess == ServerHandle)
	{
		OnServerSuspendState.ExecuteIfBound(InSuspendState);
	}

	// @todo JohnB: Want to support client process debugging in future?
}


void UClientUnitTest::NotifyDeveloperModeRequest(bool bInDeveloperMode)
{
	bDeveloperMode = bInDeveloperMode;
}


bool UClientUnitTest::NotifyConsoleCommandRequest(FString CommandContext, FString Command)
{
	bool bHandled = false;

	if (CommandContext == TEXT("Local") || CommandContext == TEXT("Global"))
	{
		UWorld* TargetWorld = (CommandContext == TEXT("Local") ? UnitWorld : NULL);

		// Don't execute commands that crash
		TArray<FString> BadCmds;
		BadCmds.Add(TEXT("exit"));

		if (!BadCmds.Contains(Command))
		{
			// @todo JohnB: Should this mark the log origin, as from the unit test?
			// @todo JohnB: In general, I'm not sure how I handle the log-origin of UI-triggered events;
			//				they maybe should be captured/categorized better
			UNIT_LOG_BEGIN(this, ELogType::OriginConsole);
			bHandled = GEngine->Exec(TargetWorld, *Command, *GLog);
			UNIT_LOG_END();
		}
		else
		{
			UNIT_LOG(, TEXT("Can't execute command '%s', it's in the 'bad commands' list (i.e. probably crashes)"), *Command);
		}
	}
	else if (CommandContext == TEXT("Server"))
	{
		// @todo JohnB: Perhaps add extra checks here, to be sure we're ready to send console commands?
		if (UnitConn != NULL)
		{
			FOutBunch* ControlChanBunch = NUTNet::CreateChannelBunch(ControlBunchSequence, UnitConn, CHTYPE_Control, 0);

			uint8 ControlMsg = NMT_NUTControl;
			uint8 ControlCmd = ENUTControlCommand::Command_NoResult;
			FString Cmd = Command;

			*ControlChanBunch << ControlMsg;
			*ControlChanBunch << ControlCmd;
			*ControlChanBunch << Cmd;

			NUTNet::SendControlBunch(UnitConn, *ControlChanBunch);


			UNIT_LOG(, TEXT("Sent command '%s' to server."), *Command);

			bHandled = true;
		}
		else
		{
			UNIT_LOG(, TEXT("Failed to send console command '%s', no server connection."), *Command);
		}
	}
	else if (CommandContext == TEXT("Client"))
	{
		// @todo JohnB

		UNIT_LOG(, TEXT("Client console commands not yet implemented"));
	}
	else
	{
		UNIT_LOG(ELogType::StatusFailure, TEXT("Unknown console command context '%s' - this is a bug that should be fixed."),
					*CommandContext);
	}

	return bHandled;
}

void UClientUnitTest::GetCommandContextList(TArray<TSharedPtr<FString>>& OutList, FString& OutDefaultContext)
{
	OutList.Add(MakeShareable(new FString(TEXT("Global"))));
	OutList.Add(MakeShareable(new FString(TEXT("Local"))));

	if (!!(UnitTestFlags & EUnitTestFlags::LaunchServer))
	{
		OutList.Add(MakeShareable(new FString(TEXT("Server"))));
	}

	if (!!(UnitTestFlags & EUnitTestFlags::LaunchClient))
	{
		OutList.Add(MakeShareable(new FString(TEXT("Client"))));
	}

	OutDefaultContext = TEXT("Local");
}


bool UClientUnitTest::SendRPCChecked(AActor* Target, const TCHAR* FunctionName, void* Parms)
{
	bool bSuccess = false;
	UFunction* TargetFunc = Target->FindFunction(FName(FunctionName));

	PreSendRPC();

	if (TargetFunc != NULL)
	{
		Target->ProcessEvent(TargetFunc, Parms);
	}

	bSuccess = PostSendRPC(FunctionName, Target);

	return bSuccess;
}

void UClientUnitTest::PreSendRPC()
{
	// Flush before and after, so no queued data is counted as a send, and so that the queued RPC is immediately sent and detected
	UnitConn->FlushNet();

	GSentBunch = false;
}

bool UClientUnitTest::PostSendRPC(FString RPCName, AActor* Target/*=NULL*/)
{
	bool bSuccess = false;

	UnitConn->FlushNet();

	// Just hack-erase bunch overflow tracking for this actors channel
	UChannel* TargetChan = UnitConn->ActorChannels.FindRef(Target);

	if (TargetChan != NULL)
	{
		TargetChan->NumOutRec = 0;
	}

	// If sending failed, trigger an overall unit test failure
	if (!GSentBunch)
	{
		FString LogMsg = FString::Printf(TEXT("Failed to send RPC '%s', unit test needs update."), *RPCName);

		// If specific/known failure cases are encountered, append them to the log message, to aid debugging
		if (Target != NULL && Target->GetNetConnection() == NULL)
		{
			LogMsg += TEXT(" (GetNetConnection() returned NULL)");
		}

		UNIT_LOG(ELogType::StatusFailure, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

		VerificationState = EUnitTestVerification::VerifiedNeedsUpdate;

		bSuccess = false;
	}
	else
	{
		bSuccess = true;
	}

	return bSuccess;
}


bool UClientUnitTest::ValidateUnitTestSettings(bool bCDOCheck/*=false*/)
{
	bool bSuccess = Super::ValidateUnitTestSettings();

	// Currently, unit tests don't support NOT launching/connecting to a server
	UNIT_ASSERT(!!(UnitTestFlags & EUnitTestFlags::LaunchServer));

	// If launching a server, make sure the base URL for the server is set
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::LaunchServer) || BaseServerURL.Len() > 0);

	// If launching a client, make sure some default client parameters have been set (to avoid e.g. launching fullscreen)
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::LaunchClient) || BaseClientParameters.Len() > 0);


	// If you require a player/NUTActor, you need to accept actor channels
	UNIT_ASSERT((!(UnitTestFlags & EUnitTestFlags::AcceptPlayerController) && !(UnitTestFlags & EUnitTestFlags::RequireNUTActor)) ||
					!!(UnitTestFlags & EUnitTestFlags::AcceptActors));

	// Don't require a PlayerController, if you don't accept one
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::RequirePlayerController) ||
					!!(UnitTestFlags & EUnitTestFlags::AcceptPlayerController));

	// If you require a pawn, you must require a PlayerController
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::RequirePawn) || !!(UnitTestFlags & EUnitTestFlags::RequirePlayerController));

#if UE_BUILD_SHIPPING
	// You can't detect pawns in shipping builds, as you need to hook ProcessEvent for RPC notifications
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::RequirePawn));
#endif

	// If you require a pawn, validate the existence of certain RPC's that are needed for pawn setup and verification
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::RequirePawn) || (
				GetDefault<APlayerController>()->FindFunction(FName(TEXT("ClientRestart"))) != NULL &&
				GetDefault<APlayerController>()->FindFunction(FName(TEXT("ClientRetryClientRestart"))) != NULL));

	// For part of pawn-setup detection, you need notification for net actors
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::RequirePawn) || !!(UnitTestFlags & EUnitTestFlags::NotifyNetActors));

	// If the ping requirements flag is set, it should be the ONLY one set
	// (which means only one bit should be set, and one bit means it should be power-of-two)
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::RequirePing) ||
					FMath::IsPowerOfTwo((uint32)(UnitTestFlags & EUnitTestFlags::RequirementsMask)));

	// As above, but with the 'custom' requirements flag
	// NOTE: Removed, as it can still be useful to have other requirements flags
	//UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::RequireCustom) ||
	//				FMath::IsPowerOfTwo((uint32)(UnitTestFlags & EUnitTestFlags::RequirementsMask)));

	// You can't accept RPC's, without accepting actors
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::AcceptRPCs) || !!(UnitTestFlags & EUnitTestFlags::AcceptActors));

	// You can't send RPC's, without accepting a player controller (netcode blocks this, without a PC); unless this is a beacon
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::SendRPCs) || !!(UnitTestFlags & EUnitTestFlags::AcceptPlayerController) ||
				!!(UnitTestFlags & EUnitTestFlags::BeaconConnect));

	// If connecting to a beacon, a number of unit test flags are not supported
	const EUnitTestFlags RejectedBeaconFlags = (EUnitTestFlags::AcceptPlayerController | EUnitTestFlags::RequirePlayerController |
												EUnitTestFlags::RequirePing);

	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::BeaconConnect) || !(UnitTestFlags & RejectedBeaconFlags));

	// If connecting to a beacon, net actor notification is required, for proper setup
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::BeaconConnect) || !!(UnitTestFlags & EUnitTestFlags::NotifyNetActors));

	// If connecting to a beacon, you must specify the beacon type
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::BeaconConnect) || ServerBeaconType.Len() > 0);

	// Don't require a beacon, if you're not connecting to a beacon
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::RequireBeacon) || !!(UnitTestFlags & EUnitTestFlags::BeaconConnect));

	// In shipping builds, you MUST accept RPC's, as there is no way to filter them out (can't hook ProcessEvent); soft-fail
#if UE_BUILD_SHIPPING
	if (!(UnitTestFlags & EUnitTestFlags::AcceptRPCs))
	{
		UNIT_LOG(ELogType::StatusFailure | ELogType::StyleBold, TEXT("Unit tests run in shipping mode, must accept RPC's"));

		bSuccess = false;
	}
#endif

	// Don't specify server-dependent flags, if not auto-launching a server
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::LaunchClient) || !!(UnitTestFlags & EUnitTestFlags::LaunchServer));

	// If DumpReceivedRaw is set, make sure hooking of ReceivedRawPacket is enabled too
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::DumpReceivedRaw) || !!(UnitTestFlags & EUnitTestFlags::CaptureReceivedRaw));

	// If DumpSendRaw is set, make sure hooking of SendRawPacket is enabled too
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::DumpSendRaw) || !!(UnitTestFlags & EUnitTestFlags::CaptureSendRaw));

	// You can't get net actor allow notifications, unless you accept actors
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::NotifyAllowNetActor) || !!(UnitTestFlags & EUnitTestFlags::AcceptActors));

	// You can't get net actor notifications, unless you accept actors
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::NotifyNetActors) || !!(UnitTestFlags & EUnitTestFlags::AcceptActors));

	// You can't use 'RequireNUTActor', without net actor notifications
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::RequireNUTActor) || !!(UnitTestFlags & EUnitTestFlags::NotifyNetActors));

	// Don't accept any 'Ignore' flags, once the unit test is finalized (they're debug only - all crashes must be handled in final code)
	UNIT_ASSERT(bWorkInProgress || !(UnitTestFlags & (EUnitTestFlags::IgnoreServerCrash | EUnitTestFlags::IgnoreClientCrash |
				EUnitTestFlags::IgnoreDisconnect)));


	return bSuccess;
}

EUnitTestFlags UClientUnitTest::GetMetRequirements()
{
	EUnitTestFlags ReturnVal = EUnitTestFlags::None;

	if (!!(UnitTestFlags & EUnitTestFlags::RequirePlayerController) && UnitPC != NULL)
	{
		ReturnVal |= EUnitTestFlags::RequirePlayerController;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequirePawn) && UnitPC != NULL && UnitPC->GetCharacter() != NULL)
	{
		ReturnVal |= EUnitTestFlags::RequirePawn;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequirePing) && bReceivedPong)
	{
		ReturnVal |= EUnitTestFlags::RequirePing;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequireNUTActor) && UnitNUTActor.IsValid())
	{
		ReturnVal |= EUnitTestFlags::RequireNUTActor;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequireBeacon) && UnitBeacon.IsValid())
	{
		ReturnVal |= EUnitTestFlags::RequireBeacon;
	}

	// ExecuteClientUnitTest should be triggered manually - unless you override HasAllCustomRequirements
	if (!!(UnitTestFlags & EUnitTestFlags::RequireCustom) && HasAllCustomRequirements())
	{
		ReturnVal |= EUnitTestFlags::RequireCustom;
	}

	return ReturnVal;
}

bool UClientUnitTest::HasAllRequirements()
{
	bool bReturnVal = true;

	// The fake client creation/connection is now delayed, so need to wait for that too
	if (UnitConn == NULL)
	{
		bReturnVal = false;
	}

	EUnitTestFlags RequiredFlags = (UnitTestFlags & EUnitTestFlags::RequirementsMask);

	if ((RequiredFlags & GetMetRequirements()) != RequiredFlags)
	{
		bReturnVal = false;
	}

	return bReturnVal;
}

ELogType UClientUnitTest::GetExpectedLogTypes()
{
	ELogType ReturnVal = Super::GetExpectedLogTypes();

	if (!!(UnitTestFlags & EUnitTestFlags::LaunchServer))
	{
		ReturnVal |= ELogType::Server;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::LaunchClient))
	{
		ReturnVal |= ELogType::Client;
	}

	if (!!(UnitTestFlags & (EUnitTestFlags::DumpReceivedRaw | EUnitTestFlags::DumpSendRaw | EUnitTestFlags::DumpControlMessages)))
	{
		ReturnVal |= ELogType::StatusDebug;
	}

	return ReturnVal;
}

void UClientUnitTest::ResetTimeout(FString ResetReason, bool bResetConnTimeout/*=false*/, uint32 MinDuration/*=0*/)
{
	// Extend the timeout to at least two minutes, if a crash is expected, as sometimes crash dumps take a very long time
	if (!!(UnitTestFlags & EUnitTestFlags::ExpectServerCrash) &&
			(ResetReason.Contains("ExecuteClientUnitTest") || ResetReason.Contains("Detected crash.")))
	{
		MinDuration = FMath::Max<uint32>(MinDuration, 120);
		bResetConnTimeout = true;
	}

	Super::ResetTimeout(ResetReason, bResetConnTimeout, MinDuration);

	if (bResetConnTimeout && UnitConn != NULL && UnitConn->State != USOCK_Closed && UnitConn->Driver != NULL)
	{
		// @todo JohnB: This is a slightly hacky way of setting the timeout to a large value, which will be overridden by newly
		//				received packets, making it unsuitable for most situations (except crashes - but that could still be subject
		//				to a race condition)
		UnitConn->LastReceiveTime = UnitConn->Driver->Time + (float)(TimeoutExpire - FPlatformTime::Seconds());
	}
}


bool UClientUnitTest::ExecuteUnitTest()
{
	bool bSuccess = false;

	bool bValidSettings = ValidateUnitTestSettings();

	// @todo JohnB: Fix support for Steam eventually
	bool bSteamAvailable = NUTNet::IsSteamNetDriverAvailable();

	if (bValidSettings && !bSteamAvailable)
	{
		if (!!(UnitTestFlags & EUnitTestFlags::LaunchServer))
		{
			StartUnitTestServer();

			if (!!(UnitTestFlags & EUnitTestFlags::LaunchClient))
			{
				// Client handle is set outside of StartUnitTestClient, in case support for multiple clients is added later
				ClientHandle = StartUnitTestClient(ServerAddress);
			}
		}

		// No longer immediately setup the fake client, wait for the server to fully startup first (monitoring its log output)
#if 0
		bSuccess = ConnectFakeClient();
#else
		bSuccess = true;
#endif
	}
	else if (!bValidSettings)
	{
		FString LogMsg = TEXT("Failed to validate unit test settings/environment");

		UNIT_LOG(ELogType::StatusFailure, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);
	}
	else if (bSteamAvailable)
	{
		FString LogMsg = TEXT("Unit tests should not be run when Steam is running");

		UNIT_LOG(ELogType::StatusFailure, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);
	}

	return bSuccess;
}

void UClientUnitTest::CleanupUnitTest()
{
	CleanupFakeClient();

	for (int32 i=ActiveProcesses.Num()-1; i>=0; i--)
	{
		if (ActiveProcesses[i].IsValid())
		{
			ShutdownUnitTestProcess(ActiveProcesses[i]);
		}
	}

#if !UE_BUILD_SHIPPING
	RemoveProcessEventCallback(this, &UClientUnitTest::InternalScriptProcessEvent);
#endif

	UUnitTest::CleanupUnitTest();
}

bool UClientUnitTest::ConnectFakeClient(FUniqueNetIdRepl* InNetID/*=NULL*/)
{
	bool bSuccess = false;

	if (UnitWorld == NULL)
	{
		// Make all of this happen in a blank, newly constructed world
		UnitWorld = NUTNet::CreateUnitTestWorld();

		if (UnitWorld != NULL)
		{
			UnitNotify = new FNetworkNotifyHook();

			UnitNotify->NotifyAcceptingChannelDelegate.BindUObject(this, &UClientUnitTest::NotifyAcceptingChannel);
			UnitNotify->NotifyHandleClientPlayerDelegate.BindUObject(this, &UClientUnitTest::NotifyHandleClientPlayer);

			bool bFailedScriptHook = false;

			if (!(UnitTestFlags & EUnitTestFlags::AcceptRPCs))
			{
#if !UE_BUILD_SHIPPING
				AddProcessEventCallback(this, &UClientUnitTest::InternalScriptProcessEvent);
				bFailedScriptHook = false;
#else
				bFailedScriptHook = true;
#endif
			}

			if (!bFailedScriptHook)
			{
				// Create a fake player/net-connection within the newly created world, and connect to the server
				bool bSkipControlJoin = !!(UnitTestFlags & EUnitTestFlags::SkipControlJoin);
				bool bBeaconConnect = !!(UnitTestFlags & EUnitTestFlags::BeaconConnect);

				FString ConnectAddress = (bBeaconConnect ? BeaconAddress : ServerAddress);

				if (NUTNet::CreateFakePlayer(UnitWorld, (UNetDriver*&)UnitNetDriver, ConnectAddress, UnitNotify, bSkipControlJoin,
												InNetID, bBeaconConnect, ServerBeaconType))
				{
					bSuccess = true;


					if (GEngine != NULL)
					{
#ifdef DELEGATE_DEPRECATED
						InternalNotifyNetworkFailureDelegateHandle = GEngine->OnNetworkFailure().AddUObject(this,
																		&UClientUnitTest::InternalNotifyNetworkFailure);
#else
						GEngine->OnNetworkFailure().AddUObject(this, &UClientUnitTest::InternalNotifyNetworkFailure);
#endif
					}


					if (!(UnitTestFlags & EUnitTestFlags::SendRPCs) || !!(UnitTestFlags & EUnitTestFlags::DumpSendRPC))
					{
						UUnitTestNetDriver* CurUnitDriver = CastChecked<UUnitTestNetDriver>(UnitNetDriver);

						CurUnitDriver->SendRPCDel.BindUObject(this, &UClientUnitTest::NotifySendRPC);
					}


					UnitConn = UnitNetDriver->ServerConnection;
					UUnitTestNetConnection* CurUnitConn = CastChecked<UUnitTestNetConnection>(UnitConn);
					UUnitTestChannel* UnitControlChan = CastChecked<UUnitTestChannel>(UnitConn->Channels[0]);

					UnitControlChan->ReceivedBunchDel.BindUObject(this, &UClientUnitTest::ReceivedControlBunch);

					if (!!(UnitTestFlags & EUnitTestFlags::CaptureReceivedRaw))
					{
						CurUnitConn->ReceivedRawPacketDel.BindUObject(this, &UClientUnitTest::NotifyReceivedRawPacket);
					}

					if (!!(UnitTestFlags & EUnitTestFlags::CaptureSendRaw))
					{
						CurUnitConn->LowLevelSendDel.BindUObject(this, &UClientUnitTest::NotifySendRawPacket);
					}

					if (!!(UnitTestFlags & EUnitTestFlags::NotifyAllowNetActor))
					{
						CurUnitConn->ActorChannelSpawnDel.BindUObject(this, &UClientUnitTest::NotifyAllowNetActor);
					}

					// If you don't have to wait for any requirements, execute immediately
					if (!(UnitTestFlags & (EUnitTestFlags::RequirementsMask)))
					{
						ResetTimeout(TEXT("ExecuteClientUnitTest (ExecuteUnitTest)"));
						ExecuteClientUnitTest();
					}


					if (!!(UnitTestFlags & EUnitTestFlags::RequirePing))
					{
						// Send the 'ping' bunch
						ControlBunchSequence = 1;
						FOutBunch* ControlChanBunch = NUTNet::CreateChannelBunch(ControlBunchSequence, UnitConn, CHTYPE_Control, 0);

						uint8 ControlMsg = NMT_NUTControl;
						uint8 PingCmd = ENUTControlCommand::Ping;
						FString Dud = TEXT("");

						*ControlChanBunch << ControlMsg;
						*ControlChanBunch << PingCmd;
						*ControlChanBunch << Dud;

						NUTNet::SendControlBunch(UnitConn, *ControlChanBunch);
					}

					FString LogMsg = FString::Printf(TEXT("Successfully created fake player connection to IP '%s'"), *ServerAddress);

					UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
					UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);
				}
				else
				{
					FString LogMsg = TEXT("Failed to create fake player connection");

					UNIT_LOG(ELogType::StatusFailure, TEXT("%s"), *LogMsg);
					UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);
				}
			}
			else
			{
				FString LogMsg = TEXT("Require ProcessEvent hook, but current build configuration does not support it.");

				UNIT_LOG(ELogType::StatusFailure, TEXT("%s"), *LogMsg);
				UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);
			}
		}
		else
		{
			FString LogMsg = TEXT("Failed to create unit test world");

			UNIT_LOG(ELogType::StatusFailure, TEXT("%s"), *LogMsg);
			UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);
		}
	}
	else
	{
		FString LogMsg = TEXT("Unit test world already exists, can't create fake client");

		UNIT_LOG(ELogType::StatusWarning, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);
	}

	return bSuccess;
}

// @todo JohnB: Reconsider naming of this function, if you modify the other functions using 'fake'
void UClientUnitTest::CleanupFakeClient()
{
	if (UnitWorld != NULL && UnitNetDriver != NULL)
	{
		// @todo JohnB: As with the 'CreateFakePlayer' function, naming it 'fake' may not be optimal
		NUTNet::DisconnectFakePlayer(UnitWorld, UnitNetDriver);

		UnitNetDriver->Notify = NULL;
	}

	UnitNetDriver = NULL;
	UnitPC = NULL;
	UnitConn = NULL;
	UnitNUTActor = NULL;

	if (UnitNotify != NULL)
	{
		delete UnitNotify;
		UnitNotify = NULL;
	}

	if (GEngine != NULL)
	{
#ifdef DELEGATE_DEPRECATED
		GEngine->OnNetworkFailure().Remove(InternalNotifyNetworkFailureDelegateHandle);
#else
		GEngine->OnNetworkFailure().RemoveUObject(this, &UClientUnitTest::InternalNotifyNetworkFailure);
#endif
	}

	// Immediately cleanup (or rather, start of next tick, as that's earliest possible time) after sending the RPC
	if (UnitWorld != NULL)
	{
		NUTNet::MarkUnitTestWorldForCleanup(UnitWorld);
		UnitWorld = NULL;
	}
}


void UClientUnitTest::StartUnitTestServer()
{
	if (!ServerHandle.IsValid())
	{
		FString LogMsg = TEXT("Unit test launching a server");

		UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

		// Determine the new server port
		int DefaultPort = 0;
		GConfig->GetInt(TEXT("URL"), TEXT("Port"), DefaultPort, GEngineIni);

		// Increment the server port used by 10, for every unit test
		static int ServerPortOffset = 0;
		int ServerPort = DefaultPort + 50 + (++ServerPortOffset * 10);
		int BeaconPort = ServerPort + 5;


		// Setup the launch URL
		FString ServerParameters = ConstructServerParameters() + FString::Printf(TEXT(" -Port=%i"), ServerPort);

		if (!!(UnitTestFlags & EUnitTestFlags::BeaconConnect))
		{
			ServerParameters += FString::Printf(TEXT(" -BeaconPort=%i"), BeaconPort);
		}

		ServerHandle = StartUnitTestProcess(ServerParameters);

		if (ServerHandle.IsValid())
		{
			ServerAddress = FString::Printf(TEXT("127.0.0.1:%i"), ServerPort);

			if (!!(UnitTestFlags & EUnitTestFlags::BeaconConnect))
			{
				BeaconAddress = FString::Printf(TEXT("127.0.0.1:%i"), BeaconPort);
			}

			auto CurHandle = ServerHandle.Pin();

			CurHandle->ProcessTag = TEXT("Server");
			CurHandle->BaseLogType = ELogType::Server;
			CurHandle->LogPrefix = TEXT("[SERVER]");
			CurHandle->MainLogColor = COLOR_CYAN;
			CurHandle->SlateLogColor = FLinearColor(0.f, 1.f, 1.f);
		}
	}
	else
	{
		UNIT_LOG(ELogType::StatusFailure, TEXT("ERROR: Server process already started."));
	}
}

FString UClientUnitTest::ConstructServerParameters()
{
	// NOTE: In the absence of "-ddc=noshared", a VPN connection can cause UE4 to take a long time to startup
	// NOTE: Without '-CrashForUAT'/'-unattended' the auto-reporter can pop up
	FString Parameters = FString(FApp::GetGameName()) + TEXT(" ") + BaseServerURL + TEXT(" -server ") + BaseServerParameters +
							TEXT(" -Log=UnitTestServer.log -forcelogflush -stdout -AllowStdOutLogVerbosity -ddc=noshared") +
							TEXT(" -unattended -CrashForUAT -NoShaderWorker");

	return Parameters;
}

TWeakPtr<FUnitTestProcess> UClientUnitTest::StartUnitTestClient(FString ConnectIP, bool bMinimized/*=true*/)
{
	TWeakPtr<FUnitTestProcess> ReturnVal = NULL;

	FString LogMsg = TEXT("Unit test launching a client");

	UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
	UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

	FString ClientParameters = ConstructClientParameters(ConnectIP);

	ReturnVal = StartUnitTestProcess(ClientParameters, bMinimized);

	if (ReturnVal.IsValid())
	{
		auto CurHandle = ReturnVal.Pin();

		// @todo JohnB: If you add support for multiple clients, make the log prefix numbered, also try to differentiate colours
		CurHandle->ProcessTag = TEXT("Client");
		CurHandle->BaseLogType = ELogType::Client;
		CurHandle->LogPrefix = TEXT("[CLIENT]");
		CurHandle->MainLogColor = COLOR_GREEN;
		CurHandle->SlateLogColor = FLinearColor(0.f, 1.f, 0.f);
	}

	return ReturnVal;
}

FString UClientUnitTest::ConstructClientParameters(FString ConnectIP)
{
	// NOTE: In the absence of "-ddc=noshared", a VPN connection can cause UE4 to take a long time to startup
	// NOTE: Without '-CrashForUAT'/'-unattended' the auto-reporter can pop up
	FString Parameters = FString(FApp::GetGameName()) + TEXT(" ") + ConnectIP + BaseClientURL + TEXT(" -game ") + BaseClientParameters +
							TEXT(" -Log=UnitTestClient.log -forcelogflush -stdout -AllowStdOutLogVerbosity -ddc=noshared -nosplash") +
							TEXT(" -unattended -CrashForUAT -NoShaderWorker -nosound");

	return Parameters;
}

TWeakPtr<FUnitTestProcess> UClientUnitTest::StartUnitTestProcess(FString InCommandline, bool bMinimized/*=true*/)
{
	TSharedPtr<FUnitTestProcess> ReturnVal = MakeShareable(new FUnitTestProcess());

	FString GamePath = FPlatformProcess::GenerateApplicationPath(FApp::GetName(), FApp::GetBuildConfiguration());

	verify(FPlatformProcess::CreatePipe(ReturnVal->ReadPipe, ReturnVal->WritePipe));

	UNIT_LOG(ELogType::StatusImportant, TEXT("Starting process with parameters: %s"), *InCommandline);

	ReturnVal->ProcessHandle = FPlatformProcess::CreateProc(*GamePath, *InCommandline, true, bMinimized, false, &ReturnVal->ProcessID,
															0, NULL, ReturnVal->WritePipe);

	if (ReturnVal->ProcessHandle.IsValid())
	{
		ReturnVal->ProcessTag = FString::Printf(TEXT("Process_%i"), ReturnVal->ProcessID);

		ActiveProcesses.Add(ReturnVal);
	}
	else
	{
		UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to start process"));
	}

	return ReturnVal;
}

void UClientUnitTest::ShutdownUnitTestProcess(TSharedPtr<FUnitTestProcess> InHandle)
{
	if (InHandle->ProcessHandle.IsValid())
	{
		FString LogMsg = FString::Printf(TEXT("Shutting down process '%s'."), *InHandle->ProcessTag);

		UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg)


		FPlatformProcess::TerminateProc(InHandle->ProcessHandle, true);

#if TARGET_UE4_CL < CL_CLOSEPROC
		InHandle->ProcessHandle.Close();
#else
		FPlatformProcess::CloseProc(InHandle->ProcessHandle);
#endif
	}

	FPlatformProcess::ClosePipe(InHandle->ReadPipe, InHandle->WritePipe);


	// Print out any detected error logs
	if (InHandle->ErrorLogStage != EErrorLogStage::ELS_NoError && InHandle->ErrorText.Num() > 0)
	{
		FString LogMsg = FString::Printf(TEXT("Detected a crash in process '%s':"),
											*InHandle->ProcessTag);

		UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusImportant, TEXT("%s (click 'Advanced Summary' for more detail)"), *LogMsg);


		// If this was the server, and we were not expecting a crash, print out a warning
		if (!(UnitTestFlags & EUnitTestFlags::ExpectServerCrash) && InHandle == ServerHandle.Pin())
		{
			LogMsg = TEXT("WARNING: Got server crash, but unit test not marked as expecting a server crash.");

			STATUS_SET_COLOR(FLinearColor(1.0, 1.0, 0.0));

			UNIT_LOG(ELogType::StatusWarning, TEXT("%s"), *LogMsg);
			UNIT_STATUS_LOG(ELogType::StatusWarning, TEXT("%s"), *LogMsg);

			STATUS_RESET_COLOR();
		}


		uint32 CallstackCount = 0;

		for (auto CurErrorLine : InHandle->ErrorText)
		{
			ELogType CurLogType = ELogType::None;

			if (CurErrorLine.Stage == EErrorLogStage::ELS_ErrorStart)
			{
				CurLogType = ELogType::StatusAdvanced;
			}
			else if (CurErrorLine.Stage == EErrorLogStage::ELS_ErrorDesc)
			{
				CurLogType = ELogType::StatusImportant | ELogType::StatusAdvanced | ELogType::StyleBold;
			}
			else if (CurErrorLine.Stage == EErrorLogStage::ELS_ErrorCallstack)
			{
				CurLogType = ELogType::StatusAdvanced;

				// Include the first five callstack lines in the main summary printout
				if (CallstackCount < 5)
				{
					CurLogType |= ELogType::StatusImportant;
				}

				CallstackCount++;
			}
			else if (CurErrorLine.Stage == EErrorLogStage::ELS_ErrorExit)
			{
				continue;
			}
			else
			{
				// Make sure there's a check for all error log stages
				UNIT_ASSERT(false);
			}

			// For the current unit-test log window, all entries are marked 'StatusImportant'
			UNIT_LOG(CurLogType | ELogType::StatusImportant, TEXT("%s"), *CurErrorLine.Line);

			UNIT_STATUS_LOG(CurLogType, TEXT("%s"), *CurErrorLine.Line);
		}
	}


	ActiveProcesses.Remove(InHandle);
}


void UClientUnitTest::UpdateProcessStats()
{
	SIZE_T TotalProcessMemoryUsage = 0;

	for (auto CurHandle : ActiveProcesses)
	{
		// Check unit test memory stats, and update if necessary (NOTE: Processes not guaranteed to still be running)
		if (CurHandle.IsValid() && CurHandle->ProcessID != 0)
		{
			SIZE_T ProcessMemUsage = 0;

			if (FPlatformProcess::GetApplicationMemoryUsage(CurHandle->ProcessID, &ProcessMemUsage) && ProcessMemUsage != 0)
			{
				TotalProcessMemoryUsage += ProcessMemUsage;
			}
		}
	}

	if (TotalProcessMemoryUsage > 0)
	{
		CurrentMemoryUsage = TotalProcessMemoryUsage;

		float RunningTime = (float)(FPlatformTime::Seconds() - StartTime);

		// Update saved memory usage stats, to help with tracking estimated memory usage, on future unit test launches
		if (CurrentMemoryUsage > PeakMemoryUsage)
		{
			if (PeakMemoryUsage == 0)
			{
				bFirstTimeStats = true;
			}

			PeakMemoryUsage = CurrentMemoryUsage;

			// Reset TimeToPeakMem
			TimeToPeakMem = RunningTime;

			SaveConfig();
		}
		// Check if we have hit a new record, for the time it takes to get within 90% of PeakMemoryUsage
		else if (RunningTime < TimeToPeakMem && (CurrentMemoryUsage * 100) >= (PeakMemoryUsage * 90))
		{
			TimeToPeakMem = RunningTime;
			SaveConfig();
		}
	}
}

void UClientUnitTest::CheckOutputForError(TSharedPtr<FUnitTestProcess> InProcess, const TArray<FString>& InLines)
{
	// Array of log messages that can indicate the start of an error
	static const TArray<FString> ErrorStartLogs = TArrayBuilder<FString>()
		.Add(FString(TEXT("Windows GetLastError: ")))
		.Add(FString(TEXT("=== Critical error: ===")));


	for (FString CurLine : InLines)
	{
		// Using 'ContainsByPredicate' as an iterator
		const auto CheckForErrorLog =
			[&CurLine](const FString& ErrorLine)
			{
				return CurLine.Contains(ErrorLine);
			};

		// Search for the beginning of an error log message
		if (InProcess->ErrorLogStage == EErrorLogStage::ELS_NoError)
		{
			if (ErrorStartLogs.ContainsByPredicate(CheckForErrorLog))
			{
				InProcess->ErrorLogStage = EErrorLogStage::ELS_ErrorStart;

				// Reset the timeout for both the unit test and unit test connection here, as callstack logs are prone to failure
				ResetTimeout(TEXT("Detected crash."), true);
			}
		}


		if (InProcess->ErrorLogStage != EErrorLogStage::ELS_NoError)
		{
			// Regex pattern for matching callstack logs - matches: " (0x000007fefe22cacd) + 0 bytes ["
			const FRegexPattern CallstackPattern(TEXT("\\s\\(0x[0-9,a-f]+\\) \\+ [0-9]+ bytes \\["));

			// Check for the beginning of description logs
			if (InProcess->ErrorLogStage == EErrorLogStage::ELS_ErrorStart &&
				!ErrorStartLogs.ContainsByPredicate(CheckForErrorLog))
			{
				InProcess->ErrorLogStage = EErrorLogStage::ELS_ErrorDesc;
			}

			// Check-for/verify callstack logs
			if (InProcess->ErrorLogStage == EErrorLogStage::ELS_ErrorDesc ||
				InProcess->ErrorLogStage == EErrorLogStage::ELS_ErrorCallstack)
			{
				FRegexMatcher CallstackMatcher(CallstackPattern, CurLine);

				if (CallstackMatcher.FindNext())
				{
					InProcess->ErrorLogStage = EErrorLogStage::ELS_ErrorCallstack;
				}
				else if (InProcess->ErrorLogStage == EErrorLogStage::ELS_ErrorCallstack)
				{
					InProcess->ErrorLogStage = EErrorLogStage::ELS_ErrorExit;
				}
			}

			// The rest of the lines, after callstack parsing, should be ELS_ErrorExit logs - these are not verified though (no need)


			InProcess->ErrorText.Add(FErrorLog(InProcess->ErrorLogStage, CurLine));
		}
	}
}


#if !UE_BUILD_SHIPPING
// @todo JohnB: 'HookOrigin' is not optimal - try to at least make it a UClientUnitTest pointer
bool UClientUnitTest::InternalScriptProcessEvent(AActor* Actor, UFunction* Function, void* Parameters, void* HookOrigin)
{
	bool bBlockEvent = false;

	if (HookOrigin != NULL)
	{
		// Match up the origin to an active unit test (for safety)
		UClientUnitTest* OriginUnitTest = NULL;

		if (GUnitTestManager != NULL)
		{
			for (int i=0; i<GUnitTestManager->ActiveUnitTests.Num(); i++)
			{
				UUnitTest* CurUnitTest = GUnitTestManager->ActiveUnitTests[i];

				if (CurUnitTest == HookOrigin)
				{
					OriginUnitTest = Cast<UClientUnitTest>(CurUnitTest);

					if (OriginUnitTest->UnitNetDriver == NULL || OriginUnitTest->UnitConn == NULL)
					{
						OriginUnitTest = NULL;
					}

					break;
				}
			}
		}


		if (OriginUnitTest != NULL)
		{
			// Verify the net driver matches, so we don't stomp on other unit test actors
			if (Actor != NULL && ((Actor->GetWorld() != NULL && Actor->GetNetDriver() == OriginUnitTest->UnitNetDriver) ||
				OriginUnitTest->UnitConn->ActorChannels.Contains(Actor)))

			{
				UNIT_EVENT_BEGIN(OriginUnitTest);

				bBlockEvent = OriginUnitTest->NotifyScriptProcessEvent(Actor, Function, Parameters, HookOrigin);

				UNIT_EVENT_END;
			}
		}
		else
		{
			UE_LOG(LogUnitTest, Warning, TEXT("Failed to find HookOrigin in active unit test list"));
		}
	}

	return bBlockEvent;
}
#endif

void UClientUnitTest::InternalNotifyNetworkFailure(UWorld* InWorld, UNetDriver* InNetDriver, ENetworkFailure::Type FailureType,
													const FString& ErrorString)
{
	if (InNetDriver == (UNetDriver*)UnitNetDriver)
	{
		UNIT_EVENT_BEGIN(this);

		NotifyNetworkFailure(FailureType, ErrorString);

		UNIT_EVENT_END;
	}
}

void UClientUnitTest::UnitTick(float DeltaTime)
{
	Super::UnitTick(DeltaTime);

	if (!!(UnitTestFlags & EUnitTestFlags::NotifyNetActors) && PendingNetActorChans.Num() > 0 && UnitConn != NULL)
	{
		for (int32 i=PendingNetActorChans.Num()-1; i>=0; i--)
		{
			UActorChannel* CurChan = Cast<UActorChannel>(UnitConn->Channels[PendingNetActorChans[i]]);

			if (CurChan != NULL && CurChan->Actor != NULL)
			{
				NotifyNetActor(CurChan, CurChan->Actor);
				PendingNetActorChans.RemoveAt(i);
			}
		}
	}


	PollProcessOutput();

	// Update/save memory stats
	UpdateProcessStats();
}

void UClientUnitTest::PollProcessOutput()
{
	for (auto CurHandle : ActiveProcesses)
	{
		if (CurHandle.IsValid())
		{
			// Process any log/stdout data
			FString LogDump = TEXT("");
			bool bProcessedPipeRead = false;

			// Need to iteratively grab pipe data, as it has a buffer (can miss data near process-end, for large logs, e.g. stack dumps)
			while (true)
			{
				FString CurStdOut = FPlatformProcess::ReadPipe(CurHandle->ReadPipe);

				if (CurStdOut.Len() > 0)
				{
					LogDump += CurStdOut;
					bProcessedPipeRead = true;
				}
				// Sometimes large reads (typically > 4096 on the write side) clog the pipe buffer,
				// and even looped reads won't receive it all, so when a large enough amount of data gets read, sleep momentarily
				// (not ideal, but it works - spent a long time trying to find a better solution)
				else if (bProcessedPipeRead && LogDump.Len() > 2048)
				{
					bProcessedPipeRead = false;

					FPlatformProcess::Sleep(0.01f);
				}
				else
				{
					break;
				}
			}

			if (LogDump.Len() > 0)
			{
				// Every log line should start with an endline, so if one is missing, print that into the log as an error
				bool bPartialRead = !LogDump.StartsWith(FString(LINE_TERMINATOR));
				const TCHAR* PartialLog = TEXT("--MISSING ENDLINE - PARTIAL PIPE READ--");

				// Now split up the log into multiple lines
				TArray<FString> LogLines;
				
				// @todo JohnB: Perhaps add support for both platforms line terminator, at some stage
#if TARGET_UE4_CL < CL_STRINGPARSEARRAY
				LogDump.ParseIntoArray(&LogLines, LINE_TERMINATOR, true);
#else
				LogDump.ParseIntoArray(LogLines, LINE_TERMINATOR, true);
#endif


				// For process logs, replace the timestamp/category with a tag (e.g. [SERVER]), and set a unique colour so it stands out
				ELogTimes::Type bPreviousPrintLogTimes = GPrintLogTimes;
				GPrintLogTimes = ELogTimes::None;

				SET_WARN_COLOR(CurHandle->MainLogColor);

				// Clear the engine-event log hook, to prevent duplication of the below log entry
				UNIT_EVENT_CLEAR;

				if (bPartialRead)
				{
					UE_LOG(None, Log, TEXT("%s"), PartialLog);
				}

				for (int LineIdx=0; LineIdx<LogLines.Num(); LineIdx++)
				{
					UE_LOG(None, Log, TEXT("%s%s"), *CurHandle->LogPrefix, *(LogLines[LineIdx]));
				}

				// Restore the engine-event log hook
				UNIT_EVENT_RESTORE;

				CLEAR_WARN_COLOR();

				GPrintLogTimes = bPreviousPrintLogTimes;


				// Also output to the unit test log window
				if (LogWindow.IsValid())
				{
					TSharedPtr<SLogWidget>& LogWidget = LogWindow->LogWidget;

					if (LogWidget.IsValid())
					{
						if (bPartialRead)
						{
							LogWidget->AddLine(CurHandle->BaseLogType, MakeShareable(new FString(PartialLog)),
												CurHandle->SlateLogColor);
						}

						for (int LineIdx=0; LineIdx<LogLines.Num(); LineIdx++)
						{
							FString LogLine = CurHandle->LogPrefix + LogLines[LineIdx];

							LogWidget->AddLine(CurHandle->BaseLogType, MakeShareable(new FString(LogLine)), CurHandle->SlateLogColor);
						}
					}
				}


				// Now trigger notification of log lines (only if the unit test is not yet verified though)
				if (VerificationState == EUnitTestVerification::Unverified)
				{
					NotifyProcessLog(CurHandle, LogLines);
				}


				// If the verification state has not changed, pass on the log lines to the error checking code
				CheckOutputForError(CurHandle, LogLines);
			}
		}
	}
}

void UClientUnitTest::PostUnitTick(float DeltaTime)
{
	Super::PostUnitTick(DeltaTime);

	TArray<TSharedPtr<FUnitTestProcess>> HandlesPendingShutdown;
	bool bServerFinished = false;
	bool bClientFinished  = false;

	for (auto CurHandle : ActiveProcesses)
	{
		if (CurHandle.IsValid() && !FPlatformProcess::IsProcRunning(CurHandle->ProcessHandle))
		{
			FString LogMsg = FString::Printf(TEXT("Process '%s' has finished."), *CurHandle->ProcessTag);

			UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
			UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

			HandlesPendingShutdown.Add(CurHandle);


			if (ServerHandle.IsValid() && ServerHandle.HasSameObject(CurHandle.Get()))
			{
				bServerFinished = true;
			}
			else if (ClientHandle.IsValid() && ClientHandle.HasSameObject(CurHandle.Get()))
			{
				bClientFinished = true;
			}
		}
	}

	if (bServerFinished || bClientFinished)
	{
		bool bProcessError = false;
		FString UpdateMsg;

		// If the server just finished, cleanup the fake client
		if (bServerFinished)
		{
			FString LogMsg = FString::Printf(TEXT("Server process has finished, cleaning up fake client."));

			UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
			UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

			if (UnitConn != NULL)
			{
				UnitConn->Close();
			}

			// Immediately cleanup the fake client (don't wait for end-of-life cleanup in CleanupUnitTest)
			CleanupFakeClient();


			// If a server exit was unexpected, mark the unit test as broken
			if (!(UnitTestFlags & EUnitTestFlags::IgnoreServerCrash) && VerificationState == EUnitTestVerification::Unverified)
			{
				UpdateMsg = FString::Printf(TEXT("Unexpected server exit, marking unit test as needing update."));
				bProcessError = true;
			}
		}

		// If a client exit was unexpected, mark the unit test as broken
		if (bClientFinished && !(UnitTestFlags & EUnitTestFlags::IgnoreClientCrash) &&
			VerificationState == EUnitTestVerification::Unverified)
		{
			UpdateMsg = FString::Printf(TEXT("Unexpected client exit, marking unit test as needing update."));
			bProcessError = true;
		}


		// If either the client/server finished, process the error
		if (bProcessError)
		{
			UNIT_LOG(ELogType::StatusFailure | ELogType::StyleBold, TEXT("%s"), *UpdateMsg);
			UNIT_STATUS_LOG(ELogType::StatusFailure | ELogType::StatusVerbose | ELogType::StyleBold, TEXT("%s"), *UpdateMsg);

			VerificationState = EUnitTestVerification::VerifiedNeedsUpdate;
		}
	}


	// Finally, clean up all handles pending shutdown (if they aren't already cleaned up)
	if (HandlesPendingShutdown.Num() > 0)
	{
		for (auto CurHandle : HandlesPendingShutdown)
		{
			if (CurHandle.IsValid())
			{
				ShutdownUnitTestProcess(CurHandle);
			}
		}

		HandlesPendingShutdown.Empty();
	}
}

bool UClientUnitTest::IsTickable() const
{
	bool bReturnVal = Super::IsTickable();

	bReturnVal = bReturnVal && (ActiveProcesses.Num() > 0 ||
					(!!(UnitTestFlags & EUnitTestFlags::NotifyNetActors) && PendingNetActorChans.Num() > 0));

	return bReturnVal;
}


void UClientUnitTest::FinishDestroy()
{
	Super::FinishDestroy();

	// Force close any processes still running
	for (int32 i=ActiveProcesses.Num()-1; i>=0; i--)
	{
		if (ActiveProcesses[i].IsValid())
		{
			ShutdownUnitTestProcess(ActiveProcesses[i]);
		}
	}
}

void UClientUnitTest::ShutdownAfterError()
{
	Super::ShutdownAfterError();

	// Force close any processes still running
	for (int32 i=ActiveProcesses.Num()-1; i>=0; i--)
	{
		if (ActiveProcesses[i].IsValid())
		{
			ShutdownUnitTestProcess(ActiveProcesses[i]);
		}
	}
}

