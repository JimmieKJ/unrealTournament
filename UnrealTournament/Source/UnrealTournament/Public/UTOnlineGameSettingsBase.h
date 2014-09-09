// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#define SETTING_SERVERNAME FName(TEXT("UT_SERVERNAME"))
#define SETTING_SERVERVERSION FName(TEXT("UT_SERVERVERSION"))
#define SETTING_SERVERMOTD FName(TEXT("UT_SERVERMOTD"))
#define SETTING_PLAYERSONLINE FName(TEXT("UT_PLAYERONLINE"))
#define SETTING_SPECTATORSONLINE FName(TEXT("UT_SPECTATORSONLINE"))

class FUTOnlineGameSettingsBase : public FOnlineSessionSettings
{
public:
	FUTOnlineGameSettingsBase(bool bIsLanGame = false, bool bIsPresense = false, int32 MaxNumberPlayers = 32);
	virtual ~FUTOnlineGameSettingsBase(){}

	virtual void ApplyGameSettings(AUTGameMode* CurrentGame);
};