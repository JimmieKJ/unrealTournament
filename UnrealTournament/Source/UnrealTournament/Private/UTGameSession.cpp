// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGameSession.h"
#include "Online.h"
#include "OnlineSubsystemTypes.h"
#include "UTOnlineGameSettingsBase.h"
#include "UTGameMode.h"


AUTGameSession::AUTGameSession(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
}

void AUTGameSession::Destroyed()
{
	Super::Destroyed();
	CleanUpOnlineSubsystem();
}

void AUTGameSession::InitOptions( const FString& Options )
{
	Super::InitOptions(Options);

	// Cache the GameMode for later.
	UTGameMode = Cast<AUTGameMode>(GetWorld()->GetAuthGameMode());
}



FString AUTGameSession::ApproveLogin(const FString& Options)
{
	AUTGameMode* GameMode = Cast<AUTGameMode>(GetWorld()->GetAuthGameMode());

	if (GameMode)
	{
		UE_LOG(UT, Log, TEXT("ApproveLogin: %s"), *Options);

		if (!GameMode->HasOption(Options, TEXT("VersionCheck")) && (GetNetMode() != NM_Standalone) && !GetWorld()->IsPlayInEditor())
		{
			UE_LOG(UT, Warning, TEXT("********************************YOU MUST UPDATE TO A NEW VERSION %s"), *Options);
			return TEXT("You must update to a the latest version.  For more information, go to forums.unrealtournament.com");
		}
		if (GameMode->bRequirePassword && !GameMode->ServerPassword.IsEmpty())
		{
			FString Password = GameMode->ParseOption(Options, TEXT("Password"));
			if (Password.IsEmpty() || Password != GameMode->ServerPassword)
			{
				return TEXT("NEEDPASS");
			}
		}
	}

	return Super::ApproveLogin(Options);
}


void AUTGameSession::CleanUpOnlineSubsystem()
{
	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegate(OnCreateSessionCompleteDelegate);
			SessionInterface->ClearOnStartSessionCompleteDelegate(OnStartSessionCompleteDelegate);
			SessionInterface->ClearOnEndSessionCompleteDelegate(OnEndSessionCompleteDelegate);
			SessionInterface->ClearOnDestroySessionCompleteDelegate(OnDestroySessionCompleteDelegate);
		}

	}
}

bool AUTGameSession::ProcessAutoLogin()
{
	if (GetWorld()->GetNetMode() == NM_DedicatedServer) 
	{
		//RegisterServer();
		return true;
	}
	return Super::ProcessAutoLogin();
}


void AUTGameSession::RegisterServer()
{

	UE_LOG(UT,Log,TEXT("--------------[REGISTER SERVER] ----------------"));
	UE_LOG(UT,Log,TEXT("--------------[REGISTER SERVER] ----------------"));

	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			OnCreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &AUTGameSession::OnCreateSessionComplete);
			SessionInterface->AddOnCreateSessionCompleteDelegate(OnCreateSessionCompleteDelegate);

			TSharedPtr<class FUTOnlineGameSettingsBase> OnlineGameSettings = MakeShareable(new FUTOnlineGameSettingsBase(false, false, 32));
			if (OnlineGameSettings.IsValid() && UTGameMode)
			{
				OnlineGameSettings->ApplyGameSettings(UTGameMode);
				SessionInterface->CreateSession(0, GameSessionName, *OnlineGameSettings);
				return;
			}
		}
	}
	UE_LOG(UT,Log,TEXT("Couldn't register the server."))
}

void AUTGameSession::UnRegisterServer()
{
	UE_LOG(UT,Log,TEXT("--------------[UNREGISTER SERVER] ----------------"));
	UE_LOG(UT,Log,TEXT("--------------[UNREGISTER SERVER] ----------------"));


	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			OnDestroySessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &AUTGameSession::OnDestroySessionComplete);
			SessionInterface->AddOnDestroySessionCompleteDelegate(OnDestroySessionCompleteDelegate);
			SessionInterface->DestroySession(GameSessionName);
		}
	}
}

void AUTGameSession::StartMatch()
{

	UE_LOG(UT,Log,TEXT("--------------[START MATCH] ----------------"));
	UE_LOG(UT,Log,TEXT("--------------[START MATCH] ----------------"));

	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			OnStartSessionCompleteDelegate = FOnStartSessionCompleteDelegate::CreateUObject(this, &AUTGameSession::OnStartSessionComplete);
			SessionInterface->AddOnStartSessionCompleteDelegate(OnStartSessionCompleteDelegate);
			SessionInterface->StartSession(GameSessionName);
		}
	}
}

void AUTGameSession::EndMatch()
{
	UE_LOG(UT,Log,TEXT("--------------[END MATCH] ----------------"));
	UE_LOG(UT,Log,TEXT("--------------[END MATCH] ----------------"));

	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			OnEndSessionCompleteDelegate = FOnEndSessionCompleteDelegate::CreateUObject(this, &AUTGameSession::OnEndSessionComplete);
			SessionInterface->AddOnEndSessionCompleteDelegate(OnEndSessionCompleteDelegate);
			SessionInterface->EndSession(GameSessionName);
		}
	}
	
}


void AUTGameSession::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	// If we were not successful, then clear the online game settings member and move on
	if (!bWasSuccessful)
	{
		UE_LOG(UT,Log,TEXT("Failed to Create the session '%s' so this match will not be visible.  See the logs!"), *SessionName.ToString());
	}

	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegate(OnCreateSessionCompleteDelegate);
		}
	}

}

void AUTGameSession::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		UE_LOG(UT,Log,TEXT("Failed to start the session '%s' so this match will not be visible.  See the logs!"), *SessionName.ToString());
	}

	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnStartSessionCompleteDelegate(OnStartSessionCompleteDelegate);
			UpdateGameState();		// Immediately perform an update so as to pickup any players that have joined since.
		}
	}


}

void AUTGameSession::OnEndSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		UE_LOG(UT,Log,TEXT("Failed to end the session '%s' so match stats will not save.  See the logs!"), *SessionName.ToString());
	}

	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnEndSessionCompleteDelegate(OnEndSessionCompleteDelegate);
		}
	}
}

void AUTGameSession::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bWasSuccessful)
	{
		UE_LOG(UT,Log,TEXT("Failed to destroy the session '%s'!  Matchmaking will be broken until restart.  See the logs!"), *SessionName.ToString());
	}

	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate(OnDestroySessionCompleteDelegate);
		}
	}
}
void AUTGameSession::UpdateGameState()
{
	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			EOnlineSessionState::Type State = SessionInterface->GetSessionState(GameSessionName);
			if (State != EOnlineSessionState::Creating && State != EOnlineSessionState::Ended && State != EOnlineSessionState::Ending && State != EOnlineSessionState::Destroying && State != EOnlineSessionState::NoSession )
			{
				AUTGameMode* CurrentGame = Cast<AUTGameMode>(GetWorld()->GetAuthGameMode());
				FUTOnlineGameSettingsBase* OGS = (FUTOnlineGameSettingsBase*)SessionInterface->GetSessionSettings(GameSessionName);

				OGS->Set(SETTING_PLAYERSONLINE, CurrentGame->NumPlayers, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
				OGS->Set(SETTING_SPECTATORSONLINE, CurrentGame->NumSpectators, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

				SessionInterface->UpdateSession(SessionName, *OGS, true);
			}
		}
	}
}

