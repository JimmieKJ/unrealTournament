// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTCTFRewardMessage.h"
#include "GameFramework/LocalMessage.h"

UUTCTFRewardMessage::UUTCTFRewardMessage(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bIsPartiallyUnique = true;
	bIsSpecial = true;
	Lifetime = 5.0f;
	MessageArea = FName(TEXT("Announcements"));
	MessageSlot = FName(TEXT("MajorRewardMessage"));
	bIsConsoleMessage = false;
	AssistMessage = NSLOCTEXT("CTFRewardMessage", "Assist", "Assist!");
	DeniedMessage = NSLOCTEXT("CTFRewardMessage", "Denied", "Denied!");
	BlueTeamName = NSLOCTEXT("CTFRewardMessage", "BlueTeamName", "BLUE TEAM");
	RedTeamName = NSLOCTEXT("CTFRewardMessage", "RedTeamName", "RED TEAM");
	TeamScorePrefix = NSLOCTEXT("CTFRewardMessage", "TeamScorePrefix", "");
	TeamScorePostfix = NSLOCTEXT("CTFRewardMessage", "TeamScorePostfix", " Scores!");
	HatTrickMessage = NSLOCTEXT("CTFRewardMessage", "HatTrick", "Hat Trick!");
	OtherHatTrickMessage = NSLOCTEXT("CTFRewardMessage", "OtherHatTrick", "{Player1Name} got a Hat Trick!");
	GoldScoreBonusPrefix = NSLOCTEXT("CTFRewardMessage", "GoldScoreBonusPrefix", "");
	GoldScoreBonusPostfix = NSLOCTEXT("CTFRewardMessage", "GoldScoreBonusPostfix", " Scores!  Gold Bonus +{BonusAmount}");
	SilverScoreBonusPrefix = NSLOCTEXT("CTFRewardMessage", "SilverScoreBonusPrefix", "");
	SilverScoreBonusPostfix = NSLOCTEXT("CTFRewardMessage", "SilverScoreBonusPostfix", " Scores!  Silver Bonus +{BonusAmount}");
	BronzeScoreBonusPrefix = NSLOCTEXT("CTFRewardMessage", "BronzeScoreBonusPrefix", "");
	BronzeScoreBonusPostfix = NSLOCTEXT("CTFRewardMessage", "BronzeScoreBonusPostfix", " Scores!  Bronze Bonus +{BonusAmount}");
	bIsStatusAnnouncement = false;
	bWantsBotReaction = true;
	ScaleInSize = 3.f;
}

FLinearColor UUTCTFRewardMessage::GetMessageColor_Implementation(int32 MessageIndex) const
{
	if (MessageIndex == 3)
	{
		return FLinearColor::Red;
	}
	if (MessageIndex == 4)
	{
		return FLinearColor::Blue;
	}
	return FLinearColor::White;
}

void UUTCTFRewardMessage::PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const
{
	for (int32 i = 0; i < 6; i++)
	{
		Announcer->PrecacheAnnouncement(GetAnnouncementName(i, NULL, NULL, NULL));
	}
}

float UUTCTFRewardMessage::GetAnnouncementDelay(int32 Switch)
{
	return ((Switch == 2) || (Switch == 5)) ? 1.5f : 0.f;
}

FName UUTCTFRewardMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
{
	switch (Switch)
	{
	case 0: return TEXT("Denied"); break;
	case 1: return TEXT("LastSecondSave"); break;
	case 2: return TEXT("Assist"); break;
	case 5: return TEXT("HatTrick"); break;
	case 6: return TEXT("Denied"); break;
	}
	return NAME_None;
}

bool UUTCTFRewardMessage::ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const
{
	return (ClientData.MessageIndex == 6) || IsLocalForAnnouncement(ClientData, true, true) || (ClientData.MessageIndex > 100);
}


void UUTCTFRewardMessage::GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	if ((Switch == 3) || (Switch == 4))
	{
		PrefixText = TeamScorePrefix;
		PostfixText = TeamScorePostfix;
		EmphasisText = (Switch == 3) ? RedTeamName : BlueTeamName;
		EmphasisColor = (Switch == 3) ? FLinearColor::Red : FLinearColor::Blue;
		return;
	}
	if (Switch >= 100)
	{
		FText TeamScoreBonusPrefix = BronzeScoreBonusPrefix;
		FText TeamScoreBonusPostfix = BronzeScoreBonusPostfix;
		if (Switch >= 300)
		{
			TeamScoreBonusPrefix = GoldScoreBonusPrefix;
			TeamScoreBonusPostfix = GoldScoreBonusPostfix;
		}
		else if (Switch >= 200)
		{
			TeamScoreBonusPrefix = SilverScoreBonusPrefix;
			TeamScoreBonusPostfix = SilverScoreBonusPostfix;
		}

		while (Switch >= 100)
		{
			Switch -= 100;
		}
		FFormatNamedArguments Args;
		Args.Add("BonusAmount", FText::AsNumber(Switch));
		PrefixText = FText::Format(TeamScoreBonusPrefix, Args);
		PostfixText = FText::Format(TeamScoreBonusPostfix, Args);
		AUTTeamInfo* EmphasisTeam = Cast<AUTTeamInfo>(OptionalObject);
		EmphasisText = (EmphasisTeam && EmphasisTeam->TeamIndex == 1) ? BlueTeamName : RedTeamName;
		EmphasisColor = (EmphasisTeam && EmphasisTeam->TeamIndex == 1) ? FLinearColor::Blue : FLinearColor::Red;
		return;
	}
	Super::GetEmphasisText(PrefixText, EmphasisText, PostfixText, EmphasisColor, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

FText UUTCTFRewardMessage::GetText(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const
{
	switch (Switch)
	{
	case 0: return DeniedMessage; break;
	case 2: return AssistMessage; break;
	case 3: return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject); break;
	case 4: return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject); break;
	case 5: return (bTargetsPlayerState1 ? HatTrickMessage : OtherHatTrickMessage); break;
	case 6: return DeniedMessage; break;
	}
	if (Switch > 100)
	{
		return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
	}

	return FText::GetEmpty();
}


