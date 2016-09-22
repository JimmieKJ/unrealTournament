// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTAnnouncer.h"
#include "UTShowdownStatusMessage.h"

UUTShowdownStatusMessage::UUTShowdownStatusMessage(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsStatusAnnouncement = true;
	bIsPartiallyUnique = true;
	MessageArea = FName(TEXT("Announcements"));
	MessageSlot = FName(TEXT("VictimMessage")); 

	FinalLifeMsg = NSLOCTEXT("ShowdownRewardMessage", "FinalLifeMsg", "FINAL LIFE!");
	FinalLife = FName(TEXT("FinalLife"));
	LivesRemainingPrefix = NSLOCTEXT("CTFGameMessage", "LivesRemainingPrefix", "");
	LivesRemainingPostfix = NSLOCTEXT("CTFGameMessage", "LivesRemainingPostfix", " lives remaining.");
	AnnouncementDelay = 0.5f;
	bPlayDuringIntermission = false;

	LivesRemaining.Add(NAME_None);
	LivesRemaining.Add(FName(TEXT("TwoLivesRemain")));
	LivesRemaining.Add(FName(TEXT("ThreeLivesRemain")));
	LivesRemaining.Add(FName(TEXT("FourLivesRemain")));
	LivesRemaining.Add(FName(TEXT("FiveLivesRemain")));
}

FName LivesRemaining[5];

void UUTShowdownStatusMessage::GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	if (Switch > 30)
	{
		Switch -= 30;
		PrefixText = LivesRemainingPrefix;
		PostfixText = LivesRemainingPostfix;
		EmphasisText = FText::AsNumber(Switch);
		EmphasisColor = FLinearColor::Red;
		return;
	}

	Super::GetEmphasisText(PrefixText, EmphasisText, PostfixText, EmphasisColor, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

FText UUTShowdownStatusMessage::GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	if (Switch > 30)
	{
		return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
	}
	switch (Switch)
	{
	case 5:
		return FinalLifeMsg;
	default:
		return FText();
	}
}

bool UUTShowdownStatusMessage::ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const
{
	return IsLocalForAnnouncement(ClientData, true, true);
}

FName UUTShowdownStatusMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
{
	if ((Switch > 30) && (Switch-31 < LivesRemaining.Num()))
	{
		return LivesRemaining[Switch - 31];
	}
	switch (Switch)
	{
	case 5:
		return FinalLife;
	default:
		return NAME_None;
	}
}

void UUTShowdownStatusMessage::PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const
{
	Announcer->PrecacheAnnouncement(FinalLife);
	for (int32 i = 0; i < LivesRemaining.Num(); i++)
	{
		Announcer->PrecacheAnnouncement(LivesRemaining[i]);
	}
}
