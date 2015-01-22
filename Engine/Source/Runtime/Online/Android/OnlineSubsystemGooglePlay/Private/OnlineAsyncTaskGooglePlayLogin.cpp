// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemGooglePlayPrivatePCH.h"
#include "OnlineAsyncTaskGooglePlayLogin.h"

namespace
{
	const int MaxRetries = 1;
}

FOnlineAsyncTaskGooglePlayLogin::FOnlineAsyncTaskGooglePlayLogin(
	FOnlineSubsystemGooglePlay* InSubsystem,
	int InPlayerId)
	: FOnlineAsyncTaskBasic(InSubsystem)
	, PlayerId(InPlayerId)
	, Status(gpg::AuthStatus::ERROR_NOT_AUTHORIZED)
	, RetryCount(MaxRetries)
{
}

void FOnlineAsyncTaskGooglePlayLogin::Finalize()
{
	Subsystem->GetIdentityGooglePlay()->OnLoginCompleted(PlayerId, Status);
}

void FOnlineAsyncTaskGooglePlayLogin::TriggerDelegates()
{
}

void FOnlineAsyncTaskGooglePlayLogin::OnAuthActionFinished(gpg::AuthOperation InOp, gpg::AuthStatus InStatus)
{
	if (InOp == AuthOperation::SIGN_IN)
	{
		if (InStatus == AuthStatus::ERROR_NOT_AUTHORIZED && RetryCount > 0)
		{
			--RetryCount;
			Subsystem->GetGameServices()->StartAuthorizationUI();
			return;
		}
		
		Status = InStatus;
		bWasSuccessful = Status == AuthStatus::VALID;
		bIsComplete = true;
	}
}



