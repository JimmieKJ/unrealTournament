// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTProfileSettings.h"

UUTProfileSettings::UUTProfileSettings(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	PlayerName = TEXT("Malcolm");
}


void UUTProfileSettings::SetPlayerName(FString NewName)
{
	PlayerName = NewName;
}

FString UUTProfileSettings::GetPlayerName()
{
	return PlayerName;
}
