// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTPartyMemberState.h"
#include "UTPartyGameState.h"
#include "UTParty.h"


#define LOCTEXT_NAMESPACE "UTParties"

void FUTPartyMemberRepState::Reset()
{
	FPartyMemberRepState::Reset();
	Location = EUTPartyMemberLocation::PreLobby;
}

UUTPartyMemberState::UUTPartyMemberState(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	InitPartyMemberState(&MemberState);

	Reset();
}

void UUTPartyMemberState::Reset()
{
	MemberState.Reset();
}

void UUTPartyMemberState::SetLocation(EUTPartyMemberLocation NewLocation)
{
	if (MemberState.Location != NewLocation)
	{
		MemberState.Location = NewLocation;
		UUTPartyGameState* Party = GetUTParty();
		check(Party);
		Party->UpdatePartyMemberState(UniqueId, this);
	}
}

void UUTPartyMemberState::ComparePartyMemberData(const FPartyMemberRepState& OldPartyMemberState)
{
	const FUTPartyMemberRepState& OldUTPartyMemberState = static_cast<const FUTPartyMemberRepState&>(OldPartyMemberState);

	UUTPartyGameState* Party = GetUTParty();
	check(Party);
	
	if (OldUTPartyMemberState.Location != MemberState.Location)
	{
		Party->OnLocationChanged().Broadcast(UniqueId);
	}
}

#undef LOCTEXT_NAMESPACE