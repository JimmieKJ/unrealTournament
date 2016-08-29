// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
#include "NUTUtilReflection.h"

#include "LogWindowManager.h"

#include "OnlineBeaconClient.h"

#include "Engine/ActorChannel.h"


// @todo #JohnBMultiFakeClient: Eventually, move >all< of the minimal/headless client handling code, into a new/separate class,
//				so that a single unit test can have multiple minimal clients on a server.
//				This would be useful for licensees, for doing load testing:
//				https://udn.unrealengine.com/questions/247014/clientserver-automation.html

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
	, AllowedClientActors()
	, AllowedClientRPCs()
	, ServerHandle(NULL)
	, ServerAddress(TEXT(""))
	, BeaconAddress(TEXT(""))
	, ClientHandle(NULL)
	, bBlockingServerDelay(false)
	, bBlockingClientDelay(false)
	, bBlockingFakeClientDelay(false)
	, NextBlockingTimeout(0.0)
	, UnitWorld(NULL)
	, UnitNotify(NULL)
	, UnitNetDriver(NULL)
	, UnitConn(NULL)
	, bTriggerredInitialConnect(false)
	, UnitPC(NULL)
	, bUnitPawnSetup(false)
	, UnitNUTActor(NULL)
	, UnitBeacon(NULL)
	, bReceivedPong(false)
	, ControlBunchSequence(0)
	, PendingNetActorChans()
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

bool UClientUnitTest::NotifyAllowNetActor(UClass* ActorClass, bool bActorChannel)
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

	if (!!(UnitTestFlags & EUnitTestFlags::RequireBeacon) && ActorClass->IsChildOf(AOnlineBeaconClient::StaticClass()) &&
		UnitBeacon == NULL)
	{
		bAllow = true;
	}

	if (!bAllow && AllowedClientActors.Num() > 0)
	{
		const auto CheckIsChildOf =
			[&](const UClass* CurEntry)
			{
				return ActorClass->IsChildOf(CurEntry);
			};

		// Use 'ContainsByPredicate' as iterator
		bAllow = AllowedClientActors.ContainsByPredicate(CheckIsChildOf);
	}

	return bAllow;
}

void UClientUnitTest::NotifyNetActor(UActorChannel* ActorChannel, AActor* Actor)
{
	if (!UnitNUTActor.IsValid())
	{
		// Set this even if not required, as it's needed for some UI elements to function
		UnitNUTActor = Cast<ANUTActor>(Actor);

		if (UnitNUTActor.IsValid())
		{
			ResetTimeout(TEXT("NotifyNetActor - UnitNUTActor"));

			if (!!(UnitTestFlags & EUnitTestFlags::RequireNUTActor) && HasAllRequirements())
			{
				ResetTimeout(TEXT("ExecuteClientUnitTest (NotifyNetActor - UnitNUTActor)"));
				ExecuteClientUnitTest();
			}
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
	if (!!(UnitTestFlags & EUnitTestFlags::AutoReconnect))
	{
		UNIT_LOG(ELogType::StatusImportant, TEXT("Detected fake client disconnect when AutoReconnect is enabled. Reconnecting."));

		TriggerAutoReconnect();
	}
	else
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

void UClientUnitTest::NotifySendRawPacket(void* Data, int32 Count, bool& bBlockSend)
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
bool UClientUnitTest::NotifyScriptProcessEvent(AActor* Actor, UFunction* Function, void* Parameters)
{
	bool bBlockEvent = false;

	// Handle UnitTestFlags that require RPC monitoring
	if (AllowedClientRPCs.Num() > 0 || !(UnitTestFlags & EUnitTestFlags::AcceptRPCs) || !!(UnitTestFlags & EUnitTestFlags::DumpReceivedRPC) ||
		!!(UnitTestFlags & EUnitTestFlags::RequirePawn))
	{
		bool bNetClientRPC = !!(Function->FunctionFlags & FUNC_Net) && !!(Function->FunctionFlags & FUNC_NetClient);

		if (bNetClientRPC)
		{
			FString FuncName = Function->GetName();

			// Whether or not to force acceptance of this RPC
			bool bForceAccept = false;

			// Handle detection and proper setup of the PlayerController's pawn
			if (!!(UnitTestFlags & EUnitTestFlags::RequirePawn) && !bUnitPawnSetup && UnitPC != NULL)
			{
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


			bForceAccept = bForceAccept && AllowedClientRPCs.Contains(FuncName);

			// Block RPC's, if they are not accepted
			if (!(UnitTestFlags & EUnitTestFlags::AcceptRPCs) && !bForceAccept)
			{
				FString FuncParms = NUTUtilRefl::FunctionParmsToString(Function, Parameters);

				UNIT_LOG(, TEXT("Blocking receive RPC '%s' for actor '%s'"), *FuncName, *Actor->GetFullName());

				if (FuncParms.Len() > 0)
				{
					UNIT_LOG(, TEXT("     '%s' parameters: %s"), *FuncName, *FuncParms);
				}

				bBlockEvent = true;
			}

			if (!!(UnitTestFlags & EUnitTestFlags::DumpReceivedRPC) && !bBlockEvent)
			{
				FString FuncParms = NUTUtilRefl::FunctionParmsToString(Function, Parameters);

				UNIT_LOG(ELogType::StatusDebug, TEXT("Received RPC '%s' for actor '%s'"), *FuncName, *Actor->GetFullName());

				if (FuncParms.Len() > 0)
				{
					UNIT_LOG(, TEXT("     '%s' parameters: %s"), *FuncName, *FuncParms);
				}
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
	const TArray<FString>* ClientTimeoutResetLogs = NULL;

	UnitEnv->GetServerProgressLogs(ServerStartProgressLogs, ServerReadyLogs, ServerTimeoutResetLogs);
	UnitEnv->GetClientProgressLogs(ClientTimeoutResetLogs);

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

	// If launching a server, delay joining by the fake client, until the server has fully setup, and reset the unit test timeout,
	// each time there is a server log event, that indicates progress in starting up
	if (!!(UnitTestFlags & EUnitTestFlags::LaunchServer) && ServerHandle.IsValid() && InProcess.HasSameObject(ServerHandle.Pin().Get()))
	{
		if (!bTriggerredInitialConnect && (UnitConn == NULL || UnitConn->State == EConnectionState::USOCK_Pending))
		{
			if (ServerReadyLogs->ContainsByPredicate(SearchInLogLine))
			{
				// Fire off fake client connection
				if (UnitConn == NULL)
				{
					bool bBlockingProcess = IsBlockingProcessPresent(true);

					if (bBlockingProcess)
					{
						FString LogMsg = TEXT("Detected successful server startup, delaying fake client due to blocking process.");

						UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
						UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

						bBlockingFakeClientDelay = true;
					}
					else
					{
						FString LogMsg = TEXT("Detected successful server startup, launching fake client.");

						UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
						UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

						ConnectFakeClient();

						bTriggerredInitialConnect = true;
					}
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
				ResetTimeout(FString(TEXT("ServerTimeoutReset: ")) + MatchedLine, true, 60);
			}
		}
	}

	if (!!(UnitTestFlags & EUnitTestFlags::LaunchClient) && ClientHandle.IsValid() && InProcess.HasSameObject(ClientHandle.Pin().Get()))
	{
		if (ClientTimeoutResetLogs->Num() > 0)
		{
			if (ClientTimeoutResetLogs->ContainsByPredicate(SearchInLogLine))
			{
				ResetTimeout(FString(TEXT("ClientTimeoutReset: ")) + MatchedLine, true, 60);
			}
		}
	}

	// @todo #JohnBLowPri: Consider also, adding a way to communicate with launched clients,
	//				to reset their connection timeout upon server progress, if they fully startup before the server does
}

void UClientUnitTest::NotifyProcessFinished(TWeakPtr<FUnitTestProcess> InProcess)
{
	Super::NotifyProcessFinished(InProcess);

	if (InProcess.IsValid())
	{
		bool bServerFinished = false;
		bool bClientFinished  = false;

		if (ServerHandle.IsValid() && ServerHandle.HasSameObject(InProcess.Pin().Get()))
		{
			bServerFinished = true;
		}
		else if (ClientHandle.IsValid() && ClientHandle.HasSameObject(InProcess.Pin().Get()))
		{
			bClientFinished = true;
		}

		if (bServerFinished || bClientFinished)
		{
			bool bProcessError = false;
			FString UpdateMsg;

			// If the server just finished, cleanup the fake client
			if (bServerFinished)
			{
				FString LogMsg = TEXT("Server process has finished, cleaning up fake client.");

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
					UpdateMsg = TEXT("Unexpected server exit, marking unit test as needing update.");
					bProcessError = true;
				}
			}

			// If a client exit was unexpected, mark the unit test as broken
			if (bClientFinished && !(UnitTestFlags & EUnitTestFlags::IgnoreClientCrash) &&
				VerificationState == EUnitTestVerification::Unverified)
			{
				UpdateMsg = TEXT("Unexpected client exit, marking unit test as needing update.");
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
	}
}

void UClientUnitTest::NotifySuspendRequest()
{
	// @todo #JohnBFeature: Currently on suspends the server, suspend the client as well, and also add more granularity
	//						(deciding which to suspend)

#if PLATFORM_WINDOWS
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
#else
	UNIT_LOG(ELogType::StatusImportant, TEXT("Suspend/Resume is only supported in Windows."));
#endif
}

void UClientUnitTest::NotifyProcessSuspendState(TWeakPtr<FUnitTestProcess> InProcess, ESuspendState InSuspendState)
{
	Super::NotifyProcessSuspendState(InProcess, InSuspendState);

	if (InProcess == ServerHandle)
	{
		OnSuspendStateChange.ExecuteIfBound(InSuspendState);
	}

	// @todo #JohnBLowPri: Want to support client process debugging in future?
}


bool UClientUnitTest::NotifyConsoleCommandRequest(FString CommandContext, FString Command)
{
	bool bHandled = Super::NotifyConsoleCommandRequest(CommandContext, Command);

	if (!bHandled)
	{
		if (CommandContext == TEXT("Local"))
		{
			UNIT_LOG_BEGIN(this, ELogType::OriginConsole);
			bHandled = GEngine->Exec(UnitWorld, *Command, *GLog);
			UNIT_LOG_END();
		}
		else if (CommandContext == TEXT("Server"))
		{
			// @todo #JohnBBug: Perhaps add extra checks here, to be sure we're ready to send console commands?
			//
			//				UPDATE: Yes, this is a good idea, because if the client hasn't gotten to the correct login stage
			//				(NMT_Join or such, need to check when server rejects non-login control commands),
			//				then it leads to an early disconnect when you try to spam-send a command early.
			//
			//				It's easy to test this, just type in a command before join, and hold down enter on the edit box to spam it.
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


				UNIT_LOG(ELogType::OriginConsole, TEXT("Sent command '%s' to server."), *Command);

				bHandled = true;
			}
			else
			{
				UNIT_LOG(ELogType::OriginConsole, TEXT("Failed to send console command '%s', no server connection."), *Command);
			}
		}
		else if (CommandContext == TEXT("Client"))
		{
			// @todo #JohnBFeature

			UNIT_LOG(ELogType::OriginConsole, TEXT("Client console commands not yet implemented"));
		}
	}

	return bHandled;
}

void UClientUnitTest::GetCommandContextList(TArray<TSharedPtr<FString>>& OutList, FString& OutDefaultContext)
{
	Super::GetCommandContextList(OutList, OutDefaultContext);

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


bool UClientUnitTest::SendRPCChecked(AActor* Target, const TCHAR* FunctionName, void* Parms, int16 ParmsSize,
										int16 ParmsSizeCorrection/*=0*/)
{
	bool bSuccess = false;
	UFunction* TargetFunc = Target->FindFunction(FName(FunctionName));

	PreSendRPC();

	if (TargetFunc != NULL)
	{
		if (TargetFunc->ParmsSize == ParmsSize + ParmsSizeCorrection)
		{
			if (UnitConn->IsNetReady(false))
			{
				Target->ProcessEvent(TargetFunc, Parms);
			}
			else
			{
				UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to send RPC '%s', network saturated."), FunctionName);
			}
		}
		else
		{
			UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to send RPC '%s', mismatched parameters: '%i' vs '%i' (%i - %i)."),
						FunctionName, TargetFunc->ParmsSize, ParmsSize + ParmsSizeCorrection, ParmsSize, -ParmsSizeCorrection);
		}
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


// @todo #JohnBRefactor: This should be changed to work as compile-time conditionals, if possible
bool UClientUnitTest::ValidateUnitTestSettings(bool bCDOCheck/*=false*/)
{
	bool bSuccess = Super::ValidateUnitTestSettings();

	// Currently, unit tests don't support NOT launching/connecting to a server
	UNIT_ASSERT(!!(UnitTestFlags & EUnitTestFlags::LaunchServer));

	// If launching a server, make sure the base URL for the server is set
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::LaunchServer) || BaseServerURL.Len() > 0);

	// If launching a client, make sure some default client parameters have been set (to avoid e.g. launching fullscreen)
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::LaunchClient) || BaseClientParameters.Len() > 0);


	// You can't specify an allowed actors whitelist, without the AcceptActors flag
	UNIT_ASSERT(AllowedClientActors.Num() == 0 || !!(UnitTestFlags & EUnitTestFlags::AcceptActors));

	// If you require a player/NUTActor, you need to accept actor channels
	UNIT_ASSERT((!(UnitTestFlags & EUnitTestFlags::AcceptPlayerController) && !(UnitTestFlags & EUnitTestFlags::RequireNUTActor)) ||
					!!(UnitTestFlags & EUnitTestFlags::AcceptActors));

	// Don't require a PlayerController, if you don't accept one
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::RequirePlayerController) ||
					!!(UnitTestFlags & EUnitTestFlags::AcceptPlayerController));

	// If you require a pawn, you must require a PlayerController
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::RequirePawn) || !!(UnitTestFlags & EUnitTestFlags::RequirePlayerController));

	// If you require a pawn, you must enable NotifyProcessEvent
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::RequirePawn) || !!(UnitTestFlags & EUnitTestFlags::NotifyProcessEvent));

	// If you want to dump received RPC's, you need to hook NotifyProcessEvent
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::DumpReceivedRPC) || !!(UnitTestFlags & EUnitTestFlags::NotifyProcessEvent));

	// You can't whitelist client RPC's (i.e. unblock whitelisted RPC's), unless all RPC's are blocked by default
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::AcceptRPCs) || AllowedClientRPCs.Num() == 0);

#if UE_BUILD_SHIPPING
	// You can't hook ProcessEvent or block RPCs in shipping builds, as the main engine hook is not available in shipping; soft-fail
	if (!!(UnitTestFlags & EUnitTestFlags::NotifyProcessEvent) || !(UnitTestFlags & EUnitTestFlags::AcceptRPCs))
	{
		UNIT_LOG(ELogType::StatusFailure | ELogType::StyleBold, TEXT("Unit tests run in shipping mode, can't hook ProcessEvent."));

		bSuccess = false;
	}
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

	// Don't specify server-dependent flags, if not auto-launching a server
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::LaunchClient) || !!(UnitTestFlags & EUnitTestFlags::LaunchServer));

	// If DumpReceivedRaw is set, make sure hooking of ReceivedRawPacket is enabled too
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::DumpReceivedRaw) || !!(UnitTestFlags & EUnitTestFlags::CaptureReceivedRaw));

	// If DumpSendRaw is set, make sure hooking of SendRawPacket is enabled too
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::DumpSendRaw) || !!(UnitTestFlags & EUnitTestFlags::CaptureSendRaw));

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

bool UClientUnitTest::HasAllRequirements(bool bIgnoreCustom/*=false*/)
{
	bool bReturnVal = true;

	// The fake client creation/connection is now delayed, so need to wait for that too
	if (UnitConn == NULL)
	{
		bReturnVal = false;
	}

	EUnitTestFlags RequiredFlags = (UnitTestFlags & EUnitTestFlags::RequirementsMask);

	if (bIgnoreCustom)
	{
		RequiredFlags &= ~EUnitTestFlags::RequireCustom;
	}

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

	if (bResetConnTimeout)
	{
		ResetConnTimeout((float)(FMath::Max(MinDuration, UnitTestTimeout)));
	}
}

void UClientUnitTest::ResetConnTimeout(float Duration)
{
	if (UnitConn != NULL && UnitConn->State != USOCK_Closed && UnitConn->Driver != NULL)
	{
		// @todo #JohnBHack: This is a slightly hacky way of setting the timeout to a large value, which will be overridden by newly
		//				received packets, making it unsuitable for most situations (except crashes - but that could still be subject
		//				to a race condition)
		double NewLastReceiveTime = UnitConn->Driver->Time + Duration;

		UnitConn->LastReceiveTime = FMath::Max(NewLastReceiveTime, UnitConn->LastReceiveTime);
	}
}


bool UClientUnitTest::ExecuteUnitTest()
{
	bool bSuccess = false;

	bool bValidSettings = ValidateUnitTestSettings();

	// @todo #JohnBLowPri: Fix support for Steam eventually
	bool bSteamAvailable = NUTNet::IsSteamNetDriverAvailable();

	if (bValidSettings && !bSteamAvailable)
	{
		if (!!(UnitTestFlags & EUnitTestFlags::LaunchServer))
		{
			bool bBlockingProcess = IsBlockingProcessPresent(true);

			if (bBlockingProcess)
			{
				FString LogMsg = TEXT("Delaying server startup due to blocking process");

				UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
				UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

				bBlockingServerDelay = true;
			}
			else
			{
				StartUnitTestServer();
			}

			if (!!(UnitTestFlags & EUnitTestFlags::LaunchClient))
			{
				if (bBlockingProcess)
				{
					FString LogMsg = TEXT("Delaying client startup due to blocking process");

					UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
					UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

					bBlockingClientDelay = true;
				}
				else
				{
					// Client handle is set outside of StartUnitTestClient, in case support for multiple clients is added later
					ClientHandle = StartUnitTestClient(ServerAddress);
				}
			}
		}

		// No longer immediately setup the fake client, wait for the server to fully startup first (monitoring its log output)
#if 0
		bSuccess = ConnectFakeClient();
		bTriggerredInitialConnect = true;
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

#if !UE_BUILD_SHIPPING
	RemoveProcessEventCallback(this, &UClientUnitTest::InternalScriptProcessEvent);
#endif

	Super::CleanupUnitTest();
}

bool UClientUnitTest::ConnectFakeClient(FUniqueNetIdRepl* InNetID/*=NULL*/)
{
	bool bSuccess = false;

	bTriggerredInitialConnect = true;

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

			if (!(UnitTestFlags & EUnitTestFlags::AcceptRPCs) || !!(UnitTestFlags & EUnitTestFlags::NotifyProcessEvent))
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
#if TARGET_UE4_CL >= CL_DEPRECATEDEL
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
						CurUnitConn->SocketHook.SendToDel.BindUObject(this, &UClientUnitTest::NotifySocketSendRawPacket);
					}

					CurUnitConn->ReplicatedActorSpawnDel.BindUObject(this, &UClientUnitTest::NotifyAllowNetActor);

					if (HasAllRequirements())
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

// @todo #JohnBRefactor: Reconsider naming of this function, if you modify the other functions using 'fake'
void UClientUnitTest::CleanupFakeClient()
{
	if (UnitWorld != NULL && UnitNetDriver != NULL)
	{
		// @todo #JohnBRefactor: As with the 'CreateFakePlayer' function, naming it 'fake' may not be optimal
		NUTNet::DisconnectFakePlayer(UnitWorld, UnitNetDriver);

		UnitNetDriver->Notify = NULL;
	}

	UnitNetDriver = NULL;
	UnitPC = NULL;
	UnitConn = NULL;
	bUnitPawnSetup = false;
	UnitNUTActor = NULL;
	UnitBeacon = nullptr;
	bReceivedPong = false;
	ControlBunchSequence = 0;
	PendingNetActorChans.Empty();

	if (UnitNotify != NULL)
	{
		delete UnitNotify;
		UnitNotify = NULL;
	}

	if (GEngine != NULL)
	{
#if TARGET_UE4_CL >= CL_DEPRECATEDEL
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

void UClientUnitTest::TriggerAutoReconnect()
{
	UNIT_LOG(ELogType::StatusImportant, TEXT("Performing Auto-Reconnect."))

	CleanupFakeClient();
	ConnectFakeClient();
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
		int ServerBeaconPort = ServerPort + 5;


		// Setup the launch URL
		FString ServerParameters = ConstructServerParameters() + FString::Printf(TEXT(" -Port=%i"), ServerPort);

		if (!!(UnitTestFlags & EUnitTestFlags::BeaconConnect))
		{
			ServerParameters += FString::Printf(TEXT(" -BeaconPort=%i"), ServerBeaconPort);
		}

		ServerHandle = StartUE4UnitTestProcess(ServerParameters);

		if (ServerHandle.IsValid())
		{
			ServerAddress = FString::Printf(TEXT("127.0.0.1:%i"), ServerPort);

			if (!!(UnitTestFlags & EUnitTestFlags::BeaconConnect))
			{
				BeaconAddress = FString::Printf(TEXT("127.0.0.1:%i"), ServerBeaconPort);
			}

			auto CurHandle = ServerHandle.Pin();

			CurHandle->ProcessTag = FString::Printf(TEXT("UE4_Server_%i"), CurHandle->ProcessID);
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
							TEXT(" -unattended -CrashForUAT");
	
							// Removed this, to support detection of shader compilation, based on shader compiler .exe
							//TEXT(" -NoShaderWorker");

	return Parameters;
}

TWeakPtr<FUnitTestProcess> UClientUnitTest::StartUnitTestClient(FString ConnectIP, bool bMinimized/*=true*/)
{
	TWeakPtr<FUnitTestProcess> ReturnVal = NULL;

	FString LogMsg = TEXT("Unit test launching a client");

	UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
	UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

	FString ClientParameters = ConstructClientParameters(ConnectIP);

	ReturnVal = StartUE4UnitTestProcess(ClientParameters, bMinimized);

	if (ReturnVal.IsValid())
	{
		auto CurHandle = ReturnVal.Pin();

		// @todo #JohnBMultiClient: If you add support for multiple clients, make the log prefix numbered, also try to differentiate colours
		CurHandle->ProcessTag = FString::Printf(TEXT("UE4_Client_%i"), CurHandle->ProcessID);
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
							TEXT(" -unattended -CrashForUAT -nosound");

							// Removed this, to support detection of shader compilation, based on shader compiler .exe
							//TEXT(" -NoShaderWorker")

	return Parameters;
}

void UClientUnitTest::PrintUnitTestProcessErrors(TSharedPtr<FUnitTestProcess> InHandle)
{
	// If this was the server, and we were not expecting a crash, print out a warning
	if (!(UnitTestFlags & EUnitTestFlags::ExpectServerCrash) && ServerHandle.IsValid() && InHandle == ServerHandle.Pin())
	{
		FString LogMsg = TEXT("WARNING: Got server crash, but unit test not marked as expecting a server crash.");

		STATUS_SET_COLOR(FLinearColor(1.0, 1.0, 0.0));

		UNIT_LOG(ELogType::StatusWarning, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusWarning, TEXT("%s"), *LogMsg);

		STATUS_RESET_COLOR();
	}

	Super::PrintUnitTestProcessErrors(InHandle);
}


#if !UE_BUILD_SHIPPING
// @todo #JohnB: 'HookOrigin' is not optimal - try to at least make it a UClientUnitTest pointer
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

				bBlockEvent = OriginUnitTest->NotifyScriptProcessEvent(Actor, Function, Parameters);

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
	if (bBlockingServerDelay || bBlockingClientDelay || bBlockingFakeClientDelay)
	{
		bool bBlockingProcess = IsBlockingProcessPresent();

		if (!bBlockingProcess)
		{
			ResetTimeout(TEXT("Blocking Process Reset"), true, 60);

			auto IsWaitingOnTimeout =
				[&]()
				{
					return NextBlockingTimeout > FPlatformTime::Seconds();
				};

			if (bBlockingServerDelay && !IsWaitingOnTimeout())
			{
				StartUnitTestServer();

				bBlockingServerDelay = false;
				NextBlockingTimeout = FPlatformTime::Seconds() + 10.0;
			}

			if (bBlockingClientDelay && !IsWaitingOnTimeout())
			{
				ClientHandle = StartUnitTestClient(ServerAddress);

				bBlockingClientDelay = false;
				NextBlockingTimeout = FPlatformTime::Seconds() + 10.0;
			}

			if (bBlockingFakeClientDelay && !IsWaitingOnTimeout())
			{
				ConnectFakeClient();

				bTriggerredInitialConnect = true;
				bBlockingFakeClientDelay = false;
				NextBlockingTimeout = FPlatformTime::Seconds() + 10.0;
			}
		}
	}

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

	// Prevent net connection timeout in developer mode
	if (bDeveloperMode)
	{
		ResetConnTimeout(120.f);
	}

	Super::UnitTick(DeltaTime);
}

bool UClientUnitTest::IsTickable() const
{
	bool bReturnVal = Super::IsTickable();

	bReturnVal = bReturnVal || bDeveloperMode || bBlockingServerDelay || bBlockingClientDelay || bBlockingFakeClientDelay ||
					(!!(UnitTestFlags & EUnitTestFlags::NotifyNetActors) && PendingNetActorChans.Num() > 0);

	return bReturnVal;
}

