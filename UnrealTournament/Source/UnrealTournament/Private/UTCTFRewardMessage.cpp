// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTCTFRewardMessage.h"
#include "GameFramework/LocalMessage.h"

UUTCTFRewardMessage::UUTCTFRewardMessage(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bIsUnique = true;
	bIsSpecial = true;
	Lifetime = 3.0f;
	MessageArea = FName(TEXT("DeathMessage"));
	bIsConsoleMessage = false;
	AssistMessage = NSLOCTEXT("CTFRewardMessage", "Assist", "Assist!");
	DeniedMessage = NSLOCTEXT("CTFRewardMessage", "Denied", "Denied!");
	BlueScoreMessage = NSLOCTEXT("CTFRewardMessage", "Blue Score", "BLUE Team Scores!");
	RedScoreMessage = NSLOCTEXT("CTFRewardMessage", "Red Score", "RED Team Scores!");

	bIsStatusAnnouncement = false;
}

FLinearColor UUTCTFRewardMessage::GetMessageColor(int32 MessageIndex) const
{
	if (MessageIndex == 3)
	{
		return FLinearColor::Red;
	}
	if (MessageIndex == 4)
	{
		return FLinearColor::Blue;
	}
	return FLinearColor::Yellow;
}

void UUTCTFRewardMessage::PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const
{
	for (int32 i = 0; i < 3; i++)
	{
		Announcer->PrecacheAnnouncement(GetAnnouncementName(i, NULL));
	}
}

float UUTCTFRewardMessage::GetScaleInSize(int32 MessageIndex) const
{
	return 3.f;
}

float UUTCTFRewardMessage::GetAnnouncementDelay(int32 Switch)
{
	return (Switch == 2) ? 1.5f : 0.f;
}

FName UUTCTFRewardMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	switch (Switch)
	{
	case 0: return TEXT("Denied"); break;
	case 1: return TEXT("LastSecondSave"); break;
	case 2: return TEXT("Assist"); break;
	}
	return NAME_None;
}

bool UUTCTFRewardMessage::ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const
{
	return ((ClientData.RelatedPlayerState_1 != NULL && ClientData.LocalPC == ClientData.RelatedPlayerState_1->GetOwner())
		|| (ClientData.RelatedPlayerState_2 != NULL && ClientData.LocalPC == ClientData.RelatedPlayerState_2->GetOwner()));
}

FText UUTCTFRewardMessage::GetText(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const
{
	switch (Switch)
	{
	case 0: return DeniedMessage; break;
	case 2: return AssistMessage; break;
	case 3: return RedScoreMessage; break;
	case 4: return BlueScoreMessage; break;
	}

	return FText::GetEmpty();
}


