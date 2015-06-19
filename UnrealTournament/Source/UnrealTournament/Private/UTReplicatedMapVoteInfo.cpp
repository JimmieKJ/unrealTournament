// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTReplicatedMapVoteInfo.h"
#include "Net/UnrealNetwork.h"

AUTReplicatedMapVoteInfo::AUTReplicatedMapVoteInfo(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bAlwaysRelevant = true;
	bReplicateMovement = false;
	bNetLoadOnClient = false;

#if !UE_SERVER
	MapBrush = nullptr;
#endif

}

void AUTReplicatedMapVoteInfo::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTReplicatedMapVoteInfo, MapTitle, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedMapVoteInfo, MapScreenshotReference, COND_InitialOnly);

	DOREPLIFETIME(AUTReplicatedMapVoteInfo, VoteCount);
}

void AUTReplicatedMapVoteInfo::RegisterVoter(AUTPlayerState* Voter)
{
	int32 Index = VoterRegistry.Find(Voter);
	if (Index == INDEX_NONE)
	{
		VoterRegistry.Add(Voter);
		VoteCount++;
	}
}
void AUTReplicatedMapVoteInfo::UnregisterVoter(AUTPlayerState* Voter)
{
	int32 Index = VoterRegistry.Find(Voter);
	if (Index != INDEX_NONE)
	{
		VoterRegistry.Remove(Voter);
		VoteCount--;
	}
}

void AUTReplicatedMapVoteInfo::OnRep_VoteCount()
{
	UE_LOG(UT,Log,TEXT("Updated"));
	bNeedsUpdate = true;
}
