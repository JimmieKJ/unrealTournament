// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameUserSettings.h"

namespace EUTGameUserSettingsVersion
{
	enum Type
	{
		UT_GAMEUSERSETTINGS_VERSION = 1
	};
}


UUTGameUserSettings::UUTGameUserSettings(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

bool UUTGameUserSettings::IsVersionValid()
{
	return (Super::IsVersionValid() && (UTVersion == EUTGameUserSettingsVersion::UT_GAMEUSERSETTINGS_VERSION));
}

void UUTGameUserSettings::UpdateVersion()
{
	Super::UpdateVersion();
	UTVersion = EUTGameUserSettingsVersion::UT_GAMEUSERSETTINGS_VERSION;
}


void UUTGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	PlayerName = TEXT("Player");
}

void UUTGameUserSettings::ApplySettings()
{
	Super::ApplySettings();

	// Set the player name on the first player
	TArray<APlayerController*> PlayerList;
	GEngine->GetAllLocalPlayerControllers(PlayerList);
	for (auto It = PlayerList.CreateIterator(); It; ++It)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(*It);
		if (PC != NULL)
		{
			PC->SetName(PlayerName);
		}
		break;
	}

}

void UUTGameUserSettings::SetPlayerName(FString NewPlayerName)
{
	PlayerName = NewPlayerName;
}

FString UUTGameUserSettings::GetPlayerName()
{
	return PlayerName;
}
