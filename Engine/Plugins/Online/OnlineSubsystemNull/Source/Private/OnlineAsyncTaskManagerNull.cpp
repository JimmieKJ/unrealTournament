// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemNullPrivatePCH.h"
#include "OnlineAsyncTaskManagerNull.h"
#include "OnlineSubsystemNull.h"

void FOnlineAsyncTaskManagerNull::OnlineTick()
{
	check(NullSubsystem);
	check(FPlatformTLS::GetCurrentThreadId() == OnlineThreadId || !FPlatformProcess::SupportsMultithreading());
}

