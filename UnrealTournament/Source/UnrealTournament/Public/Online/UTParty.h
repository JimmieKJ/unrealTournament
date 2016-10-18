// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Party.h"
#include "PartyModule.h"
#include "UTParty.generated.h"

class UUTPartyGameState;
class UUTGameInstance;

/**
 * High level singleton for the management of parties, all parties are contained within
 */
UCLASS(config=Game, notplaceable)
class UNREALTOURNAMENT_API UUTParty : public UParty
{
	GENERATED_UCLASS_BODY()
	
public:
	// Begin UParty interface
	virtual void UpdatePersistentPartyLeader(const FUniqueNetIdRepl& NewPartyLeader) override;
	virtual void GetDefaultPersistentPartySettings(EPartyType& PartyType, bool& bLeaderFriendsOnly, bool& bLeaderInvitesOnly, bool& bAllowInvites) override;
	virtual void HandlePendingJoin() override;
	// End UParty interface

	void AddPendingJoin(TSharedRef<const FUniqueNetId> InLocalUserId, TSharedRef<const FPartyDetails> InPartyDetails, const UPartyDelegates::FOnJoinUPartyComplete& InDelegate);

	/** @return the party state for the persistent party */
	UUTPartyGameState* GetUTPersistentParty() const;

	static FOnlinePartyTypeId GetPersistentPartyTypeId() { return IOnlinePartySystem::GetPrimaryPartyTypeId(); }

	/**
	*	Delegate fired when a party join completes
	*
	*	@param LocalUserId The user that was attempting to join
	*	@param Result - result of the operation
	*	@param NotApprovedReason - client defined value describing why you were not approved
	*/
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPartyJoinComplete, const FUniqueNetId& /*LocalUserId*/, const EJoinPartyCompletionResult /*Result*/, const int32 /*NotApprovedReason*/);

	/** @return delegate when party join fails */
	FOnPartyJoinComplete& OnPartyJoinComplete() { return PartyJoinComplete; }

	void SetSession(const FOnlineSessionSearchResult& InSearchResult);

private:

	UUTGameInstance* GetUTGameInstance() const;

	FOnPartyJoinComplete PartyJoinComplete;

	void ProcessInviteFromSearchResult(TSharedPtr< const FUniqueNetId > UserId, const FOnlineSessionSearchResult & InviteResult);
	void OnJoinPersistentPartyFromInviteComplete(const FUniqueNetId& LocalUserId, const EJoinPartyCompletionResult Result, const int32 NotApprovedReason);
	void OnLeavePartyForNewJoin(const FUniqueNetId& LocalUserId, const ELeavePartyCompletionResult Result, TSharedRef<FPendingPartyJoin> InPendingPartyJoin);
};