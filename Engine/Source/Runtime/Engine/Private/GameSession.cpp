// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GameSession.cpp: GameSession code.
=============================================================================*/

#include "EnginePrivate.h"
#include "Net/UnrealNetwork.h"
#include "OnlineSubsystemUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/GameMode.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameSession, Log, All);

static TAutoConsoleVariable<int32> CVarMaxPlayersOverride( TEXT( "net.MaxPlayersOverride" ), 0, TEXT( "If greater than 0, will override the standard max players count. Useful for testing full servers." ) );

/** 
 * Returns the player controller associated with this net id
 * @param PlayerNetId the id to search for
 * @return the player controller if found, otherwise NULL
 */
APlayerController* GetPlayerControllerFromNetId(UWorld* World, const FUniqueNetId& PlayerNetId)
{
	// Iterate through the controller list looking for the net id
	for(FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = *Iterator;
		// Determine if this is a player with replication
		if (PlayerController->PlayerState != NULL)
		{
			// If the ids match, then this is the right player.
			if (*PlayerController->PlayerState->UniqueId == PlayerNetId)
			{
				return PlayerController;
			}
		}
	}
	return NULL;
}

AGameSession::AGameSession(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AGameSession::HandleMatchIsWaitingToStart()
{
}

void AGameSession::HandleMatchHasStarted()
{
	UWorld* World = GetWorld();
	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(World);
	if (SessionInt.IsValid())
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PlayerController = *Iterator;
			if (!PlayerController->IsLocalController())
			{
				PlayerController->ClientStartOnlineSession();
			}
		}

		StartSessionCompleteHandle = SessionInt->AddOnStartSessionCompleteDelegate_Handle(FOnStartSessionCompleteDelegate::CreateUObject(this, &AGameSession::OnStartSessionComplete));
		SessionInt->StartSession(SessionName);
	}
}

void AGameSession::OnStartSessionComplete(FName InSessionName, bool bWasSuccessful)
{
	UE_LOG(LogGameSession, Verbose, TEXT("OnStartSessionComplete %s bSuccess: %d"), *InSessionName.ToString(), bWasSuccessful);
	UWorld* World = GetWorld();
	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(World);
	if (SessionInt.IsValid())
	{
		SessionInt->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteHandle);
	}
}

void AGameSession::HandleMatchHasEnded()
{
	UWorld* World = GetWorld();
	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(World);
	if (SessionInt.IsValid())
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PlayerController = *Iterator;
			if (!PlayerController->IsLocalController())
			{
				PlayerController->ClientEndOnlineSession();
			}
		}

		SessionInt->AddOnEndSessionCompleteDelegate_Handle(FOnEndSessionCompleteDelegate::CreateUObject(this, &AGameSession::OnEndSessionComplete));
		SessionInt->EndSession(SessionName);
	}
}

void AGameSession::OnEndSessionComplete(FName InSessionName, bool bWasSuccessful)
{
	UE_LOG(LogGameSession, Verbose, TEXT("OnEndSessionComplete %s bSuccess: %d"), *InSessionName.ToString(), bWasSuccessful);
	UWorld* World = GetWorld();
	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(World);
	if (SessionInt.IsValid())
	{
		SessionInt->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteHandle);
	}
}

bool AGameSession::HandleStartMatchRequest()
{
	return false;
}

void AGameSession::InitOptions( const FString& Options )
{
	UWorld* World = GetWorld();
	check(World);
	AGameMode* const GameMode = World->GetAuthGameMode();

	MaxPlayers = GameMode->GetIntOption( Options, TEXT("MaxPlayers"), MaxPlayers );
	MaxSpectators = GameMode->GetIntOption( Options, TEXT("MaxSpectators"), MaxSpectators );
	SessionName = GetDefault<APlayerState>(GameMode->PlayerStateClass)->SessionName;
}

bool AGameSession::ProcessAutoLogin()
{
	UWorld* World = GetWorld();
	IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface(World);
	if (IdentityInt.IsValid())
	{
		OnLoginCompleteDelegateHandle = IdentityInt->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &AGameSession::OnLoginComplete));
		if (!IdentityInt->AutoLogin(0))
		{
			// Not waiting for async login
			return false;
		}

		return true;
	}

	// Not waiting for async login
	return false;
}

void AGameSession::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	UWorld* World = GetWorld();
	IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface(World);
	if (IdentityInt.IsValid())
	{
		IdentityInt->ClearOnLoginCompleteDelegate_Handle(0, OnLoginCompleteDelegateHandle);
		if (IdentityInt->GetLoginStatus(0) == ELoginStatus::LoggedIn)
		{
			RegisterServer();
		}
		else
		{
			UE_LOG(LogGameSession, Warning, TEXT("Autologin attempt failed, unable to register server!"));
		}
	}
}

void AGameSession::RegisterServer()
{
}

FString AGameSession::ApproveLogin(const FString& Options)
{
	UWorld* const World = GetWorld();
	check(World);

	AGameMode* const GameMode = World->GetAuthGameMode();
	check(GameMode);

	int32 SpectatorOnly = 0;
	SpectatorOnly = GameMode->GetIntOption(Options, TEXT("SpectatorOnly"), SpectatorOnly);

	if (AtCapacity(SpectatorOnly == 1))
	{
		return NSLOCTEXT("NetworkErrors", "ServerAtCapacity", "Server full.").ToString();
	}

	int32 SplitscreenCount = 0;
	SplitscreenCount = GameMode->GetIntOption(Options, TEXT("SplitscreenCount"), SplitscreenCount);

	if (SplitscreenCount > MaxSplitscreensPerConnection)
	{
		return FText::Format(NSLOCTEXT("NetworkErrors", "ServerAtCapacitySS", "A maximum of '{0}' splitscreen players are allowed"), FText::AsNumber(MaxSplitscreensPerConnection)).ToString();
	}

	return TEXT("");
}

void AGameSession::PostLogin(APlayerController* NewPlayer)
{
}

/** @return A new unique player ID */
int32 AGameSession::GetNextPlayerID()
{
	// Start at 256, because 255 is special (means all team for some UT Emote stuff)
	static int32 NextPlayerID = 256;
	return NextPlayerID++;
}

/**
 * Register a player with the online service session
 * 
 * @param NewPlayer player to register
 * @param UniqueId uniqueId they sent over on Login
 * @param bWasFromInvite was this from an invite
 */
void AGameSession::RegisterPlayer(APlayerController* NewPlayer, const TSharedPtr<FUniqueNetId>& UniqueId, bool bWasFromInvite)
{
	if (NewPlayer != NULL)
	{
		// Set the player's ID.
		check(NewPlayer->PlayerState);
		NewPlayer->PlayerState->PlayerId = GetNextPlayerID();
		NewPlayer->PlayerState->SetUniqueId(UniqueId);
		NewPlayer->PlayerState->RegisterPlayerWithSession(bWasFromInvite);
	}
}

/**
 * Unregister a player from the online service session
 */
void AGameSession::UnregisterPlayer(APlayerController* ExitingPlayer)
{
	UWorld* World = GetWorld();
	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(World);
	if (SessionInt.IsValid())
	{
		if (GetNetMode() != NM_Standalone && 
			ExitingPlayer != NULL &&
			ExitingPlayer->PlayerState && 
			ExitingPlayer->PlayerState->UniqueId.IsValid() &&
			ExitingPlayer->PlayerState->UniqueId->IsValid())
		{
			// Remove the player from the session
			SessionInt->UnregisterPlayer(ExitingPlayer->PlayerState->SessionName, *ExitingPlayer->PlayerState->UniqueId);
		}
	}
}

bool AGameSession::AtCapacity(bool bSpectator)
{
	if ( GetNetMode() == NM_Standalone )
	{
		return false;
	}

	if ( bSpectator )
	{
		return ( (GetWorld()->GetAuthGameMode()->NumSpectators >= MaxSpectators)
		&& ((GetNetMode() != NM_ListenServer) || (GetWorld()->GetAuthGameMode()->NumPlayers > 0)) );
	}
	else
	{
		const int32 MaxPlayersToUse = CVarMaxPlayersOverride.GetValueOnGameThread() > 0 ? CVarMaxPlayersOverride.GetValueOnGameThread() : MaxPlayers;

		return ( (MaxPlayersToUse>0) && (GetWorld()->GetAuthGameMode()->GetNumPlayers() >= MaxPlayersToUse) );
	}
}

void AGameSession::NotifyLogout(APlayerController* PC)
{
	// Unregister the player from the online layer
	UnregisterPlayer(PC);
}

void AGameSession::AddAdmin(APlayerController* AdminPlayer)
{
}

void AGameSession::RemoveAdmin(APlayerController* AdminPlayer)
{
}

bool AGameSession::KickPlayer(APlayerController* KickedPlayer, const FText& KickReason)
{
	// Do not kick logged admins
	if (KickedPlayer != NULL && Cast<UNetConnection>(KickedPlayer->Player) != NULL)
	{
		if (KickedPlayer->GetPawn() != NULL)
		{
			KickedPlayer->GetPawn()->Destroy();
		}

		KickedPlayer->ClientWasKicked(KickReason);

		if (KickedPlayer != NULL)
		{
			KickedPlayer->Destroy();
		}

		return true;
	}
	return false;
}

bool AGameSession::BanPlayer(class APlayerController* BannedPlayer, const FText& BanReason)
{
	return KickPlayer(BannedPlayer, BanReason);
}

void AGameSession::ReturnToMainMenuHost()
{
	FString RemoteReturnReason = NSLOCTEXT("NetworkErrors", "HostHasLeft", "Host has left the game.").ToString();
	FString LocalReturnReason(TEXT(""));

	APlayerController* Controller = NULL;
	FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator();
	for(; Iterator; ++Iterator)
	{
		Controller = *Iterator;
		if (Controller && !Controller->IsLocalPlayerController() && Controller->IsPrimaryPlayer())
		{
			// Clients
			Controller->ClientReturnToMainMenu(RemoteReturnReason);
		}
	}

	Iterator.Reset();
	for(; Iterator; ++Iterator)
	{
		Controller = *Iterator;
		if (Controller && Controller->IsLocalPlayerController() && Controller->IsPrimaryPlayer())
		{
			Controller->ClientReturnToMainMenu(LocalReturnReason);
			break;
		}
	}
}

bool AGameSession::TravelToSession(int32 ControllerId, FName InSessionName)
{
	UWorld* World = GetWorld();
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(World);
	if (OnlineSub)
	{
		FString URL;
		IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
		if (SessionInt.IsValid() && SessionInt->GetResolvedConnectString(InSessionName, URL))
		{
			APlayerController* PC = UGameplayStatics::GetPlayerController(World, ControllerId);
			if (PC)
			{
				PC->ClientTravel(URL, TRAVEL_Absolute);
				return true;
			}
		}
		else
		{
			UE_LOG(LogGameSession, Warning, TEXT("Failed to resolve session connect string for %s"), *InSessionName.ToString());
		}
	}

	return false;
}

void AGameSession::PostSeamlessTravel()
{
}

void AGameSession::DumpSessionState()
{
	UE_LOG(LogGameSession, Log, TEXT("  MaxPlayers: %i"), MaxPlayers);
	UE_LOG(LogGameSession, Log, TEXT("  MaxSpectators: %i"), MaxSpectators);

	IOnlineSessionPtr SessionInt = Online::GetSessionInterface(GetWorld());
	if (SessionInt.IsValid())
	{
		SessionInt->DumpSessionState();
	}
}

bool AGameSession::CanRestartGame()
{
	return true;
}

void AGameSession::UpdateSessionJoinability(FName InSessionName, bool bPublicSearchable, bool bAllowInvites, bool bJoinViaPresence, bool bJoinViaPresenceFriendsOnly)
{
	if (GetNetMode() != NM_Standalone)
	{
		IOnlineSessionPtr SessionInt = Online::GetSessionInterface(GetWorld());
		if (SessionInt.IsValid())
		{
			FOnlineSessionSettings* GameSettings = SessionInt->GetSessionSettings(InSessionName);
			if (GameSettings != NULL)
			{
				GameSettings->bShouldAdvertise = bPublicSearchable;
				GameSettings->bAllowInvites = bAllowInvites;
				GameSettings->bAllowJoinViaPresence = bJoinViaPresence && !bJoinViaPresenceFriendsOnly;
				GameSettings->bAllowJoinViaPresenceFriendsOnly = bJoinViaPresenceFriendsOnly;
				SessionInt->UpdateSession(InSessionName, *GameSettings, true);
			}
		}
	}
}
