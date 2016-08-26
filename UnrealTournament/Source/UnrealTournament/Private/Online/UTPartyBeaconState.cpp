// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPartyBeaconState.h"

UUTPartyBeaconState::UUTPartyBeaconState(const FObjectInitializer& ObjectInitializer) :
Super(ObjectInitializer)
{
}

void UUTPartyBeaconState::ChangeSessionOwner(const TSharedRef<const FUniqueNetId>& InNewSessionOwner)
{
	GameSessionOwner = InNewSessionOwner;
}

void UUTPartyBeaconState::SetUserConfiguration(const FEmptyServerReservation& InReservationData, const TSharedPtr<const FUniqueNetId>& InGameSessionOwner)
{
	ReservationData = InReservationData;
	GameSessionOwner = InGameSessionOwner;
}

/**
 * Helper for sorting team sizes
 */
struct FTeamBalanceInfo
{
	/** Index of team */
	int32 TeamIdx;
	/** Current size of team */
	int32 TeamSize;

	FTeamBalanceInfo(int32 InTeamIdx, int32 InTeamSize)
		: TeamIdx(InTeamIdx),
		TeamSize(InTeamSize)
	{}
};

/**
 * Sort teams by size (equal teams are randomly mixed)
 */
struct FSortTeamSizeLargestToSmallest
{
	bool operator()(const FTeamBalanceInfo& A, const FTeamBalanceInfo& B) const
	{
		if (A.TeamSize == B.TeamSize)
		{
			return FMath::Rand() % 2 ? true : false;
		}
		else
		{
			return A.TeamSize > B.TeamSize;
		}
	}
};

struct FSortTeamSizeSmallestToLargest
{
	bool operator()(const FTeamBalanceInfo& A, const FTeamBalanceInfo& B) const
	{
		if (A.TeamSize == B.TeamSize)
		{
			return FMath::Rand() % 2 ? true : false;
		}
		else
		{
			return A.TeamSize < B.TeamSize;
		}
	}
};

int32 UUTPartyBeaconState::GetTeamAssignment(const FPartyReservation& Party)
{
	if (NumTeams > 1)
	{
		TArray<FTeamBalanceInfo> PotentialTeamChoices;
		for (int32 TeamIdx = 0; TeamIdx < NumTeams; TeamIdx++)
		{
			const int32 CurrentPlayersOnTeam = GetNumPlayersOnTeam(TeamIdx);
			if ((CurrentPlayersOnTeam + Party.PartyMembers.Num()) <= NumPlayersPerTeam)
			{
				new (PotentialTeamChoices)FTeamBalanceInfo(TeamIdx, CurrentPlayersOnTeam);
			}
		}

		// Grab one from our list of choices
		if (PotentialTeamChoices.Num() > 0)
		{
			if (ReservationData.bRanked)
			{
				// Choose largest team
				PotentialTeamChoices.Sort(FSortTeamSizeLargestToSmallest());
				return PotentialTeamChoices[0].TeamIdx;
			}
			else
			{
				// Choose smallest team
				PotentialTeamChoices.Sort(FSortTeamSizeSmallestToLargest());
				return PotentialTeamChoices[0].TeamIdx;
			}
		}
		else
		{
			UE_LOG(LogBeacon, Warning, TEXT("(UUTPartyBeaconState.GetTeamAssignment): couldn't find an open team for party members."));
			return INDEX_NONE;
		}
	}

	return ForceTeamNum;
}