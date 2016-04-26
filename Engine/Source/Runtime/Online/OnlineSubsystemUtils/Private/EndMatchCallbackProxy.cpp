// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "EndMatchCallbackProxy.h"
#include "RepLayout.h"
#include "TurnBasedMatchInterface.h"

//////////////////////////////////////////////////////////////////////////
// UEndMatchCallbackProxy

UEndMatchCallbackProxy::UEndMatchCallbackProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, WorldContextObject(nullptr)
	, TurnBasedMatchInterface(nullptr)
{
}

UEndMatchCallbackProxy::~UEndMatchCallbackProxy()
{
}

UEndMatchCallbackProxy* UEndMatchCallbackProxy::EndMatch(UObject* WorldContextObject, class APlayerController* PlayerController, TScriptInterface<ITurnBasedMatchInterface> MatchActor, FString MatchID, EMPMatchOutcome::Outcome LocalPlayerOutcome, EMPMatchOutcome::Outcome OtherPlayersOutcome)
{
	UEndMatchCallbackProxy* Proxy = NewObject<UEndMatchCallbackProxy>();
	Proxy->PlayerControllerWeakPtr = PlayerController;
	Proxy->WorldContextObject = WorldContextObject;
	Proxy->MatchID = MatchID;
	Proxy->LocalPlayerOutcome = LocalPlayerOutcome;
	Proxy->OtherPlayersOutcome = OtherPlayersOutcome;
	return Proxy;
}

void UEndMatchCallbackProxy::Activate()
{
	FOnlineSubsystemBPCallHelper Helper(TEXT("ConnectToService"), GEngine->GetWorldFromContextObject(WorldContextObject));
	Helper.QueryIDFromPlayerController(PlayerControllerWeakPtr.Get());

	if (Helper.IsValid())
	{
		IOnlineTurnBasedPtr TurnBasedInterface = Helper.OnlineSub->GetTurnBasedInterface();
		if (TurnBasedInterface.IsValid())
		{
			FTurnBasedMatchPtr Match = TurnBasedInterface->GetMatchWithID(MatchID);
			if (Match.IsValid())
			{
				FEndMatchSignature EndMatchDelegate;
				EndMatchDelegate.BindUObject(this, &UEndMatchCallbackProxy::EndMatchDelegate);
				Match->EndMatch(EndMatchDelegate, LocalPlayerOutcome, OtherPlayersOutcome);
				return;
			}
		}
		else
		{
			FFrame::KismetExecutionMessage(TEXT("Turn based games not supported by online subsystem"), ELogVerbosity::Warning);
		}
	}

	// Fail immediately
	OnFailure.Broadcast();
}

void UEndMatchCallbackProxy::EndMatchDelegate(FString InMatchID, bool Succeeded)
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