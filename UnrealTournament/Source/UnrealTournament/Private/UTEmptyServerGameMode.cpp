// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTEmptyServerGameMode.h"
#include "UTGameSessionRanked.h"


AUTEmptyServerGameMode::AUTEmptyServerGameMode()
{
	// Hard travel into the next game mode, we don't want to seamless travel or any connecting clients might find themselves here
	bUseSeamlessTravel = false;

	// Restart the server after 10 minutes of idling, just in case the session gets messed up
	//FailsafeServerRestartTimer = 60.f * 10.f;

	PlayerControllerClass = AUTBasePlayerController::StaticClass();
}

TSubclassOf<AGameSession> AUTEmptyServerGameMode::GetGameSessionClass() const
{
	return AUTGameSessionRanked::StaticClass();
}

void AUTEmptyServerGameMode::InitGameState()
{
	Super::InitGameState();

	if (GameSession != NULL && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		FOnlineSessionSettings* SessionSettings = NULL;
		IOnlineSessionPtr SessionInt = Online::GetSessionInterface();
		if (SessionInt.IsValid())
		{
			SessionSettings = SessionInt->GetSessionSettings(GameSessionName);
		}

		if (!SessionSettings && GameSession)
		{
			GameSession->RegisterServer();
		}
	}
}

void AUTEmptyServerGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
	if (UTGameSession)
	{
		// Destroy the last game if it is still around (comes from restart/travel)
		UTGameSession->CleanupServerSession();
	}
}