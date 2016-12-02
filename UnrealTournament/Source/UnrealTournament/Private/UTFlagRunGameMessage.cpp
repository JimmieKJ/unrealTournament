// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCTFGameMessage.h"
#include "UTFlagRunGameMessage.h"

UUTFlagRunGameMessage::UUTFlagRunGameMessage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	KilledMessagePostfix = NSLOCTEXT("CTFGameMessage", "KilledMessage", " killed the flag carrier!");
	NoPowerRallyMessage = NSLOCTEXT("FlagRunGameMessage", "NoPowerRally", "You need the flag to power a rally point.");
}

FName UUTFlagRunGameMessage::GetTeamAnnouncement(int32 Switch, uint8 TeamNum, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
{
	switch (Switch)
	{
	case 0: return TEXT("FlagIsReturning"); break;
	case 1: return TEXT("FlagIsReturning"); break;
	case 2: return (TeamNum == 0) ? TEXT("RedTeamScores") : TEXT("BlueTeamScores"); break;
	case 3: return TEXT("FlagDropped"); break;
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
	return 0.f;
}

FText UUTFlagRunGameMessage::GetText(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const
{
	if ((Switch < 2) || (Switch == 3) || ((Switch == 4) && (!RelatedPlayerState_1 || !Cast<AUTPlayerController>(RelatedPlayerState_1->GetOwner()))))
	{
		return FText::GetEmpty();
	}
	if (Switch == 30)
	{
		return NoPowerRallyMessage;
	}
	return Super::GetText(Switch, bTargetsPlayerState1, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}
