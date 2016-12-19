// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTMenuGameMode.h"
#include "GameFramework/GameMode.h"
#include "UTGameMode.h"
#include "UTDMGameMode.h"
#include "UTWorldSettings.h"
#include "UTProfileSettings.h"
#include "UTAnalytics.h"

AUTMenuGameMode::AUTMenuGameMode(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AUTGameState::StaticClass();
	PlayerStateClass = AUTPlayerState::StaticClass();
	PlayerControllerClass = AUTPlayerController::StaticClass();

	static ConstructorHelpers::FObjectFinder<USoundBase> DefaultMusicRef[] = 
		{ 
				TEXT("/Game/RestrictedAssets/Audio/Music/Music_UTMuenu_New01.Music_UTMuenu_New01"), 
				TEXT("/Game/RestrictedAssets/Audio/Music/Music_FragCenterIntro.Music_FragCenterIntro") 
		};
	

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
				UGameplayStatics::SpawnSoundAtLocation( this, MenuMusic, FVector(0,0,0), FRotator::ZeroRotator, 1.0, 1.0 );
			}
			
		}
	
	}
	return;
}

void AUTMenuGameMode::OnLoadingMovieEnd()
{
	Super::OnLoadingMovieEnd();

	AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC != NULL)
	{
		PC->ClientReturnedToMenus();
		UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(PC->Player);
		if (LP != NULL)
		{
			if (LP->LoginPhase == ELoginPhase::Offline || LP->LoginPhase == ELoginPhase::LoggedIn)
			{
				ShowMenu(PC);
			}
			else
			{
				LP->GetAuth();
			}
		}
	}
}

void AUTMenuGameMode::ShowMenu(AUTBasePlayerController* PC)
{
	if (PC != NULL)
	{
		PC->ClientReturnedToMenus();
		FURL& LastURL = GEngine->GetWorldContextFromWorld(GetWorld())->LastURL;

		bool bReturnedFromChallenge = LastURL.HasOption(TEXT("showchallenge"));
		PC->ShowMenu((bReturnedFromChallenge ? TEXT("showchallenge") : TEXT("")));

		UUTProfileSettings* ProfileSettings = PC->GetProfileSettings();
		bool bForceTutorialMenu = false;

		if (!PC->SkipTutorialCheck())
		{
			if (ProfileSettings != nullptr && !FParse::Param(FCommandLine::Get(), TEXT("skiptutcheck")) )
			{
				if (ProfileSettings->TutorialMask == 0 )
				{
					if ( !LastURL.HasOption(TEXT("tutorialmenu")) )
					{
	#if !PLATFORM_LINUX
						if (FUTAnalytics::IsAvailable())
						{
							FUTAnalytics::FireEvent_UTTutorialStarted(Cast<AUTPlayerController>(PC),FString("Onboarding"));
						}

						GetWorld()->ServerTravel(TEXT("/Game/RestrictedAssets/Tutorials/TUT-MovementTraining?game=/Game/RestrictedAssets/Tutorials/Blueprints/UTTutorialGameMode.UTTutorialGameMode_C?timelimit=0?TutorialMask=1"), true, true);
						return;
	#endif
					}
				}
				else if ( ((ProfileSettings->TutorialMask & 0x07) != 0x07)  || (ProfileSettings->TutorialMask < 8) )
				{
					bForceTutorialMenu = true;
				}
			}
		}

#if !UE_SERVER
		// start with tutorial menu if requested
		if (bForceTutorialMenu || LastURL.HasOption(TEXT("tutorialmenu")))
		{
			UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(PC->Player);
			// NOTE: If we are in a party, never return to the tutorial menu
			if (LP != NULL && !LP->IsInAnActiveParty())
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
