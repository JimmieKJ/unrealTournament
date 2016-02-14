// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTOnlineSessionTypes.generated.h"

UENUM(BlueprintType)
namespace EMatchmakingState
{
	enum Type
	{
		// Not in any matchmaking (begin/end state machine)
		NotMatchmaking,
		// In process of canceling matchmaking
		CancelingMatchmaking,
		// Release a lock on the world
		ReleasingLock,
		// Acquire a lock on the world
		AcquiringLock,
		// Failed to acquire a lock on the world, game needs to make decision
		LockAcquistionFailure,
		// Getting a list of empty servers
		FindingEmptyServer,
		// Getting a list of available sessions
		FindingExistingSession,
		// Testing empty server list
		TestingEmptyServers,
		// Testing available session list 
		TestingExistingSessions,
		// In process of joining a session
		JoiningExistingSession,
		// Cleaning after no matches available, but not done matchmaking
		NoMatchesAvailable,
		// Waiting for cleanup and continue
		CleaningUpExisting,
		// Handling an error condition (end state machine)
		HandlingFailure,
		// Successful join (end state machine)
		JoinSuccess
	};
}

namespace EMatchmakingState
{
	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EMatchmakingState::Type MatchmakingState)
	{
		switch (MatchmakingState)
		{
		case NotMatchmaking:
		{
			return TEXT("Not Matchmaking");
		}
		case CancelingMatchmaking:
		{
			return TEXT("Canceling Matchmaking");
		}
		case AcquiringLock:
		{
			return TEXT("Acquiring Lock");
		}
		case ReleasingLock:
		{
			return TEXT("Releasing Lock");
		}
		case LockAcquistionFailure:
		{
			return TEXT("Lock Acquistion Failure");
		}
		case FindingEmptyServer:
		{
			return TEXT("Finding Empty Server");
		}
		case FindingExistingSession:
		{
			return TEXT("Finding Existing Session");
		}
		case TestingEmptyServers:
		{
			return TEXT("Testing Empty Servers");
		}
		case TestingExistingSessions:
		{
			return TEXT("Testing Existing Sessions");
		}
		case JoiningExistingSession:
		{
			return TEXT("Joining Existing Session");
		}
		case NoMatchesAvailable:
		{
			return TEXT("No matches available");
		}
		case CleaningUpExisting:
		{
			return TEXT("Cleaning up existing session");
		}
		case HandlingFailure:
		{
			return TEXT("Handling failure");
		}
		case JoinSuccess:
		{
			return TEXT("Join Success");
		}
		}
		return TEXT("");
	}
}
