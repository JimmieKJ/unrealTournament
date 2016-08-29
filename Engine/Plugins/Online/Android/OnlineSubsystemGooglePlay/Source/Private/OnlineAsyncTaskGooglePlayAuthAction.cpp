// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemGooglePlayPrivatePCH.h"
#include "OnlineAsyncTaskGooglePlayAuthAction.h"

#include "gpg/builder.h"
#include "gpg/debug.h"

FOnlineAsyncTaskGooglePlayAuthAction::FOnlineAsyncTaskGooglePlayAuthAction(
	FOnlineSubsystemGooglePlay* InSubsystem)
	: FOnlineAsyncTaskBasic(InSubsystem)
	, bInit(false)
{
	check(InSubsystem);
}

void FOnlineAsyncTaskGooglePlayAuthAction::Tick()
{
	if ( !bInit)
	{
		Start_OnTaskThread();
		bInit = true;
	}
}
