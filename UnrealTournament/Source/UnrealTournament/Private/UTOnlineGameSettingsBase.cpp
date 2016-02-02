// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "OnlineSessionSettings.h"
#include "UTOnlineGameSettingsBase.h"
#include "UTGameEngine.h"
#include "UnrealNetwork.h"

FUTOnlineGameSettingsBase::FUTOnlineGameSettingsBase(bool bIsLanGame, bool bIsPresense, bool bPrivate, int32 MaxNumberPlayers)
{
	NumPublicConnections = FMath::Max(MaxNumberPlayers, 0);
	NumPrivateConnections = 0;
	bIsLANMatch = bIsLanGame;

	bShouldAdvertise = true;
	bAllowJoinInProgress = true;
	bAllowInvites = !bPrivate;
	bUsesPresence = bIsPresense;
	bAllowJoinViaPresence = !bPrivate;
	bAllowJoinViaPresenceFriendsOnly = !bPrivate;
}

void FUTOnlineGameSettingsBase::ApplyGameSettings(FOnlineSessionSettings* Settings, AUTBaseGameMode* CurrentGame, AUTGameSession* CurrentSession) const
{
	if (!CurrentGame) return;

	// Stub function.  We will need to fill this out later.
	Settings->bIsDedicated = CurrentGame->GetWorld()->GetNetMode() == NM_DedicatedServer;

	Settings->Set(SETTING_GAMEMODE, CurrentGame->GetClass()->GetPathName(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings->Set(SETTING_MAPNAME, CurrentGame->GetWorld()->GetMapName(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	AUTGameMode* UTGameMode = Cast<AUTGameMode>(CurrentGame);
	if (UTGameMode)
	{
		Settings->Set(SETTING_GAMENAME, UTGameMode->DisplayName.ToString(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	}

	int32 RedTeamSize = 0;
	int32 BlueTeamSize = 0;

	AUTGameState* GameState = CurrentGame->GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		Settings->Set(SETTING_SERVERNAME, GameState->ServerName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		Settings->Set(SETTING_SERVERMOTD, GameState->ServerMOTD, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

		RedTeamSize = GameState->Teams.Num() > 0 ? GameState->Teams[0]->GetSize() : 0;
		BlueTeamSize = GameState->Teams.Num() > 1 ? GameState->Teams[1]->GetSize() : 0;

		Settings->Set(SETTING_UTMATCHDURATION, GameState->TimeLimit, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		Settings->Set(SETTING_UTMATCHELAPSEDTIME, GameState->ElapsedTime);
	}

	Settings->Set(SETTING_UTREDTEAMSIZE, RedTeamSize, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings->Set(SETTING_UTBLUETEAMSIZE, BlueTeamSize, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	Settings->Set(SETTING_TRAININGGROUND, CurrentGame->bTrainingGround,EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings->Set(SETTING_MINELO, CurrentGame->MinAllowedRank,EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings->Set(SETTING_MAXELO, CurrentGame->MaxAllowedRank,EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	Settings->Set(SETTING_PLAYERSONLINE, CurrentGame->NumPlayers, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings->Set(SETTING_SPECTATORSONLINE, CurrentGame->NumSpectators, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		
	FString GameVer = FString::Printf(TEXT("%i"), FNetworkVersion::GetLocalNetworkVersion());
	Settings->Set(SETTING_SERVERVERSION, GameVer, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings->Set(SETTING_SERVERINSTANCEGUID, CurrentGame->ServerInstanceGUID.ToString(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	int32 bGameInstanceServer = CurrentGame->IsGameInstanceServer();
	Settings->Set(SETTING_GAMEINSTANCE, bGameInstanceServer, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	int32 ServerFlags = 0x0;
	if (CurrentGame->bRequirePassword && !CurrentGame->ServerPassword.IsEmpty()) ServerFlags = ServerFlags | SERVERFLAG_RequiresPassword;			// Passworded

	Settings->Set(SETTING_SERVERFLAGS, ServerFlags, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	int32 NumMatches = CurrentGame->GetNumMatches();
	Settings->Set(SETTING_NUMMATCHES, NumMatches, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	Settings->Set(SETTING_UTMAXPLAYERS, CurrentSession->MaxPlayers, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings->Set(SETTING_UTMAXSPECTATORS, CurrentSession->MaxSpectators, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);


	Settings->Set(SETTING_UTMAXSPECTATORS, CurrentGame->GetMatchState().ToString(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings->Set(SETTING_UTMATCHELO, CurrentGame->GetAverageElo(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
}

