// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFGameMessage.h"
#include "UTFlagRunGameMessage.h"

UUTFlagRunGameMessage::UUTFlagRunGameMessage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FName UUTFlagRunGameMessage::GetTeamAnnouncement(int32 Switch, uint8 TeamNum, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
{
	switch (Switch)
	{
	case 0: TEXT("FlagDown"); break;
	case 1: TEXT("FlagDown"); break;
	case 2: return TeamNum == 0 ? TEXT("RedTeamScores") : TEXT("BlueTeamScores"); break;
	case 3: TEXT("FlagDropped"); break;
	case 4:
		if (RelatedPlayerState_1 && Cast<AUTPlayerController>(RelatedPlayerState_1->GetOwner()))
		{
			return TEXT("YouHaveTheFlag_02"); break;
		}
		return TEXT("FlagTaken"); break;
	}
	return NAME_None;
}

float UUTFlagRunGameMessage::GetAnnouncementSpacing_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	return 0.05f;
}
