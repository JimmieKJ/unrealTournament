// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTReplicatedMapInfo.h"
#include "Net/UnrealNetwork.h"

AUTReplicatedMapInfo::AUTReplicatedMapInfo(const class FObjectInitializer& ObjectInitializer)
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

void AUTReplicatedMapInfo::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, MapPackageName, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, MapAssetName, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, Title, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, Author, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, Description, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, OptimalPlayerCount, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, OptimalTeamPlayerCount, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, Redirect, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AUTReplicatedMapInfo, MapScreenshotReference, COND_InitialOnly);

	DOREPLIFETIME(AUTReplicatedMapInfo, VoteCount);
}

void AUTReplicatedMapInfo::RegisterVoter(AUTPlayerState* Voter)
{
	int32 Index = VoterRegistry.Find(Voter);
	if (Index == INDEX_NONE)
	{
		VoterRegistry.Add(Voter);
		VoteCount++;
	}
}
void AUTReplicatedMapInfo::UnregisterVoter(AUTPlayerState* Voter)
{
	int32 Index = VoterRegistry.Find(Voter);
	if (Index != INDEX_NONE)
	{
		VoterRegistry.Remove(Voter);
		VoteCount--;
	}
}

void AUTReplicatedMapInfo::OnRep_VoteCount()
{
	UE_LOG(UT,Log,TEXT("Updated"));
	bNeedsUpdate = true;
}
