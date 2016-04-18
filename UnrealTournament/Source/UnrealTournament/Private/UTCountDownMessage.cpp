// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTCountDownMessage.h"
#include "GameFramework/LocalMessage.h"

UUTCountDownMessage::UUTCountDownMessage(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsUnique = true;
	bOptionalSpoken = true;
	Lifetime = 0.95;
	MessageArea = FName(TEXT("Announcements"));
	MessageSlot = FName(TEXT("CountDownMessages"));
	bIsStatusAnnouncement = true;
	CountDownText = NSLOCTEXT("UTTimerMessage","MatBeginCountdown","{Count}");
	RedFlagDelayMessage = NSLOCTEXT("CTFGameMessage", "RedFlagDelay", "Red Flag can be picked up in ");
	BlueFlagDelayMessage = NSLOCTEXT("CTFGameMessage", "BlueFlagDelay", "Blue Flag can be picked up in ");
}

float UUTCountDownMessage::GetScaleInSize_Implementation(int32 MessageIndex) const
{
	return (MessageIndex >= 1000) ? 1.f : 4.f;
}

float UUTCountDownMessage::GetLifeTime(int32 Switch) const
{
	if (Switch >= 1000)
	{
		return 2.f;
	}
	return Blueprint_GetLifeTime(Switch);
}

void UUTCountDownMessage::GetArgs(FFormatNamedArguments& Args, int32 Switch, bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const
{
	Super::GetArgs(Args, Switch, bTargetsPlayerState1, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
	while (Switch >= 1000)
	{
		Switch -= 1000;
	}
	Args.Add(TEXT("Count"), FText::AsNumber(Switch));
}

void UUTCountDownMessage::GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	if (Switch < 1000)
	{
		Super::GetEmphasisText(PrefixText, EmphasisText, PostfixText, EmphasisColor, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
		return;
	}
	PrefixText = (Switch >= 2000) ? BlueFlagDelayMessage : RedFlagDelayMessage;
	PostfixText = FText::GetEmpty();
	while (Switch >= 1000)
	{
		Switch -= 1000;
	}
	FFormatNamedArguments Args;
	Args.Add(TEXT("Count"), FText::AsNumber(Switch));
	EmphasisText = FText::Format(CountDownText, Args);;
	EmphasisColor = FLinearColor::Yellow;
}

FText UUTCountDownMessage::GetText(int32 Switch, bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const
{
	if (Switch >= 1000)
	{
		return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
	}
	return GetDefault<UUTCountDownMessage>(GetClass())->CountDownText;
}

void UUTCountDownMessage::PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const
{
	for (int32 i = 0; i <= 10; i++)
	{
		Announcer->PrecacheAnnouncement(GetAnnouncementName(i, NULL));
	}
}