// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalMessage.h"
#include "UTCTFRewardMessage.h"
#include "GameFramework/LocalMessage.h"

UUTCTFRewardMessage::UUTCTFRewardMessage(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bIsPartiallyUnique = true;
	Lifetime = 5.0f;
	MessageArea = FName(TEXT("Announcements"));
	MessageSlot = FName(TEXT("MajorRewardMessage"));
	bIsConsoleMessage = false;
	DeniedMessage = NSLOCTEXT("CTFRewardMessage", "Denied", "Denied by ");
	RejectedMessage = NSLOCTEXT("CTFRewardMessage", "Rejected", "Redeemer Rejected by ");
	BlueTeamName = NSLOCTEXT("CTFRewardMessage", "BlueTeamName", "BLUE TEAM");
	RedTeamName = NSLOCTEXT("CTFRewardMessage", "RedTeamName", "RED TEAM");
	TeamScorePrefix = NSLOCTEXT("CTFRewardMessage", "TeamScorePrefix", "");
	TeamScorePostfix = NSLOCTEXT("CTFRewardMessage", "TeamScorePostfix", " Scores!");
	GoldScoreBonusPrefix = NSLOCTEXT("CTFRewardMessage", "GoldScoreBonusPrefix", "");
	GoldScoreBonusPostfix = NSLOCTEXT("CTFRewardMessage", "GoldScoreBonusPostfix", " Scores!  \u2605 \u2605 \u2605");
	SilverScoreBonusPrefix = NSLOCTEXT("CTFRewardMessage", "SilverScoreBonusPrefix", "");
	SilverScoreBonusPostfix = NSLOCTEXT("CTFRewardMessage", "SilverScoreBonusPostfix", " Scores!  \u2605 \u2605");
	BronzeScoreBonusPrefix = NSLOCTEXT("CTFRewardMessage", "BronzeScoreBonusPrefix", "");
	BronzeScoreBonusPostfix = NSLOCTEXT("CTFRewardMessage", "BronzeScoreBonusPostfix", " Scores!  \u2605");
	DefenseScoreBonusPrefix = NSLOCTEXT("CTFRewardMessage", "DefenseScoreBonusPrefix", "");
	DefenseScoreBonusPostfix = NSLOCTEXT("CTFRewardMessage", "DefenseScoreBonusPostfix", " successfully defends!  \u2605");
	EarnedSpecialPrefix = NSLOCTEXT("CTFGameMessage", "EarnedSpecialPrefix", "");
	EarnedSpecialPostfix = NSLOCTEXT("CTFGameMessage", "EarnedSpecialPostfix", " earned a power up for your team!");
	ExclamationPostfix = NSLOCTEXT("CTFGameMessage", "ExclamationPostfix", "!");
	bIsStatusAnnouncement = false;
	bWantsBotReaction = true;
	ScaleInSize = 3.f;

	static ConstructorHelpers::FObjectFinder<USoundBase> EarnedSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/BoostAvailable.BoostAvailable'"));
	EarnedBoostSound = EarnedSoundFinder.Object;
}

bool UUTCTFRewardMessage::InterruptAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const
{
	if (GetClass() == OtherMessageClass)
	{
		return ((Switch == 3) || (Switch == 4) || (Switch >= 100));
	}
	if (Switch == 6)
	{
		return  (Cast<UUTLocalMessage>(OtherMessageClass->GetDefaultObject())->GetAnnouncementPriority(OtherSwitch) < 0.7f);
	}
	return Cast<UUTLocalMessage>(OtherMessageClass->GetDefaultObject())->IsOptionalSpoken(OtherSwitch);
}

void UUTCTFRewardMessage::ClientReceive(const FClientReceiveData& ClientData) const 
{
	Super::ClientReceive(ClientData);
	if (EarnedBoostSound && (ClientData.MessageIndex == 7))
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(ClientData.LocalPC);
		if (PC != NULL)
		{
			PC->UTClientPlaySound(EarnedBoostSound);
		}
	}
}

FLinearColor UUTCTFRewardMessage::GetMessageColor_Implementation(int32 MessageIndex) const
{
	return FLinearColor::White;
}

void UUTCTFRewardMessage::PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const
{
	for (int32 i = 0; i < 8; i++)
	{
		Announcer->PrecacheAnnouncement(GetAnnouncementName(i, NULL, NULL, NULL));
	}
}

float UUTCTFRewardMessage::GetAnnouncementDelay(int32 Switch)
{
	if (Switch == 7)
	{
		return 1.f;
	}
	return 0.f;
}

float UUTCTFRewardMessage::GetAnnouncementPriority(int32 Switch) const
{
	return 1.f;
}

FName UUTCTFRewardMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
{
	switch (Switch)
	{
	case 0: return TEXT("RW_Rejected"); break;
	case 1: return TEXT("LastSecondSave"); break;
	case 6: return TEXT("Denied"); break;
	case 7: return TEXT("RZE_PowerupUnlocked"); break;
	}
	return NAME_None;
}

bool UUTCTFRewardMessage::ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const
{
	return (ClientData.MessageIndex == 0) || (ClientData.MessageIndex == 6)  || (ClientData.MessageIndex == 7) || IsLocalForAnnouncement(ClientData, true, true) || (ClientData.MessageIndex >= 100);
}

void UUTCTFRewardMessage::GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	if (Switch == 0)
	{
		PrefixText = RejectedMessage;
		EmphasisText = RelatedPlayerState_1 ? FText::FromString(RelatedPlayerState_1->PlayerName) : FText::GetEmpty();
		AUTPlayerState* Denier = Cast<AUTPlayerState>(RelatedPlayerState_1);
		EmphasisColor = Denier && Denier->Team && (Denier->Team->TeamIndex == 1) ? FLinearColor::Blue : FLinearColor::Red;
		PostfixText = ExclamationPostfix;
		return;
	}
	if (Switch == 6)
	{
		PrefixText = DeniedMessage;
		EmphasisText = RelatedPlayerState_1 ? FText::FromString(RelatedPlayerState_1->PlayerName) : FText::GetEmpty();
		AUTPlayerState* Denier = Cast<AUTPlayerState>(RelatedPlayerState_1);
		EmphasisColor = Denier && Denier->Team && (Denier->Team->TeamIndex == 1) ? FLinearColor::Blue : FLinearColor::Red;
		PostfixText = ExclamationPostfix;
		return;
	}
	if ((Switch == 3) || (Switch == 4))
	{
		PrefixText = TeamScorePrefix;
		PostfixText = TeamScorePostfix;
		EmphasisText = (Switch == 3) ? RedTeamName : BlueTeamName;
		EmphasisColor = (Switch == 3) ? FLinearColor::Red : FLinearColor::Blue;
		return;
	}
	if (Switch == 7)
	{
		PrefixText = EarnedSpecialPrefix;
		PostfixText = EarnedSpecialPostfix;
		EmphasisText = RelatedPlayerState_1 ? FText::FromString(RelatedPlayerState_1->PlayerName) : FText::GetEmpty();
		EmphasisColor = FLinearColor::Yellow;
		return;
	}
	if (Switch >= 100)
	{
		FText TeamScoreBonusPrefix = BronzeScoreBonusPrefix;
		FText TeamScoreBonusPostfix = BronzeScoreBonusPostfix;
		if (Switch >= 400)
		{
			TeamScoreBonusPrefix = DefenseScoreBonusPrefix;
			TeamScoreBonusPostfix = DefenseScoreBonusPostfix;
		}
		else if (Switch >= 300)
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
	case 0: return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject); break;
	case 3: return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject); break;
	case 4: return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject); break;
	case 6: return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject); break;
	case 7: return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject); break;
	}
	if (Switch > 100)
	{
		return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
	}

	return FText::GetEmpty();
}


