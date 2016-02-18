// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGameSession.h"
#include "Online.h"
#include "OnlineSubsystemTypes.h"
#include "UTOnlineGameSettingsBase.h"
#include "UTBaseGameMode.h"
#include "UTLobbyGameMode.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"

AUTGameSession::AUTGameSession(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bSessionValid = false;
	bNoJoinInProgress = false;
	CantBindBeaconPortIsNotFatal = false;
}

void AUTGameSession::Destroyed()
{
	Super::Destroyed();
	CleanUpOnlineSubsystem();
}

void AUTGameSession::CleanUpOnlineSubsystem()
{
	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->ClearOnSessionFailureDelegate_Handle(OnSessionFailuredDelegate);
		}
	}
}

void AUTGameSession::InitOptions( const FString& Options )
{
	bReregisterWhenDone = false;
	Super::InitOptions(Options);

	// Register a connection status change delegate so we can look for failures on the backend.

	const auto OnlineSub = IOnlineSubsystem::Get();
	if (OnlineSub && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		const auto SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			OnSessionFailuredDelegate = SessionInterface->AddOnSessionFailureDelegate_Handle(FOnSessionFailureDelegate::CreateUObject(this, &AUTGameSession::SessionFailure));
		}
	}

	// Cache the GameMode for later.
	UTBaseGameMode = Cast<AUTBaseGameMode>(GetWorld()->GetAuthGameMode());
	bNoJoinInProgress = UGameplayStatics::HasOption(Options, "NoJIP");
}

void AUTGameSession::ValidatePlayer(const FString& Address, const TSharedPtr<const FUniqueNetId>& UniqueId, FString& ErrorMessage, bool bValidateAsSpectator)
{
	if ( !bValidateAsSpectator && (UniqueId.IsValid() && AllowedAdmins.Find(UniqueId->ToString()) == INDEX_NONE) && bNoJoinInProgress && UTBaseGameMode->HasMatchStarted() )
	{
		ErrorMessage = TEXT("CANTJOININPROGRESS");
		return;
	}
	
	if (!UniqueId.IsValid() && Address != TEXT("127.0.0.1") && !FParse::Param(FCommandLine::Get(), TEXT("AllowEveryone")) && Cast<AUTLobbyGameMode>(UTBaseGameMode) == NULL)
	{
		ErrorMessage = TEXT("NOTLOGGEDIN");
	}

	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		if (GameState->IsTempBanned(UniqueId))
		{
			UE_LOG(UT,Log,TEXT("Banned"));
			ErrorMessage = TEXT("BANNED");
			return;
		}
	
	}
}


FString AUTGameSession::ApproveLogin(const FString& Options)
{
	if (UTBaseGameMode)
	{
		if (!UGameplayStatics::HasOption(Options, TEXT("VersionCheck")) && (GetNetMode() != NM_Standalone) && !GetWorld()->IsPlayInEditor())
		{
			UE_LOG(UT, Warning, TEXT("********************************YOU MUST UPDATE TO A NEW VERSION %s"), *Options);
			return TEXT("You must update to a the latest version.  For more information, go to forums.unrealtournament.com");
		}
		// force allow split casting views
		// warning: relies on check in Login() that this is really a split view
		if (UGameplayStatics::HasOption(Options, TEXT("CastingView")))
		{
			return TEXT("");
		}

		// If this is a rank locked server, don't allow players in
		AUTGameMode* UTGameMode = Cast<AUTGameMode>(UTBaseGameMode);
		if (UTGameMode)
		{
			if (UTGameMode->bRankLocked)
			{
				int32 IncomingRank = UGameplayStatics::GetIntOption(Options, TEXT("RankCheck"), 0);

				if (IncomingRank > UTGameMode->RankCheck)
				{
					return TEXT("TOOSTRONG");
				}
			}
		}

		if (GetNetMode() != NM_Standalone && !GetWorld()->IsPlayInEditor())
		{
			bool bSpectator = FCString::Stricmp(*UGameplayStatics::ParseOption(Options, TEXT("SpectatorOnly")), TEXT("1")) == 0;

			FString Password = UGameplayStatics::ParseOption(Options, TEXT("Password"));
			if (!bSpectator && !UTBaseGameMode->ServerPassword.IsEmpty())
			{
				UE_LOG(UT,Log,TEXT("Pass: %s v %s"), Password.IsEmpty() ? TEXT("") : *Password, *UTBaseGameMode->ServerPassword);
				if (Password.IsEmpty() || !UTBaseGameMode->ServerPassword.Equals(Password, ESearchCase::CaseSensitive))
				{
					return TEXT("NEEDPASS");
				}
			}
			FString SpecPassword = UGameplayStatics::ParseOption(Options, TEXT("SpecPassword"));
			if (bSpectator && !UTBaseGameMode->SpectatePassword.IsEmpty())
			{
				UE_LOG(UT,Log,TEXT("Spec: %s v %s"), SpecPassword.IsEmpty() ? TEXT("") : *SpecPassword, *UTBaseGameMode->SpectatePassword);
				if (SpecPassword.IsEmpty() || !UTBaseGameMode->SpectatePassword.Equals(SpecPassword, ESearchCase::CaseSensitive))
				{
					return TEXT("NEEDSPECPASS");
				}
			}
		}
	}
	return Super::ApproveLogin(Options);
}

bool AUTGameSession::ProcessAutoLogin()
{
	// UT Dedicated servers do not need to login.  
	if (GetWorld()->GetNetMode() == NM_DedicatedServer) 
	{
		// NOTE: Returning true here will effectively bypass the RegisterServer call in the base GameMode.  
		// UT servers will register elsewhere.
		return true;
	}
	return Super::ProcessAutoLogin();
}

// We want different behavior than the default engine implementation.  Our matches remain across level switches so don't allow the main game to mess with them.

void AUTGameSession::HandleMatchHasStarted() {}
void AUTGameSession::HandleMatchHasEnded() {}

void AUTGameSession::AcknowledgeAdmin(const FString& AdminId, bool bIsAdmin)
{
	if (bIsAdmin)
	{
		// Make sure he's not already in there.
		if (AllowedAdmins.Find(AdminId) == INDEX_NONE)
		{
			AllowedAdmins.Add(AdminId);
		}
	}
	else
	{
		if (AllowedAdmins.Find(AdminId) != INDEX_NONE)
		{
			AllowedAdmins.Remove(AdminId);
		}
	}
}

void AUTGameSession::SessionFailure(const FUniqueNetId& PlayerId, ESessionFailure::Type ErrorType)
{
	// Currently, SessionFailure never seems to be called.  Need to discuss it with Josh when he returns as it would
	// be better to handle this hear then in ConnectionStatusChanged
}
