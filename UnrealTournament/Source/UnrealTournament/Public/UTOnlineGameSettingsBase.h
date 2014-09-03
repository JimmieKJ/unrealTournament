// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#define SETTING_SERVERNAME FName(TEXT("SERVERNAME"))
#define SETTING_MOTD FName(TEXT("MOTD"))
#define SETTING_PLAYERSONLINE FName(TEXT("PLAYERONLINE"))
#define SETTING_SPECTATORSONLINE FName(TEXT("SPECTATORSONLINE"))


class FUTOnlineGameSettingsBase : public FOnlineSessionSettings
{
public:
	FUTOnlineGameSettingsBase(bool bIsLanGame = false, bool bIsPresense = false, int32 MaxNumberPlayers = 32);
	virtual ~FUTOnlineGameSettingsBase(){}

	virtual void ApplyGameSettings(AUTGameMode* CurrentGame);
	virtual void UpdateGameSettings(AUTGameMode* CurrentGame);
};