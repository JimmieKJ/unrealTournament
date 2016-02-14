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
	virtual void GetDefaultPersistentPartySettings(EPartyType& PartyType, bool& bLeaderInvitesOnly, bool& bLeaderFriendsOnly) override;
	virtual void HandlePendingJoin() override;
	// End UParty interface

	void AddPendingJoin(TSharedRef<const FUniqueNetId> InLocalUserId, const FPartyDetails& InPartyDetails, const UPartyDelegates::FOnJoinUPartyComplete& InDelegate);

	/** @return the party state for the persistent party */
	UUTPartyGameState* GetUTPersistentParty() const;

	static FOnlinePartyTypeId GetPersistentPartyTypeId() { return IOnlinePartySystem::GetPrimaryPartyTypeId(); }

private:

	UUTGameInstance* GetUTGameInstance() const;

	void OnLeavePartyForNewJoin(const FUniqueNetId& LocalUserId, const ELeavePartyCompletionResult Result, TSharedRef<FPendingPartyJoin> InPendingPartyJoin);
};