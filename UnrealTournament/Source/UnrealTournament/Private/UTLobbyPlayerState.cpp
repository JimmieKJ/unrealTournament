// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/PlayerState.h"
#include "UTLobbyGameMode.h"
#include "UTLobbyPlayerState.h"
#include "Net/UnrealNetwork.h"

AUTLobbyPlayerState::AUTLobbyPlayerState(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void AUTLobbyPlayerState::PreInitializeComponents()
{
	DesiredQuickStartGameMode = TEXT("");
	if (GetWorld()->GetNetMode() == ENetMode::NM_Client)
	{
		bHostInitializationComplete = false;
	}
}


void AUTLobbyPlayerState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUTLobbyPlayerState, CurrentMatch);
}

void AUTLobbyPlayerState::MatchButtonPressed()
{
	if (CurrentMatch == NULL)
	{
		ServerCreateMatch();
	}
	else
	{
		ServerDestroyOrLeaveMatch();
	}
}

bool AUTLobbyPlayerState::ServerCreateMatch_Validate() { return true; }
void AUTLobbyPlayerState::ServerCreateMatch_Implementation()
{
	if (CurrentMatch == NULL)
	{
		AUTLobbyGameState* GameState = GetWorld()->GetGameState<AUTLobbyGameState>();
		if (GameState)
		{
			GameState->AddNewMatch(this);
		}
	}
}

bool AUTLobbyPlayerState::ServerDestroyOrLeaveMatch_Validate() { return true; }
void AUTLobbyPlayerState::ServerDestroyOrLeaveMatch_Implementation()
{
	AUTLobbyGameState* GameState = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (GameState)
	{
		GameState->RemoveFromAMatch(this);
	}
}

bool AUTLobbyPlayerState::ServerJoinMatch_Validate(AUTLobbyMatchInfo* MatchToJoin) { return true; }
void AUTLobbyPlayerState::ServerJoinMatch_Implementation(AUTLobbyMatchInfo* MatchToJoin)
{
	// CurrentMatch may have been set by a previous call if client is lagged and sends multiple requests
	if (CurrentMatch != MatchToJoin)
	{
		AUTLobbyGameState* GameState = GetWorld()->GetGameState<AUTLobbyGameState>();
		if (GameState != NULL)
		{
			if (CurrentMatch != NULL)
			{
				GameState->RemoveFromAMatch(this);
			}
			GameState->JoinMatch(MatchToJoin, this);
		}
	}
}


void AUTLobbyPlayerState::AddedToMatch(AUTLobbyMatchInfo* Match)
{
	CurrentMatch = Match;
	bIsInMatch = true;
}

void AUTLobbyPlayerState::RemovedFromMatch(AUTLobbyMatchInfo* Match)
{
	CurrentMatch = NULL;
	bIsInMatch = false;
	bReadyToPlay = false;
}

void AUTLobbyPlayerState::ClientMatchError_Implementation(const FText &MatchErrorMessage)
{
	AUTBasePlayerController* BasePC = Cast<AUTBasePlayerController>(GetOwner());
	if (BasePC)
	{
#if !UE_SERVER
		BasePC->ShowMessage(NSLOCTEXT("LobbyMessage","MatchMessage","Match Message"), MatchErrorMessage, UTDIALOG_BUTTON_OK, NULL);	
#endif
	}
}

void AUTLobbyPlayerState::ClientConnectToInstance_Implementation(const FString& GameInstanceGUIDString, const FString& LobbyGUIDString, bool bAsSpectator)
{
	AUTBasePlayerController* BPC = Cast<AUTBasePlayerController>(GetOwner());
	if (BPC)
	{
		UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(BPC->Player);
		if (LocalPlayer)
		{
			LocalPlayer->RememberLobby(LobbyGUIDString);
		}

		BPC->ConnectToServerViaGUID(GameInstanceGUIDString, bAsSpectator);
	}
}

void AUTLobbyPlayerState::OnRep_CurrentMatch()
{
	bIsInMatch = CurrentMatch != NULL;

#if !UE_SERVER
	if (CurrentMatchChangedDelegate.IsBound())
	{
		CurrentMatchChangedDelegate.Execute(this);
	}

#endif
}

void AUTLobbyPlayerState::StartServerToClientDataPush_Implementation()
{
	AUTLobbyGameState* GS = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (GS)
	{
		ServerBeginDataPush();
	}
	else
	{
		// Check again in a little bit....
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTLobbyPlayerState::StartServerToClientDataPush_Implementation, 0.05, false);	
	}
}

bool AUTLobbyPlayerState::ServerBeginDataPush_Validate() { return true; }
void AUTLobbyPlayerState::ServerBeginDataPush_Implementation()
{
	//UE_LOG(UT,Log,TEXT("Beginning Datapush to %s"), *PlayerName);

	CurrentBulkID = 0;
	DataIndex = 0;
	SendNextBulkBlock();
}

void AUTLobbyPlayerState::SendNextBulkBlock()
{
	uint8 SendCount = FMath::Clamp<int>(HostMatchData.Num() - DataIndex, 0, 10);
	//UE_LOG(UT,Log,TEXT("Sending next Block to %s (%i)"), *PlayerName, SendCount);

	CurrentBulkID++;
	if (SendCount > 0)
	{
		for (int i = 0; i < SendCount; i++)	// Send 10 maps at time.. make this configurable.
		{
			ClientReceiveMatchData(SendCount, CurrentBulkID, HostMatchData[DataIndex++]);
		}
	}
	else
	{
		ClientReceivedAllData();
	}

}

void AUTLobbyPlayerState::ClientReceiveMatchData_Implementation(uint8 BulkSendCount, uint16 BulkSendID, const FString& MatchData)
{
	//UE_LOG(UT,Log,TEXT("Client %s has recieved data %i %i"), *PlayerName, BulkSendCount, BulkSendID);

	AUTLobbyGameState* GS = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (GS)
	{
		// Look to see if this is a new block.
		if (BulkSendID != CurrentBulkID)
		{
			if (CurrentBlockCount != ExpectedBlockCount)
			{
				UE_LOG(UT, Verbose, TEXT("ERROR: Didn't receive everything in the block %i %i %i"), BulkSendID, CurrentBlockCount, ExpectedBlockCount);
			}

			CurrentBulkID = BulkSendID;
			CurrentBlockCount = 0;
			ExpectedBlockCount = BulkSendCount;
		}

		// Parse the Match data.

		FString DataType;
		FString Data;
		if (MatchData.Split(TEXT("="), &DataType, &Data))
		{
			if (DataType.ToLower() == TEXT("game"))
			{
				AUTGameMode* DefaultGame = AUTLobbyGameState::GetGameModeDefaultObject(Data);
				if (DefaultGame)
				{
					GS->ClientAvailbleGameModes.Add(FAllowedGameModeData::Make(Data, DefaultGame->DisplayName.ToString(), DefaultGame));
				}
			}
			else if (DataType.ToLower() == TEXT("map"))
			{
				FString Name, Guid;
				Data.Split(TEXT(":"), &Name, &Guid);
				// Add code to check if the map exists..
				GS->ClientAvailableMaps.Add(FAllowedMapData::MakeShared(Name, Guid));
			}
		}


		CurrentBlockCount++;
		//UE_LOG(UT,Log,TEXT("----- Client %s %i %i"), *PlayerName, CurrentBlockCount, ExpectedBlockCount);
		if (CurrentBlockCount == ExpectedBlockCount)
		{
			ServerACKBulkCompletion(CurrentBulkID);
		}
	}
	else
	{
		UE_LOG(UT,Verbose,TEXT("ERROR: Recieved Bulk data before GameState!!!"));
	}
}

bool AUTLobbyPlayerState::ServerACKBulkCompletion_Validate(uint16 BuildSendID) { return true; }
void AUTLobbyPlayerState::ServerACKBulkCompletion_Implementation(uint16 BuildSendID)
{
	//UE_LOG(UT,Log,TEXT("Server recieved Bulk ACK from %s"), *PlayerName);
	SendNextBulkBlock();
}

void AUTLobbyPlayerState::ClientReceivedAllData_Implementation()
{
	//UE_LOG(UT,Log,TEXT("Client %s has Received All Data"), *PlayerName);
	bHostInitializationComplete = true;
	ServerACKReceivedAllData();
}

bool AUTLobbyPlayerState::ServerACKReceivedAllData_Validate() { return true; }
void AUTLobbyPlayerState::ServerACKReceivedAllData_Implementation()
{
	//UE_LOG(UT,Log,TEXT("######### [Initial Player Replication is Completed for %s"), *PlayerName);

	// This client is now ready with all of the information it needs to start a match.  Look to see if they were previously in a match
	AUTLobbyGameState* GS = GetWorld()->GetGameState<AUTLobbyGameState>();
	if (GS)
	{
		GS->CheckForExistingMatch(this);
	}
}
