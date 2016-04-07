// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCarriedObjectMessage.h"
#include "UTCTFGameMessage.h"
#include "UTSCTFGameMessage.h"
#include "UTAnnouncer.h"

UUTSCTFGameMessage::UUTSCTFGameMessage(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	MessageArea = FName(TEXT("GameMessages"));
	InitialFlagSpawnMessage = NSLOCTEXT("SCTFGameMessage","FlagSpawnTimer","Flag spawns in 30 seconds!");
	FlagSpawnMessage = NSLOCTEXT("SCTFGameMessage", "FlagSpawn", "Flag spawns in...");
	YouAreOnOffenseMessage = NSLOCTEXT("SCTFGameMessage", "OnOffense", "You are on OFFENSE!");
	YouAreOnDefenseMessage = NSLOCTEXT("SCTFGameMessage", "OnDefense", "You are on DEFENSE!");
}

FText UUTSCTFGameMessage::GetText(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const
{

	switch (Switch)
	{
		case 0 : return InitialFlagSpawnMessage; break;
		case 1 : return FlagSpawnMessage; break;
		case 2 : return YouAreOnOffenseMessage; break;
		case 3 : return YouAreOnDefenseMessage; break;
	}

	return Super::GetText(Switch, bTargetsPlayerState1, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

