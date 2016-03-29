// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "QuitMatchCallbackProxy.h"
#include "GameFramework/Actor.h"
#include "RepLayout.h"

UQuitMatchCallbackProxy::UQuitMatchCallbackProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UQuitMatchCallbackProxy::~UQuitMatchCallbackProxy()
{
}

UQuitMatchCallbackProxy* UQuitMatchCallbackProxy::QuitMatch(UObject* WorldContextObject, APlayerController* PlayerController, FString MatchID, EMPMatchOutcome::Outcome Outcome, int32 TurnTimeoutInSeconds)
{
	UQuitMatchCallbackProxy* Proxy = NewObject<UQuitMatchCallbackProxy>();
	Proxy->PlayerControllerWeakPtr = PlayerController;
	Proxy->WorldContextObject = WorldContextObject;
    Proxy->MatchID = MatchID;
	Proxy->Outcome = Outcome;
	Proxy->TurnTimeoutInSeconds = TurnTimeoutInSeconds;
	return Proxy;
}

void UQuitMatchCallbackProxy::Activate()
{
	FOnlineSubsystemBPCallHelper Helper(TEXT("ConnectToService"), GEngine->GetWorldFromContextObject(WorldContextObject));
	Helper.QueryIDFromPlayerController(PlayerControllerWeakPtr.Get());

	if (Helper.IsValid())
	{
		IOnlineTurnBasedPtr OnlineTurnBasedPtr = Helper.OnlineSub->GetTurnBasedInterface();
		if (OnlineTurnBasedPtr.IsValid())
		{
			FTurnBasedMatchPtr Match = OnlineTurnBasedPtr->GetMatchWithID(MatchID);
			if (Match.IsValid())
			{
				FQuitMatchSignature QuitMatchSignature;
				QuitMatchSignature.BindUObject(this, &UQuitMatchCallbackProxy::QuitMatchDelegate);
				Match->QuitMatch(Outcome, TurnTimeoutInSeconds, QuitMatchSignature);
                return;
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

	// Fail immediately
	OnFailure.Broadcast();
}

void UQuitMatchCallbackProxy::QuitMatchDelegate(FString InMatchID, bool Succeeded)
{
	if (Succeeded)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast();
	}
}
