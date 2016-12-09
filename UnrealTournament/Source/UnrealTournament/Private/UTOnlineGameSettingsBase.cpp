// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "OnlineSessionSettings.h"
#include "UTOnlineGameSettingsBase.h"
#include "UTGameEngine.h"
#include "UnrealNetwork.h"
#include "Misc/NetworkVersion.h"

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

void FUTOnlineGameSettingsBase::ApplyGameSettings(FOnlineSessionSettings* InSettings, AUTBaseGameMode* CurrentGame, AUTGameSession* CurrentSession) const
{
	if (!CurrentGame) return;

	// Stub function.  We will need to fill this out later.
	InSettings->bIsDedicated = CurrentGame->GetWorld()->GetNetMode() == NM_DedicatedServer;

	InSettings->Set(SETTING_GAMEMODE, CurrentGame->GetClass()->GetPathName(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	InSettings->Set(SETTING_MAPNAME, CurrentGame->GetWorld()->GetMapName(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	AUTGameMode* UTGameMode = Cast<AUTGameMode>(CurrentGame);
	if (UTGameMode)
	{
		InSettings->Set(SETTING_GAMENAME, UTGameMode->DisplayName.ToString(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		InSettings->Set(SETTING_UTMATCHELO, UTGameMode->RankCheck, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	}

	int32 RedTeamSize = 0;
	int32 BlueTeamSize = 0;

	AUTGameState* GameState = CurrentGame->GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		InSettings->Set(SETTING_SERVERNAME, GameState->ServerName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		InSettings->Set(SETTING_SERVERMOTD, GameState->ServerMOTD, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

		RedTeamSize = GameState->Teams.Num() > 0 ? GameState->Teams[0]->GetSize() : 0;
		BlueTeamSize = GameState->Teams.Num() > 1 ? GameState->Teams[1]->GetSize() : 0;

		InSettings->Set(SETTING_UTMATCHDURATION, GameState->TimeLimit, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		InSettings->Set(SETTING_UTMATCHELAPSEDTIME, GameState->ElapsedTime);
	}

	InSettings->Set(SETTING_UTREDTEAMSIZE, RedTeamSize, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	InSettings->Set(SETTING_UTBLUETEAMSIZE, BlueTeamSize, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	InSettings->Set(SETTING_TRAININGGROUND, CurrentGame->bTrainingGround,EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	InSettings->Set(SETTING_MINELO, CurrentGame->MinAllowedRank,EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	InSettings->Set(SETTING_MAXELO, CurrentGame->MaxAllowedRank,EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	InSettings->Set(SETTING_PLAYERSONLINE, CurrentGame->NumPlayers, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	InSettings->Set(SETTING_SPECTATORSONLINE, CurrentGame->NumSpectators, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
		
	FString GameVer = FString::Printf(TEXT("%i"), FNetworkVersion::GetLocalNetworkVersion());
	InSettings->Set(SETTING_SERVERVERSION, GameVer, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	InSettings->Set(SETTING_SERVERINSTANCEGUID, CurrentGame->ServerInstanceGUID.ToString(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	int32 bGameInstanceServer = CurrentGame->IsGameInstanceServer();
	InSettings->Set(SETTING_GAMEINSTANCE, bGameInstanceServer, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	int32 ServerFlags = 0x0;
	if (CurrentGame->bRequirePassword && !CurrentGame->ServerPassword.IsEmpty()) ServerFlags = ServerFlags | SERVERFLAG_RequiresPassword;			// Passworded

	InSettings->Set(SETTING_SERVERFLAGS, ServerFlags, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	int32 NumMatches = CurrentGame->GetNumMatches();
	InSettings->Set(SETTING_NUMMATCHES, NumMatches, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	InSettings->Set(SETTING_UTMAXPLAYERS, CurrentSession->MaxPlayers, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	InSettings->Set(SETTING_UTMAXSPECTATORS, CurrentSession->MaxSpectators, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);


	InSettings->Set(SETTING_UTMATCHSTATE, CurrentGame->GetMatchState().ToString(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
}

