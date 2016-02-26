// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BlueprintContextTypes.h"
#include "BlueprintContextBase.h"
#include "Online/UTPartyGameState.h"
#include "Online/UTPartyMemberState.h"

class IGameAndPartyService;
class IChatNotificationService;
class FFriendsAndChatMessage;

// NOTE: Defining this enum prior to the generated header since the generated file contains
//		static delegates that depend on it being defined

/** Describes the player's current party transition */
UENUM(BlueprintType)
enum class EUTPartyTransition : uint8
{
	// No transition
	Idle,
	// Joining a new party
	Joining,
	// Leaving a party
	Leaving,
	Max UMETA(Hidden)
};

#include "PartyContext.generated.h"

/**
 * The PartyContext object is an Epic Ecosystem representation for your XMPP party.
 *
 * this should not be confused with your team, which is a gameplay concept of people you are playing with
 */
UCLASS()
class BLUEPRINTCONTEXT_API UPartyContext : public UBlueprintContextBase
{
	GENERATED_BODY()

	UPartyContext();

protected:
	
	TSharedPtr<const FOnlineUser> PendingPartyPlayerInfo;

	/** 
	 * Used to tell the party transition started and completed delegates of the action being 
	 * performed, e.g. joining, or leaving. 
	 */
	EUTPartyTransition CurrentTransition;

	IOnlinePartyPtr PartyInterface;
	IOnlineFriendsPtr FriendsInterface;
	IOnlineUserPtr UserInfoInterface;
	IOnlinePresencePtr PresenceInterface;
	TSharedPtr<class IFriendsAndChatManager> FriendsAndChatManager;

	void OnFriendsListJoinParty(const FUniqueNetId& SenderId, const TSharedRef<class IOnlinePartyJoinInfo>& PartyJoinInfo, bool bIsFromInvite);
	void JoinPartyInternal(const FUniqueNetId& LocalPlayerId, bool bIsFromInvite, const TSharedRef<const IOnlinePartyJoinInfo>& PartyJoinInfo);
	void OnJoinPartyCompleteInternal(const FUniqueNetId& LocalUserId, EJoinPartyCompletionResult Result, int32 DeniedResultCode);

	/** Handler for not approved join party failure code */
	void HandleJoinPartyFailure(EJoinPartyCompletionResult Result, int32 DeniedResultCode);
	void HandlePartyJoined(UPartyGameState* PartyState);
	void HandlePartyLeft(UPartyGameState* PartyState, EMemberExitedReason Reason);
	void HandlePartyMemberJoined(UPartyGameState* PartyState, const FUniqueNetIdRepl& UniqueId);
	void HandlePartyMemberLeft(UPartyGameState* PartyState, const FUniqueNetIdRepl& RemovedMemberId, EMemberExitedReason Reason);

public:

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartyTransitionStartedDelegate, EUTPartyTransition, PartyTransition);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartyTransitionCompleteDelegate, EUTPartyTransition, PartyTransition);
	DECLARE_MULTICAST_DELEGATE(FOnPartyJoinedDelegate);
	DECLARE_MULTICAST_DELEGATE(FOnPartyLeftDelegate);
	DECLARE_MULTICAST_DELEGATE(FOnPlayerStateChangedDelegate);

	/** Called when a player starts joining or leaving a party */
	UPROPERTY(BlueprintAssignable, Category=PartyContext)
	FOnPartyTransitionStartedDelegate OnPartyTransitionStarted;
	/** Called when a player has completed joining or leaving a party */
	UPROPERTY(BlueprintAssignable, Category=PartyContext)
	FOnPartyTransitionCompleteDelegate OnPartyTransitionCompleted;
	
	/** Called when the local player has joined a party */
	//UPROPERTY(BlueprintAssignable, Category = PartyContext)
	FOnPartyJoinedDelegate OnPartyJoined;

	/** Called when the local player has left a party */
	//UPROPERTY(BlueprintAssignable, Category=PartyContext)
	FOnPartyLeftDelegate OnPartyLeft;

	//UPROPERTY(BlueprintAssignable, Category = PartyContext)
	FOnPlayerStateChangedDelegate OnPlayerStateChanged;

	/**
	 * Gets the list of unique net IDs that correspond to each player in same party as the local player
	 *
	 * @param PartyMemberIDs The list of unique IDs for the players in the local party
	 */
	UFUNCTION(BlueprintCallable, Category = PartyContext)
	void GetLocalPartyMemberIDs(TArray<FUniqueNetIdRepl>& PartyMemberIDs) const;
	
	UFUNCTION(BlueprintCallable, Category = PartyContext)
	void GetLocalPartyMemberNames(TArray<FText>& PartyMemberNamess) const;
	
	virtual void Initialize() override;
	virtual void Finalize() override;
};