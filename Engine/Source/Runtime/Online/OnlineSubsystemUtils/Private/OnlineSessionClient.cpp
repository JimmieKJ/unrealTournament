// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OnlineSessionClient.cpp: Online session related implementations 
	(creating/joining/leaving/destroying sessions)
=============================================================================*/

#include "OnlineSubsystemUtilsPrivatePCH.h"

#include "Engine/GameInstance.h"

#define INVALID_CONTROLLERID 255

UOnlineSessionClient::UOnlineSessionClient(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHandlingDisconnect = false;
	bIsFromInvite = false;
}

APlayerController* UOnlineSessionClient::GetPlayerController()
{
	ULocalPlayer* LP = Cast<ULocalPlayer>(GetOuter());
	if (LP != NULL)
	{
		return LP->PlayerController;
	}

	return NULL;
}

/**
 * Helper function to retrieve the controller id of the owning controller
 *
 * @return controller id of the controller
 */
int32 UOnlineSessionClient::GetControllerId()
{
	ULocalPlayer* LP = Cast<ULocalPlayer>(GetOuter());
	if (LP != nullptr)
	{
		return LP->GetControllerId();
	}

	return INVALID_CONTROLLERID;
}

/**
 * Register all delegates needed to manage online sessions
 */
void UOnlineSessionClient::RegisterOnlineDelegates(UWorld* InWorld)
{
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(InWorld);
	if (OnlineSub)
	{
		SessionInt = OnlineSub->GetSessionInterface();
		if (SessionInt.IsValid())
		{
			int32 ControllerId = GetControllerId();
			if (ControllerId != INVALID_CONTROLLERID)
			{
				// Always on the lookout for invite acceptance (via actual invite or join from external ui)
				OnSessionInviteAcceptedDelegate = FOnSessionInviteAcceptedDelegate::CreateUObject(this, &UOnlineSessionClient::OnSessionInviteAccepted);
				SessionInt->AddOnSessionInviteAcceptedDelegate(ControllerId, OnSessionInviteAcceptedDelegate);
			}
		}

		OnJoinSessionCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &UOnlineSessionClient::OnJoinSessionComplete);
		OnEndForJoinSessionCompleteDelegate = FOnEndSessionCompleteDelegate::CreateUObject(this, &UOnlineSessionClient::OnEndForJoinSessionComplete);
		OnDestroyForJoinSessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &UOnlineSessionClient::OnDestroyForJoinSessionComplete);
	}

	// Register disconnect delegate even if we don't have an online system
	OnDestroyForMainMenuCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &UOnlineSessionClient::OnDestroyForMainMenuComplete);
}

/**
 * Tear down all delegates used to manage online sessions
 */
void UOnlineSessionClient::ClearOnlineDelegates(UWorld* InWorld)
{
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(InWorld);
	if (OnlineSub)
	{
		if (SessionInt.IsValid())
		{
			int32 ControllerId = GetControllerId();
			if (ControllerId != INVALID_CONTROLLERID)
			{
				SessionInt->ClearOnSessionInviteAcceptedDelegate(ControllerId, OnSessionInviteAcceptedDelegate);
			}
		}
	}

	SessionInt = NULL;
}

/**
 * Delegate fired when an invite request has been accepted (via external UI)
 *
 * @param LocalUserNum local user accepting invite
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 * @param SearchResult search result containing the invite data
 */
void UOnlineSessionClient::OnSessionInviteAccepted(int32 LocalUserNum, bool bWasSuccessful, const FOnlineSessionSearchResult& SearchResult)
{
	UE_LOG(LogOnline, Verbose, TEXT("OnSessionInviteAccepted LocalUserNum: %d bSuccess: %d"), LocalUserNum, bWasSuccessful);
	// Don't clear invite accept delegate

	if (bWasSuccessful)
	{
		if (SearchResult.IsValid())
		{
			bIsFromInvite = true;
			check(GetControllerId() == LocalUserNum);
			JoinSession(LocalUserNum, GameSessionName, SearchResult);
		}
		else
		{
			UE_LOG(LogOnline, Warning, TEXT("Invite accept returned no search result."));
		}
	}
}

/**
 * Transition from ending a session to destroying a session
 *
 * @param SessionName session that just ended
 * @param bWasSuccessful was the end session attempt successful
 */
void UOnlineSessionClient::OnEndForJoinSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogOnline, Verbose, TEXT("OnEndForJoinSessionComplete %s bSuccess: %d"), *SessionName.ToString(), bWasSuccessful);
	SessionInt->ClearOnEndSessionCompleteDelegate(OnEndForJoinSessionCompleteDelegate);
	DestroyExistingSession(SessionName, OnDestroyForJoinSessionCompleteDelegate);
}

/**
 * Ends an existing session of a given name
 *
 * @param SessionName name of session to end
 * @param Delegate delegate to call at session end
 */
void UOnlineSessionClient::EndExistingSession(FName SessionName, FOnEndSessionCompleteDelegate& Delegate)
{
	if (SessionInt.IsValid())
	{
		SessionInt->AddOnEndSessionCompleteDelegate(Delegate);
		SessionInt->EndSession(SessionName);
	}
	else
	{
		Delegate.ExecuteIfBound(SessionName, true);
	}
}

/**
 * Transition from destroying a session to joining a new one of the same name
 *
 * @param SessionName name of session recently destroyed
 * @param bWasSuccessful was the destroy attempt successful
 */
void UOnlineSessionClient::OnDestroyForJoinSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogOnline, Verbose, TEXT("OnDestroyForJoinSessionComplete %s bSuccess: %d"), *SessionName.ToString(), bWasSuccessful);
	if (SessionInt.IsValid())
	{
		SessionInt->ClearOnDestroySessionCompleteDelegate(OnDestroyForJoinSessionCompleteDelegate);
	}

	if (bWasSuccessful)
	{
		int32 ControllerId = GetControllerId();
		if (ControllerId != INVALID_CONTROLLERID)
		{
			JoinSession(ControllerId, SessionName, CachedSessionResult);
		}
	}

	bHandlingDisconnect = false;
}

/**
 * Transition from destroying a session to returning to the main menu
 *
 * @param SessionName name of session recently destroyed
 * @param bWasSuccessful was the destroy attempt successful
 */
void UOnlineSessionClient::OnDestroyForMainMenuComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogOnline, Verbose, TEXT("OnDestroyForMainMenuComplete %s bSuccess: %d"), *SessionName.ToString(), bWasSuccessful);
	if (SessionInt.IsValid())
	{
		SessionInt->ClearOnDestroySessionCompleteDelegate(OnDestroyForMainMenuCompleteDelegate);
	}	

	APlayerController* PC = GetPlayerController();
	if (PC)
	{
		// Call disconnect to force us back to the menu level
		GEngine->HandleDisconnect(PC->GetWorld(), PC->GetWorld()->GetNetDriver());
	}

	bHandlingDisconnect = false;
}

/**
 * Destroys an existing session of a given name
 *
 * @param SessionName name of session to destroy
 * @param Delegate delegate to call at session destruction
 */
void UOnlineSessionClient::DestroyExistingSession(FName SessionName, FOnDestroySessionCompleteDelegate& Delegate)
{
	if (SessionInt.IsValid())
	{
		SessionInt->AddOnDestroySessionCompleteDelegate(Delegate);
		SessionInt->DestroySession(SessionName);
	}
	else
	{
		Delegate.ExecuteIfBound(SessionName, true);
	}
}

/**
 * Delegate fired when the joining process for an online session has completed
 *
 * @param SessionName the name of the session this callback is for
 * @param bWasSuccessful true if the async action completed without error, false if there was an error
 */
void UOnlineSessionClient::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	UE_LOG(LogOnline, Verbose, TEXT("OnJoinSessionComplete %s bSuccess: %d"), *SessionName.ToString(), static_cast<int32>(Result));
	SessionInt->ClearOnJoinSessionCompleteDelegate(OnJoinSessionCompleteDelegate);

	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		FString URL;
		if (SessionInt->GetResolvedConnectString(SessionName, URL))
		{
			APlayerController* PC = GetPlayerController();
			if (PC)
			{
				if (bIsFromInvite)
				{
					URL += TEXT("?bIsFromInvite");
					bIsFromInvite = false;
				}
				PC->ClientTravel(URL, TRAVEL_Absolute);
			}
		}
		else
		{
			UE_LOG(LogOnline, Warning, TEXT("Failed to join session %s"), *SessionName.ToString());
		}
	}
}

/**
 * Join a session of a given name after potentially tearing down an existing one
 *
 * @param LocalUserNum local user id
 * @param SessionName name of session to join
 * @param SearchResult the session to join
 */
void UOnlineSessionClient::JoinSession(int32 LocalUserNum, FName SessionName, const FOnlineSessionSearchResult& SearchResult)
{
	// Clean up existing sessions if applicable
	EOnlineSessionState::Type SessionState = SessionInt->GetSessionState(SessionName);
	if (SessionState != EOnlineSessionState::NoSession)
	{
		CachedSessionResult = SearchResult;
		EndExistingSession(SessionName, OnEndForJoinSessionCompleteDelegate);
	}
	else
	{
		UGameInstance * const GameInstance = GetPlayerController()->GetGameInstance();
		GameInstance->JoinSession(static_cast<ULocalPlayer*>(GetPlayerController()->Player), SearchResult);
		/*SessionInt->AddOnJoinSessionCompleteDelegate(OnJoinSessionCompleteDelegate);
		SessionInt->JoinSession(LocalUserNum, SessionName, SearchResult);*/
	}
}

/**
 * Called to tear down any online sessions and return to main menu
 */
void UOnlineSessionClient::HandleDisconnect(UWorld *World, UNetDriver *NetDriver)
{
	bool bWasHandled = HandleDisconnectInternal(World, NetDriver);
	
	if (!bWasHandled)
	{
		// This may have been a pending net game that failed, let the engine handle it (dont tear our stuff down)
		// (Would it be better to return true/false based on if we handled the disconnect or not? Let calling code call GEngine stuff
		GEngine->HandleDisconnect(World, NetDriver);
	}
}

bool UOnlineSessionClient::HandleDisconnectInternal(UWorld* World, UNetDriver* NetDriver)
{
	APlayerController* PC = GetPlayerController();
	if (PC)
	{
		// This was a disconnect for our active world, we will handle it
		if (PC->GetWorld() == World)
		{
			// Prevent multiple calls to this async flow
			if (!bHandlingDisconnect)
			{
				bHandlingDisconnect = true;
				DestroyExistingSession(GameSessionName, OnDestroyForMainMenuCompleteDelegate);
			}

			return true;
		}
	}

	return false;
}