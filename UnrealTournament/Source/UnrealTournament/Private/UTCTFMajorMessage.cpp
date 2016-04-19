// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCarriedObjectMessage.h"
#include "UTCTFMajorMessage.h"
#include "UTAnnouncer.h"

UUTCTFMajorMessage::UUTCTFMajorMessage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsSpecial = true;
	Lifetime = 3.0f;
	MessageArea = FName(TEXT("Announcements"));
	MessageSlot = FName(TEXT("MajorRewardMessage"));
	CaptureMessagePrefix = NSLOCTEXT("CTFGameMessage", "CaptureMessagePrefix", "");
	CaptureMessagePostfix = NSLOCTEXT("CTFGameMessage", "CaptureMessagePostfix", " scores for {OptionalTeam}!");
	HalftimeMessage = NSLOCTEXT("CTFGameMessage", "Halftime", "");
	OvertimeMessage = NSLOCTEXT("CTFGameMessage", "Overtime", "OVERTIME!");
	LastLifeMessage = NSLOCTEXT("CTFGameMessage", "LastLife", "This is your last life!");

	bIsStatusAnnouncement = true;
	bIsPartiallyUnique = true;
	ScaleInSize = 3.f;
}

FLinearColor UUTCTFMajorMessage::GetMessageColor_Implementation(int32 MessageIndex) const
{
	return FLinearColor::Yellow;
}

void UUTCTFMajorMessage::GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	if ((Switch == 2) || (Switch == 8) || (Switch == 9))
	{
		PrefixText = CaptureMessagePrefix;
		PostfixText = CaptureMessagePostfix;
		EmphasisText = RelatedPlayerState_1 ? FText::FromString(RelatedPlayerState_1->PlayerName) : FText::GetEmpty();
		AUTPlayerState* PS = Cast<AUTPlayerState>(RelatedPlayerState_1);
		EmphasisColor = (PS && PS->Team) ? PS->Team->TeamColor : FLinearColor::Red;
		return;
	}

	Super::GetEmphasisText(PrefixText, EmphasisText, PostfixText, EmphasisColor, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

FText UUTCTFMajorMessage::GetText(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const
{
	switch (Switch)
	{
	case 2: return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject); break;
	case 8: return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject); break;
	case 9: return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject); break;
	case 11: return HalftimeMessage; break;
	case 12: return OvertimeMessage; break;
	case 20: return LastLifeMessage; break;
	}
	return FText::GetEmpty();
}

float UUTCTFMajorMessage::GetAnnouncementPriority(int32 Switch) const
{
	return 1.f;
}

bool UUTCTFMajorMessage::InterruptAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const
{
	if (OtherMessageClass->GetDefaultObject<UUTLocalMessage>()->bOptionalSpoken)
	{
		return true;
	}
	if (GetClass() == OtherMessageClass)
	{
		return false;
	}
	if (GetAnnouncementPriority(Switch) > OtherMessageClass->GetDefaultObject<UUTLocalMessage>()->GetAnnouncementPriority(OtherSwitch))
	{
		return true;
	}
	return false;
}

float UUTCTFMajorMessage::GetAnnouncementSpacing_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	return 0.1f;
}

bool UUTCTFMajorMessage::CancelByAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const
{
	return false;
}

void UUTCTFMajorMessage::PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const
{
	for (int32 i = 0; i < 11; i++)
	{
		for (uint8 j = 0; j < 2; j++)
		{
			Announcer->PrecacheAnnouncement(GetTeamAnnouncement(i, j));
		}
	}
	Announcer->PrecacheAnnouncement(GetTeamAnnouncement(11, 0));
	Announcer->PrecacheAnnouncement(GetTeamAnnouncement(12, 0));
}

FName UUTCTFMajorMessage::GetTeamAnnouncement(int32 Switch, uint8 TeamNum) const
{
	switch (Switch)
	{
	case 2: return TeamNum == 0 ? TEXT("RedTeamScores") : TEXT("BlueTeamScores"); break;
	case 8: return TeamNum == 0 ? TEXT("RedIncreasesLead") : TEXT("BlueIncreasesLead"); break;
	case 9: return TeamNum == 0 ? TEXT("RedDominating") : TEXT("BlueDominating"); break;
	case 10: return TeamNum == 0 ? TEXT("RedDominating") : TEXT("BlueDominating"); break;
	case 11: return TEXT("HalfTime"); break;
	case 12: return TEXT("OverTime"); break;
		//case 14: return TEXT("Attack"); break;
		//case 15: return TEXT("Defend"); break;
	}
	return NAME_None;
}

FName UUTCTFMajorMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const
{
	const AUTTeamInfo* TeamInfo = Cast<AUTTeamInfo>(OptionalObject);
	uint8 TeamNum = (TeamInfo != NULL) ? TeamInfo->GetTeamNum() : 0;
	return GetTeamAnnouncement(Switch, TeamNum);
}

