// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OnlineSessionClient.cpp: Online session related implementations 
	(creating/joining/leaving/destroying sessions)
=============================================================================*/

#include "OnlineSubsystemUtilsPrivatePCH.h"

#include "Engine/GameInstance.h"

UOnlineSessionClient::UOnlineSessionClient(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHandlingDisconnect = false;
	bIsFromInvite = false;
}

UWorld* UOnlineSessionClient::GetWorld() const
{
	ULocalPlayer* LP = Cast<ULocalPlayer>(GetOuter());
	check(LP);
	return LP->GetWorld();
}

APlayerController* UOnlineSessionClient::GetPlayerController()
{
	ULocalPlayer* LP = Cast<ULocalPlayer>(GetOuter());
	if (LP != nullptr)
	{
		return LP->PlayerController;
	}

	return nullptr;
}

int32 UOnlineSessionClient::GetControllerId()
{
	ULocalPlayer* LP = Cast<ULocalPlayer>(GetOuter());
	if (LP != nullptr)
	{
		return LP->GetControllerId();
	}

	return INVALID_CONTROLLERID;
}

TSharedPtr<FUniqueNetId> UOnlineSessionClient::GetUniqueId()
{
	ULocalPlayer* LP = Cast<ULocalPlayer>(GetOuter());
	if (LP != nullptr)
	{
		return LP->GetPreferredUniqueNetId();
	}

	return nullptr;
}

void UOnlineSessionClient::RegisterOnlineDelegates(UWorld* InWorld)
{
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(InWorld);
	if (OnlineSub)
	{
		OnJoinSessionCompleteDelegate           = FOnJoinSessionCompleteDelegate   ::CreateUObject(this, &UOnlineSessionClient::OnJoinSessionComplete);
		OnEndForJoinSessionCompleteDelegate     = FOnEndSessionCompleteDelegate    ::CreateUObject(this, &UOnlineSessionClient::OnEndForJoinSessionComplete);
		OnDestroyForJoinSessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &UOnlineSessionClient::OnDestroyForJoinSessionComplete);
	}

	// Register disconnect delegate even if we don't have an online system
	OnDestroyForMainMenuCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &UOnlineSessionClient::OnDestroyForMainMenuComplete);
}

void UOnlineSessionClient::ClearOnlineDelegates(UWorld* InWorld)
{
	SessionInt = NULL;
}

void UOnlineSessionClient::OnSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId, TSharedPtr<FUniqueNetId> UserId, const FOnlineSessionSearchResult& SearchResult)
{
	UE_LOG(LogOnline, Verbose, TEXT("OnSessionInviteAccepted LocalUserNum: %d bSuccess: %d"), ControllerId, bWasSuccessful);
	// Don't clear invite accept delegate

	if (bWasSuccessful)
	{
		if (SearchResult.IsValid())
		{
			bIsFromInvite = true;
			JoinSession(GameSessionName, SearchResult);
		}
		else
		{
			UE_LOG(LogOnline, Warning, TEXT("Invite accept returned no search result."));
		}
	}
}

void UOnlineSessionClient::OnEndForJoinSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogOnline, Verbose, TEXT("OnEndForJoinSessionComplete %s bSuccess: %d"), *SessionName.ToString(), bWasSuccessful);
	SessionInt->ClearOnEndSessionCompleteDelegate_Handle(OnEndForJoinSessionCompleteDelegateHandle);
	DestroyExistingSession_Impl(OnDestroyForJoinSessionCompleteDelegateHandle, SessionName, OnDestroyForJoinSessionCompleteDelegate);
}

void UOnlineSessionClient::EndExistingSession(FName SessionName, FOnEndSessionCompleteDelegate& Delegate)
{
	EndExistingSession_Impl(SessionName, Delegate);
}

/**
 * Implementation of EndExistingSession
 *
 * @param SessionName name of session to end
 * @param Delegate delegate to call at session end
 * @return Handle to the added delegate.
 */
FDelegateHandle UOnlineSessionClient::EndExistingSession_Impl(FName SessionName, FOnEndSessionCompleteDelegate& Delegate)
{
	FDelegateHandle Result;

	if (SessionInt.IsValid())
	{
		Result = SessionInt->AddOnEndSessionCompleteDelegate_Handle(Delegate);
		SessionInt->EndSession(SessionName);
	}
	else
	{
		Delegate.ExecuteIfBound(SessionName, true);
	}

	return Result;
}

void UOnlineSessionClient::OnDestroyForJoinSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogOnline, Verbose, TEXT("OnDestroyForJoinSessionComplete %s bSuccess: %d"), *SessionName.ToString(), bWasSuccessful);
	if (SessionInt.IsValid())
	{
		SessionInt->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroyForJoinSessionCompleteDelegateHandle);
	}

	if (bWasSuccessful)
	{
		JoinSession(SessionName, CachedSessionResult);
	}

	bHandlingDisconnect = false;
}

void UOnlineSessionClient::OnDestroyForMainMenuComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogOnline, Verbose, TEXT("OnDestroyForMainMenuComplete %s bSuccess: %d"), *SessionName.ToString(), bWasSuccessful);
	if (SessionInt.IsValid())
	{
		SessionInt->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroyForMainMenuCompleteDelegateHandle);
	}	

	// Call disconnect to force us back to the menu level
	GEngine->HandleDisconnect(GetWorld(), GetWorld()->GetNetDriver());

	bHandlingDisconnect = false;
}

void UOnlineSessionClient::DestroyExistingSession(FName SessionName, FOnDestroySessionCompleteDelegate& Delegate)
{
	FDelegateHandle UnusedHandle;
	DestroyExistingSession_Impl(UnusedHandle, SessionName, Delegate);
}

/**
 * Implementation of DestroyExistingSession
 *
 * @param OutResult Handle to the added delegate.
 * @param SessionName name of session to destroy
 * @param Delegate delegate to call at session destruction
 */
void UOnlineSessionClient::DestroyExistingSession_Impl(FDelegateHandle& OutResult, FName SessionName, FOnDestroySessionCompleteDelegate& Delegate)
{
	if (SessionInt.IsValid())
	{
		OutResult = SessionInt->AddOnDestroySessionCompleteDelegate_Handle(Delegate);
		SessionInt->DestroySession(SessionName);
	}
	else
	{
		OutResult.Reset();
		Delegate.ExecuteIfBound(SessionName, true);
	}
}

void UOnlineSessionClient::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	UE_LOG(LogOnline, Verbose, TEXT("OnJoinSessionComplete %s bSuccess: %d"), *SessionName.ToString(), static_cast<int32>(Result));
	SessionInt->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegateHandle);

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

void UOnlineSessionClient::JoinSession(FName SessionName, const FOnlineSessionSearchResult& SearchResult)
{
	// Clean up existing sessions if applicable
	EOnlineSessionState::Type SessionState = SessionInt->GetSessionState(SessionName);
	if (SessionState != EOnlineSessionState::NoSession)
	{
		CachedSessionResult = SearchResult;
		OnEndForJoinSessionCompleteDelegateHandle = EndExistingSession_Impl(SessionName, OnEndForJoinSessionCompleteDelegate);
	}
	else
	{
		ULocalPlayer* LocalPlayer = static_cast<ULocalPlayer*>(GetOuter());
		check(LocalPlayer);
		UGameInstance * const GameInstance = LocalPlayer->GetGameInstance();
		check(GameInstance);

		GameInstance->JoinSession(LocalPlayer, SearchResult);
	}
}

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
	// This was a disconnect for our active world, we will handle it
	if (GetWorld() == World)
	{
		// Prevent multiple calls to this async flow
		if (!bHandlingDisconnect)
		{
			bHandlingDisconnect = true;
			DestroyExistingSession_Impl(OnDestroyForMainMenuCompleteDelegateHandle, GameSessionName, OnDestroyForMainMenuCompleteDelegate);
		}

		return true;
	}

	return false;
}

void UOnlineSessionClient::StartOnlineSession(FName SessionName)
{
	IOnlineSessionPtr SessionInterface = Online::GetSessionInterface(GetWorld());
	if (SessionInterface.IsValid())
	{
		FNamedOnlineSession* Session = SessionInterface->GetNamedSession(SessionName);
		if (Session &&
			(Session->SessionState == EOnlineSessionState::Pending || Session->SessionState == EOnlineSessionState::Ended))
		{
			StartSessionCompleteHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(FOnStartSessionCompleteDelegate::CreateUObject(this, &UOnlineSessionClient::OnStartSessionComplete));
			SessionInterface->StartSession(SessionName);
		}
	}
}

void UOnlineSessionClient::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogOnline, Verbose, TEXT("OnStartSessionComplete %s bSuccess: %d"), *SessionName.ToString(), bWasSuccessful);

	IOnlineSessionPtr SessionInterface = Online::GetSessionInterface(GetWorld());
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteHandle);
	}
}

void UOnlineSessionClient::EndOnlineSession(FName SessionName)
{
	IOnlineSessionPtr SessionInterface = Online::GetSessionInterface(GetWorld());
	if (SessionInterface.IsValid())
	{
		FNamedOnlineSession* Session = SessionInterface->GetNamedSession(SessionName);
		if (Session &&
			Session->SessionState == EOnlineSessionState::InProgress)
		{
			EndSessionCompleteHandle = SessionInterface->AddOnEndSessionCompleteDelegate_Handle(FOnStartSessionCompleteDelegate::CreateUObject(this, &UOnlineSessionClient::OnEndSessionComplete));
			SessionInterface->EndSession(SessionName);
		}
	}
}

void UOnlineSessionClient::OnEndSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogOnline, Verbose, TEXT("OnEndSessionComplete %s bSuccess: %d"), *SessionName.ToString(), bWasSuccessful);

	IOnlineSessionPtr SessionInterface = Online::GetSessionInterface(GetWorld());
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteHandle);
	}
}