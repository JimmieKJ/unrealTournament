// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTMenuGameMode.h"
#include "GameFramework/GameMode.h"
#include "UTGameMode.h"
#include "UTDMGameMode.h"
#include "UTWorldSettings.h"

AUTMenuGameMode::AUTMenuGameMode(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AUTGameState::StaticClass();
	PlayerStateClass = AUTPlayerState::StaticClass();
	PlayerControllerClass = AUTPlayerController::StaticClass();

	static ConstructorHelpers::FObjectFinder<USoundBase> DefaultMusicRef[] = { TEXT("/Game/RestrictedAssets/Audio/Music/Music_UTMenu_Classic.Music_UTMenu_Classic"), TEXT("/Game/RestrictedAssets/Audio/Music/Music_FragCenterIntro.Music_FragCenterIntro") };

	for (int32 i = 0; i < ARRAY_COUNT(DefaultMusicRef); i++)
	{
		DefaultMusics.Add(DefaultMusicRef[i].Object);
	}
}

void AUTMenuGameMode::RestartGame()
{
	return;
}
void AUTMenuGameMode::BeginGame()
{
	return;
}

void AUTMenuGameMode::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	AUTWorldSettings* WorldSettings;
	WorldSettings = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
	if (WorldSettings)
	{
		FString MenuMusicPath = MenuMusicAssetName;
		// We use a second var to make sure we never write this value back out
		
		if ( MenuMusicAssetName != TEXT("") )
		{

			MenuMusic = LoadObject<USoundBase>(NULL, *MenuMusicPath, NULL, LOAD_NoWarn | LOAD_Quiet);
			if (MenuMusic)
			{
				UGameplayStatics::PlaySoundAtLocation( this, MenuMusic, FVector(0,0,0), 1.0, 1.0 );
			}
			
		}
	
	}
	return;
}

void AUTMenuGameMode::GenericPlayerInitialization(AController* C)
{
	Super::GenericPlayerInitialization(C);
	AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(C);
	if (PC != NULL)
	{
		FURL& LastURL = GEngine->GetWorldContextFromWorld(GetWorld())->LastURL;

		PC->ClientReturnedToMenus();
		bool bReturnedFromChallenge = LastURL.HasOption(TEXT("showchallenge"));
		PC->ShowMenu((bReturnedFromChallenge ? TEXT("showchallenge") : TEXT("")));
#if !UE_SERVER
		// start with tutorial menu if requested
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
