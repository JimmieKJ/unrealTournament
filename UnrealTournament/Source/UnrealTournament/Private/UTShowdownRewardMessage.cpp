// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTAnnouncer.h"
#include "UTShowdownRewardMessage.h"

UUTShowdownRewardMessage::UUTShowdownRewardMessage(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsStatusAnnouncement = false;
	bIsPartiallyUnique = true;
	MessageArea = FName(TEXT("DeathMessage"));

	FinishItMsg = NSLOCTEXT("ShowdownRewardMessage", "FinishItMsg", "FINISH IT!");
	LastManMsg = NSLOCTEXT("ShowdownRewardMessage", "LastManMsg", "Last Man Standing");
	OverChargeMsg = NSLOCTEXT("ShowdownRewardMessage", "OverChargeMsg", "OVERCHARGE AVAILABLE!");
	TerminationMsg = NSLOCTEXT("ShowdownRewardMessage", "TerminationMsg", "TERMINATED!");
	FinishIt = FName(TEXT("RW_FinishIt"));
	LastMan = FName(TEXT("RW_LMS"));
	OverCharge = FName(TEXT("Overload"));
	Termination = FName(TEXT("RW_Termination"));
}

FText UUTShowdownRewardMessage::GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const 
{
	switch (Switch)
	{
	case 0:
		return FinishItMsg;
	case 1:
		return LastManMsg;
	case 2:
		return OverChargeMsg;
	case 3:
		return TerminationMsg;
	default:
		return FText();
	}
}

bool UUTShowdownRewardMessage::ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const
{
	return IsLocalForAnnouncement(ClientData, true, true);
}

FLinearColor UUTShowdownRewardMessage::GetMessageColor_Implementation(int32 MessageIndex) const 
{
	return FLinearColor::White;
}

bool UUTShowdownRewardMessage::UseLargeFont(int32 MessageIndex) const
{
	return true;
}

FName UUTShowdownRewardMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	switch (Switch)
	{
	case 0:
		return FinishIt;
	case 1:
		return LastMan;
	case 2:
		return OverCharge;
	case 3:
		return Termination;
	default:
		return NAME_None;
	}
}

void UUTShowdownRewardMessage::PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const
{
	Announcer->PrecacheAnnouncement(FinishIt);
	Announcer->PrecacheAnnouncement(LastMan);
	Announcer->PrecacheAnnouncement(OverCharge);
	Announcer->PrecacheAnnouncement(Termination);
}
