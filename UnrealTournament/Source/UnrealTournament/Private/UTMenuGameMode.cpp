// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTMenuGameMode.h"
#include "GameFramework/GameMode.h"
#include "UTGameMode.h"
#include "UTDMGameMode.h"

AUTMenuGameMode::AUTMenuGameMode(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AUTGameState::StaticClass();
	PlayerStateClass = AUTPlayerState::StaticClass();
	PlayerControllerClass = AUTPlayerController::StaticClass();
}


void AUTMenuGameMode::RestartGame()
{
	return;
}
void AUTMenuGameMode::BeginGame()
{
	return;
}

void AUTMenuGameMode::GenericPlayerInitialization(AController* C)
{
	Super::GenericPlayerInitialization(C);
	AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(C);
	if (PC != NULL)
	{
		PC->ClientReturnedToMenus();
		PC->ShowMenu();
#if !UE_SERVER
		// start with tutorial menu if requested
		FURL& LastURL = GEngine->GetWorldContextFromWorld(GetWorld())->LastURL;
		if (LastURL.HasOption(TEXT("tutorialmenu")))
		{
			UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(PC->Player);
			if (LP != NULL)
			{
				LP->OpenTutorialMenu();
			}
			// make sure this doesn't get kept around
			LastURL.RemoveOption(TEXT("tutorialmenu"));
		}
#endif
	}
}

void AUTMenuGameMode::RestartPlayer(AController* aPlayer)
{
	return;
}

TSubclassOf<AGameMode> AUTMenuGameMode::SetGameMode(const FString& MapName, const FString& Options, const FString& Portal)
{
	// note that map prefixes are handled by the engine code so we don't need to do that here
	// TODO: mod handling?
	return (MapName == TEXT("UT-Entry")) ? GetClass() : AUTDMGameMode::StaticClass();
}

void AUTMenuGameMode::Logout( AController* Exiting )
{
	Super::Logout(Exiting);

	AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(Exiting);
	if (PC != NULL)
	{
		PC->HideMenu();
	}
}
