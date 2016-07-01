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
	FlagReadyMessage = NSLOCTEXT("CTFGameMessage", "FlagReadyMessage", "Attacker flag can be picked up!");
	FlagRallyMessage = NSLOCTEXT("CTFGameMessage", "FlagRallyMessage", "RALLY NOW!");
	RallyReadyMessage = NSLOCTEXT("CTFGameMessage", "RallyReadyMessage", "Rally Available");
	EnemyRallyMessage = NSLOCTEXT("CTFGameMessage", "EnemyRallyMessage", "Enemy Rally!");
	bIsStatusAnnouncement = true;
	bIsPartiallyUnique = true;
	ScaleInSize = 3.f;

	static ConstructorHelpers::FObjectFinder<USoundBase> FlagWarningSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/FlagUp_stereo.FlagUp_stereo'"));
	FlagWarningSound = FlagWarningSoundFinder.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> FlagRallySoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/RallyCall.RallyCall'"));
	FlagRallySound = FlagRallySoundFinder.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> RallyReadySoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/RallyAvailable.RallyAvailable'"));
	RallyReadySound = RallyReadySoundFinder.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> EnemyRallySoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/EnemyRally.EnemyRally'"));
	EnemyRallySound = EnemyRallySoundFinder.Object;
}

void UUTCTFMajorMessage::ClientReceive(const FClientReceiveData& ClientData) const
{
	Super::ClientReceive(ClientData);
	AUTPlayerController* PC = Cast<AUTPlayerController>(ClientData.LocalPC);
	if (PC)
	{
		if (ClientData.MessageIndex == 21)
		{
			PC->UTClientPlaySound(FlagWarningSound);
		}
		else if (ClientData.MessageIndex == 22)
		{
			PC->UTClientPlaySound(FlagRallySound);
			PC->bNeedsRallyNotify = true;
		}
		else if (ClientData.MessageIndex == 23)
		{
			PC->UTClientPlaySound(RallyReadySound);
		}
		else if (ClientData.MessageIndex == 24)
		{
			PC->UTClientPlaySound(EnemyRallySound);
		}
	}
}

FLinearColor UUTCTFMajorMessage::GetMessageColor_Implementation(int32 MessageIndex) const
{
	return FLinearColor::White;
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

		FFormatNamedArguments Args;
		GetArgs(Args, Switch, false, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject );
		PrefixText = FText::Format(PrefixText,Args);
		PostfixText = FText::Format(PostfixText,Args);
		EmphasisText = FText::Format(EmphasisText,Args);

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
	case 21: return FlagReadyMessage; break;
	case 22: return FlagRallyMessage; break;
	case 23: return RallyReadyMessage; break;
	case 24: return EnemyRallyMessage; break;
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

FName UUTCTFMajorMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
{
	const AUTTeamInfo* TeamInfo = Cast<AUTTeamInfo>(OptionalObject);
	uint8 TeamNum = (TeamInfo != NULL) ? TeamInfo->GetTeamNum() : 0;
	return GetTeamAnnouncement(Switch, TeamNum);
}

