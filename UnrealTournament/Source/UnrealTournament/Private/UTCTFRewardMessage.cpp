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
	EarnedSpecialPrefix = NSLOCTEXT("CTFGameMessage", "EarnedSpecialPrefix", "");
	EarnedSpecialPostfix = NSLOCTEXT("CTFGameMessage", "EarnedSpecialPostfix", " earned a power up for your team!");
	ExclamationPostfix = NSLOCTEXT("CTFGameMessage", "ExclamationPostfix", "!");
	bIsStatusAnnouncement = false;
	bWantsBotReaction = true;
	ScaleInSize = 3.f;

	static ConstructorHelpers::FObjectFinder<USoundBase> EarnedSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/BoostAvailable.BoostAvailable'"));
	EarnedBoostSound = EarnedSoundFinder.Object;
}

bool UUTCTFRewardMessage::InterruptAnnouncement(const FAnnouncementInfo AnnouncementInfo, const FAnnouncementInfo OtherAnnouncementInfo) const
{
	if (AnnouncementInfo.MessageClass == OtherAnnouncementInfo.MessageClass)
	{
		return ((AnnouncementInfo.Switch == 3) || (AnnouncementInfo.Switch == 4) || (AnnouncementInfo.Switch >= 100));
	}
	if (AnnouncementInfo.Switch == 6)
	{
		return  (Cast<UUTLocalMessage>(OtherAnnouncementInfo.MessageClass->GetDefaultObject())->GetAnnouncementPriority(OtherAnnouncementInfo) < 0.7f);
	}
	return Cast<UUTLocalMessage>(OtherAnnouncementInfo.MessageClass->GetDefaultObject())->IsOptionalSpoken(OtherAnnouncementInfo.Switch);
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

float UUTCTFRewardMessage::GetAnnouncementPriority(const FAnnouncementInfo AnnouncementInfo) const
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

	return FText::GetEmpty();
}


