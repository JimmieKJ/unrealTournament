// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BlueprintContextTypes.h"
#include "BlueprintContextBase.h"

#include "UTOnlineSessionTypes.h"
#include "UTMatchmakingPolicy.h"

#include "MatchmakingContext.generated.h"

enum class EUTPartyState : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMatchmakingStartedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchmakingCompletedDelegate, EMatchmakingCompleteResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMatchmakingStateChangeDelegate, EMatchmakingState::Type, OldState, EMatchmakingState::Type, NewState);

UCLASS()
class BLUEPRINTCONTEXT_API UMatchmakingContext : public UBlueprintContextBase
{
	GENERATED_BODY()

	UMatchmakingContext();
	
	/**
	 * Delegate triggered when matchmaking starts
	 *
	 * @param SessionName name of session that was joined
	 */
	UPROPERTY(BlueprintAssignable, Category = MatchmakingContext)
	FOnMatchmakingStartedDelegate OnMatchmakingStarted;

	/**
	 * Delegate triggered when matchmaking is complete
	 *
	 * @param Result in what state matchmaking ended
	 */
	UPROPERTY(BlueprintAssignable, Category = MatchmakingContext)
	FOnMatchmakingCompletedDelegate OnMatchmakingComplete;

	/**
	 * Delegate triggered when matchmaking state changes
	 *
	 * @param OldState leaving state
	 * @param NewState entering state
	 * @param MMState structure containing extended information about matchmaking
	 */
	UPROPERTY(BlueprintAssignable, Category = MatchmakingContext)
	FOnMatchmakingStateChangeDelegate OnMatchmakingStateChange;

	/**
	 * Handle matchmaking started delegates
	 */
	void OnMatchmakingStartedInternal(bool bRanked);

	/**
	 * Handle when matchmaking state changes
	 *
	 * @param OldState leaving state
	 * @param NewState entering state
	 * @param MMState structure containing extended information about matchmaking
	 */
	void OnMatchmakingStateChangeInternal(EMatchmakingState::Type OldState, EMatchmakingState::Type NewState, const FMMAttemptState& MMState);

	/**
	 * Delegate triggered when matchmaking is complete
	 *
	 * @param EndResult in what state matchmaking ended
	 */
	void OnMatchmakingCompleteInternal(EMatchmakingCompleteResult EndResult);

	void OnPartyStateChangeInternal(EUTPartyState NewPartyState);

public:
	virtual void Initialize() override;
	virtual void Finalize() override;

	void StartMatchmaking(int32 InPlaylistId);
	void AttemptReconnect(const FString& OldSessionId);
};