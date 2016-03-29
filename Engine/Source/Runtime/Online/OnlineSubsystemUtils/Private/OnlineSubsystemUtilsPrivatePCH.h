// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "Engine.h"
#include "Sockets.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSubsystemUtilsModule.h"
#include "SocketSubsystem.h"
#include "ModuleManager.h"
#include "Classes/IpConnection.h"
#include "Classes/IpNetDriver.h"

class FUniqueNetId;

// Helper class for various methods to reduce the call hierarchy
struct FOnlineSubsystemBPCallHelper
{
public:
	FOnlineSubsystemBPCallHelper(const TCHAR* CallFunctionContext, UWorld* World, FName SystemName = NAME_None);

	void QueryIDFromPlayerController(APlayerController* PlayerController);

	bool IsValid() const
	{
		return UserID.IsValid() && (OnlineSub != nullptr);
	}

public:
	TSharedPtr<const FUniqueNetId> UserID;
	IOnlineSubsystem* const OnlineSub;
	const TCHAR* FunctionContext;
};

#define INVALID_INDEX -1



