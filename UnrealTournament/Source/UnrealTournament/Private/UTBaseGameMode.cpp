// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"

AUTBaseGameMode::AUTBaseGameMode(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void AUTBaseGameMode::InitGame( const FString& MapName, const FString& Options, FString& ErrorMessage )
{
	// Grab the InstanceID if it's there.
	LobbyInstanceID = GetIntOption( Options, TEXT("InstanceID"), 0);

	// Create a server instance id for this server
	ServerInstanceGUID = FGuid::NewGuid();

	Super::InitGame(MapName, Options, ErrorMessage);

	ServerPassword = TEXT("");
	ServerPassword = ParseOption(Options, TEXT("ServerPassword"));
	bRequirePassword = !ServerPassword.IsEmpty();

	UE_LOG(UT,Log,TEXT("Password: %i %s"), bRequirePassword, ServerPassword.IsEmpty() ? TEXT("NONE") : *ServerPassword)
}

FName AUTBaseGameMode::GetNextChatDestination(AUTPlayerState* PlayerState, FName CurrentChatDestination)
{
	if (CurrentChatDestination == ChatDestinations::Local) return ChatDestinations::Team;
	if (CurrentChatDestination == ChatDestinations::Team)
	{
		if (IsGameInstanceServer())
		{
			return ChatDestinations::Lobby;
		}
	}

	return ChatDestinations::Local;
}

int32 AUTBaseGameMode::GetInstanceData(TArray<FString>& HostNames, TArray<FString>& Descriptions)
{
	return 0;
}