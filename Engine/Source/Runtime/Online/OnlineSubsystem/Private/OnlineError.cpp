// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemPrivatePCH.h"
#include "OnlineError.h"
#include "OnlineSubsystemTypes.h"

//static
const FString FOnlineError::GenericErrorCode = TEXT("GenericError");

FOnlineError::FOnlineError()
	: bSucceeded(false)
	, HttpResult(0)
{
}

FOnlineError::FOnlineError(bool bSucceededIn)
	: bSucceeded(bSucceededIn)
	, HttpResult(0)
{
}

FOnlineError::FOnlineError(const FString& ErrorCodeIn)
	: bSucceeded(false)
	, HttpResult(0)
{
	SetFromErrorCode(ErrorCodeIn);
}

void FOnlineError::SetFromErrorCode(const FString& ErrorCodeIn)
{
	ErrorCode = ErrorCodeIn;
	ErrorRaw = ErrorCodeIn;
}

FOnlineError::FOnlineError(const FText& ErrorMessageIn)
	: bSucceeded(false)
	, HttpResult(0)
{
	SetFromErrorMessage(ErrorMessageIn);
}

void FOnlineError::SetFromErrorMessage(const FText& ErrorMessageIn)
{
	ErrorMessage = ErrorMessageIn;
	ErrorCode = FTextInspector::GetKey(ErrorMessageIn).Get(GenericErrorCode);
	ErrorRaw = ErrorMessageIn.ToString();
}

EOnlineServerConnectionStatus::Type FOnlineError::GetConnectionStatusFromHttpResult() const
{
	if (bSucceeded)
	{
		return EOnlineServerConnectionStatus::Connected;
	}

	switch (HttpResult)
	{
	case 0:
		// no response means we couldn't even connect
		return EOnlineServerConnectionStatus::ConnectionDropped;
	case 401:
		// No auth at all
		return EOnlineServerConnectionStatus::InvalidUser;
	case 403:
		// Auth failure
		return EOnlineServerConnectionStatus::NotAuthorized;
	case 503:
		// service is not avail
		return EOnlineServerConnectionStatus::ServersTooBusy;
	case 505:
		// update to supported version
		return EOnlineServerConnectionStatus::UpdateRequired;
	default:
		// other bad responses (ELB, etc)
		return EOnlineServerConnectionStatus::ServiceUnavailable;
	}
}
