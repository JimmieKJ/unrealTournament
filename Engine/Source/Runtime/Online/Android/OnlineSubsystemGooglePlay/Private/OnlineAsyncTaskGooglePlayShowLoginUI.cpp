// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemGooglePlayPrivatePCH.h"
#include "OnlineAsyncTaskGooglePlayShowLoginUI.h"

#include "gpg/player_manager.h"


FOnlineAsyncTaskGooglePlayShowLoginUI::FOnlineAsyncTaskGooglePlayShowLoginUI(
	FOnlineSubsystemGooglePlay* InSubsystem,
	int InPlayerId,
	const FOnLoginUIClosedDelegate& InDelegate)
	: FOnlineAsyncTaskGooglePlayAuthAction(InSubsystem)
	, PlayerId(InPlayerId)
	, Delegate(InDelegate)
{
}

void FOnlineAsyncTaskGooglePlayShowLoginUI::Start_OnTaskThread()
{	
	if(Subsystem->GetGameServices() == nullptr)
	{
		UE_LOG(LogOnline, Log, TEXT("FOnlineAsyncTaskGooglePlayShowLoginUI: GameServicesPtr is null."));
		bWasSuccessful = false;
		bIsComplete = true;
		return;
	}

	if(Subsystem->GetGameServices()->IsAuthorized())
	{
		// User is already authorized, nothing to do.
		bWasSuccessful = true;
		Subsystem->GetGameServices()->Players().FetchSelf([this](gpg::PlayerManager::FetchSelfResponse const &response) { OnFetchSelfResponse(response); });
	}

	// The user isn't authorized, show the sign-in UI.
	Subsystem->GetGameServices()->StartAuthorizationUI();
}

void FOnlineAsyncTaskGooglePlayShowLoginUI::Finalize()
{
	// Async task manager owns the task and is responsible for cleaning it up.
	Subsystem->CurrentShowLoginUITask = nullptr;
}

void FOnlineAsyncTaskGooglePlayShowLoginUI::TriggerDelegates()
{
	TSharedPtr<const FUniqueNetIdString> UserId = Subsystem->GetIdentityGooglePlay()->GetCurrentUserId();

	if (bWasSuccessful && !UserId.IsValid())
	{
		UserId = MakeShareable(new FUniqueNetIdString());
		Subsystem->GetIdentityGooglePlay()->SetCurrentUserId(UserId);
	}
	else if (!bWasSuccessful)
	{
		Subsystem->GetIdentityGooglePlay()->SetCurrentUserId(nullptr);
	}

	Delegate.ExecuteIfBound(UserId, PlayerId);
}

void FOnlineAsyncTaskGooglePlayShowLoginUI::OnAuthActionFinished(gpg::AuthOperation InOp, gpg::AuthStatus InStatus)
{
	if (InOp == gpg::AuthOperation::SIGN_IN)
	{
		bWasSuccessful = InStatus == gpg::AuthStatus::VALID;
		if(bWasSuccessful)
		{
			Subsystem->GetGameServices()->Players().FetchSelf([this](gpg::PlayerManager::FetchSelfResponse const &response) { OnFetchSelfResponse(response); });
		}
		else
		{
			bIsComplete = true;
		}
	}
}

void FOnlineAsyncTaskGooglePlayShowLoginUI::OnFetchSelfResponse(const gpg::PlayerManager::FetchSelfResponse& SelfResponse)
{
	if(gpg::IsSuccess(SelfResponse.status))
	{
		Subsystem->GetIdentityGooglePlay()->SetPlayerDataFromFetchSelfResponse(SelfResponse.data);
	}
	else
	{
		UE_LOG(LogOnline, Warning, TEXT("FetchSelf Response Status Not Successful"));
	}
	bIsComplete = true;
}
