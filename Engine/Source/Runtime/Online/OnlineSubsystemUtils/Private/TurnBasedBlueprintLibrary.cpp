// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "TurnBasedBlueprintLibrary.h"
#include "Runtime/Online/OnlineSubsystem/Public/Interfaces/OnlineTurnBasedInterface.h"

UTurnBasedBlueprintLibrary::UTurnBasedBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UTurnBasedBlueprintLibrary::GetIsMyTurn(UObject* WorldContextObject, APlayerController* PlayerController, FString MatchID, /*out*/ bool& bIsMyTurn)
{
	bIsMyTurn = false;

	FOnlineSubsystemBPCallHelper Helper(TEXT("GetIsMyTurn"), GEngine->GetWorldFromContextObject(WorldContextObject));
	Helper.QueryIDFromPlayerController(PlayerController);

	if (Helper.IsValid())
	{
		IOnlineTurnBasedPtr OnlineTurnBasedPtr = Helper.OnlineSub->GetTurnBasedInterface();
		if (OnlineTurnBasedPtr.IsValid())
		{
			FTurnBasedMatchPtr Match = OnlineTurnBasedPtr->GetMatchWithID(MatchID);
			if (Match.IsValid())
			{
				bIsMyTurn = Match->GetCurrentPlayerIndex() == Match->GetLocalPlayerIndex();
			}
			else
			{
				FString Message = FString::Printf(TEXT("Match ID %s not found"), *MatchID);
				FFrame::KismetExecutionMessage(*Message, ELogVerbosity::Warning);
			}
		}
		else
		{
			FFrame::KismetExecutionMessage(TEXT("Turn Based Matches not supported by Online Subsystem"), ELogVerbosity::Warning);
		}
	}
}

void UTurnBasedBlueprintLibrary::GetMyPlayerIndex(UObject* WorldContextObject, APlayerController* PlayerController, FString MatchID, /*out*/ int32& PlayerIndex)
{
	PlayerIndex = -1;

	FOnlineSubsystemBPCallHelper Helper(TEXT("GetMyPlayerIndex"), GEngine->GetWorldFromContextObject(WorldContextObject));
	Helper.QueryIDFromPlayerController(PlayerController);

	if (Helper.IsValid())
	{
		IOnlineTurnBasedPtr OnlineTurnBasedPtr = Helper.OnlineSub->GetTurnBasedInterface();
		if (OnlineTurnBasedPtr.IsValid())
		{
			FTurnBasedMatchPtr Match = OnlineTurnBasedPtr->GetMatchWithID(MatchID);
			if (Match.IsValid())
			{
				PlayerIndex = Match->GetLocalPlayerIndex();
			}
			else
			{
				FString Message = FString::Printf(TEXT("Match ID %s not found"), *MatchID);
				FFrame::KismetExecutionMessage(*Message, ELogVerbosity::Warning);
			}
		}
		else
		{
			FFrame::KismetExecutionMessage(TEXT("Turn Based Matches not supported by Online Subsystem"), ELogVerbosity::Warning);
		}
	}
}

void UTurnBasedBlueprintLibrary::RegisterTurnBasedMatchInterfaceObject(UObject* WorldContextObject, APlayerController* PlayerController, UObject* Object)
{
	FOnlineSubsystemBPCallHelper Helper(TEXT("GetIsMyTurn"), GEngine->GetWorldFromContextObject(WorldContextObject));
	Helper.QueryIDFromPlayerController(PlayerController);

	if (Helper.IsValid())
	{
		IOnlineTurnBasedPtr OnlineTurnBasedPtr = Helper.OnlineSub->GetTurnBasedInterface();
		if (OnlineTurnBasedPtr.IsValid())
		{
			OnlineTurnBasedPtr->RegisterTurnBasedMatchInterfaceObject(Object);
		}
	}
}