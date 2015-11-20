// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemGooglePlayPrivatePCH.h"
#include "OnlineAsyncTaskGooglePlayShowLoginUI.h"

FOnlineAsyncTaskGooglePlayShowLoginUI::FOnlineAsyncTaskGooglePlayShowLoginUI(
	FOnlineSubsystemGooglePlay* InSubsystem,
	int InPlayerId,
	const IOnlineExternalUI::FOnLoginUIClosedDelegate& InDelegate)
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
		bIsComplete = true;
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
		bIsComplete = true;
	}
}
