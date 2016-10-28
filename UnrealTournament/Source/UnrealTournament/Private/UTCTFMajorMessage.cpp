// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCarriedObjectMessage.h"
#include "UTCTFMajorMessage.h"
#include "UTAnnouncer.h"

UUTCTFMajorMessage::UUTCTFMajorMessage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Lifetime = 3.0f;
	MessageArea = FName(TEXT("Announcements"));
	MessageSlot = FName(TEXT("MajorRewardMessage"));
	HalftimeMessage = NSLOCTEXT("CTFGameMessage", "Halftime", "");
	OvertimeMessage = NSLOCTEXT("CTFGameMessage", "Overtime", "OVERTIME!");
	FlagReadyMessage = NSLOCTEXT("CTFGameMessage", "FlagReadyMessage", "Attacker flag activated!");
	FlagRallyMessage = NSLOCTEXT("CTFGameMessage", "FlagRallyMessage", "RALLY NOW!");
	RallyReadyMessage = NSLOCTEXT("CTFGameMessage", "RallyReadyMessage", "Rally Available");
	EnemyRallyMessage = NSLOCTEXT("CTFGameMessage", "EnemyRallyMessage", "Enemy Rally!");
	EnemyRallyPrefix = NSLOCTEXT("CTFGameMessage", "EnemyRallyPrefix", "Enemy Rally in ");
	EnemyRallyPostfix = NSLOCTEXT("CTFGameMessage", "EnemyRallyPostfix", "");
	TeamRallyMessage = NSLOCTEXT("CTFGameMessage", "TeamRallyMessage", "");
	RallyCompleteMessage = NSLOCTEXT("CTFGameMessage", "RallyCompleteMessage", "Rally Complete!");
	bIsStatusAnnouncement = true;
	bIsPartiallyUnique = true;
	ScaleInSize = 3.f;
	RallyCompleteName = TEXT("RallyComplete");
	FontSizeIndex = 1;

	static ConstructorHelpers::FObjectFinder<USoundBase> FlagWarningSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/FlagUp_stereo.FlagUp_stereo'"));
	FlagWarningSound = FlagWarningSoundFinder.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> FlagRallySoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/RallyCall.RallyCall'"));
	FlagRallySound = FlagRallySoundFinder.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> RallyReadySoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/RallyAvailable.RallyAvailable'"));
	RallyReadySound = RallyReadySoundFinder.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> EnemyRallySoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/EnemyRally.EnemyRally'"));
	EnemyRallySound = EnemyRallySoundFinder.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> RallyFinalSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/RallyFinal.RallyFinal'"));
	RallyFinalSound = RallyFinalSoundFinder.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> RallyCompleteSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/RallyComplete.RallyComplete'"));
	RallyCompleteSound = RallyCompleteSoundFinder.Object;
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
		else if ((ClientData.MessageIndex == 23) || (ClientData.MessageIndex == 28))
		{
			PC->UTClientPlaySound(RallyReadySound);
			PC->bNeedsRallyNotify = (ClientData.MessageIndex == 23);
		}
		else if (ClientData.MessageIndex == 24)
		{
			PC->UTClientPlaySound(EnemyRallySound);
		}
		else if (ClientData.MessageIndex == 25)
		{
			PC->UTClientPlaySound(RallyFinalSound);
		}
		else if (ClientData.MessageIndex == 27)
		{
			PC->UTClientPlaySound(RallyCompleteSound);
		}
	}
}

bool UUTCTFMajorMessage::ShouldDrawMessage(int32 MessageIndex, AUTPlayerController* PC, bool bIsAtIntermission, bool bNoLivePawnTarget) const
{
	if (MessageIndex == 23)
	{
		// only draw if can still rally
		if (!PC->CanPerformRally())
		{
			return false;
		}
	}
	
	return Super::ShouldDrawMessage(MessageIndex, PC, bIsAtIntermission, bNoLivePawnTarget);
}

FLinearColor UUTCTFMajorMessage::GetMessageColor_Implementation(int32 MessageIndex) const
{
	return FLinearColor::White;
}

void UUTCTFMajorMessage::GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
{
	if (Switch == 24)
	{
		AUTGameVolume* GV = Cast<AUTGameVolume>(OptionalObject);
		if (GV)
		{
			PrefixText = EnemyRallyPrefix;
			EmphasisText = GV->VolumeName;
			PostfixText = EnemyRallyPostfix;
			AUTPlayerState* PS = Cast<AUTPlayerState>(RelatedPlayerState_1);
			EmphasisColor = (PS && PS->Team) ? PS->Team->TeamColor : FLinearColor::Red;
		}
		else
		{
			AUTCarriedObject* Flag = Cast<AUTCarriedObject>(OptionalObject);
			if (Flag)
			{
				PrefixText = EnemyRallyPrefix;
				EmphasisText = Flag->GetHUDStatusMessage(nullptr);
				PostfixText = EnemyRallyPostfix;
				AUTPlayerState* PS = Cast<AUTPlayerState>(RelatedPlayerState_1);
				EmphasisColor = (PS && PS->Team) ? PS->Team->TeamColor : FLinearColor::Red;
			}
			else
			{
				PrefixText = EnemyRallyMessage;
				EmphasisText = FText::GetEmpty();
				PostfixText = FText::GetEmpty();
			}
		}
		return;
	}

	Super::GetEmphasisText(PrefixText, EmphasisText, PostfixText, EmphasisColor, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

FText UUTCTFMajorMessage::GetText(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const
{
	switch (Switch)
	{
	case 11: return HalftimeMessage; break;
	case 12: return OvertimeMessage; break;
	case 21: return FlagReadyMessage; break;
	case 22: return FlagRallyMessage; break;
	case 23: return RallyReadyMessage; break;
	case 24: return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject); break;
	case 25: return RallyCompleteMessage; break;
	case 27: return TeamRallyMessage; break;
	case 28: return EnemyRallyMessage; break;
	}
	return FText::GetEmpty();
}

float UUTCTFMajorMessage::GetAnnouncementPriority(const FAnnouncementInfo AnnouncementInfo) const
{
	return (AnnouncementInfo.Switch <13) ? 1.f : 0.f;
}

bool UUTCTFMajorMessage::InterruptAnnouncement(const FAnnouncementInfo AnnouncementInfo, const FAnnouncementInfo OtherAnnouncementInfo) const
{
	if (OtherAnnouncementInfo.MessageClass->GetDefaultObject<UUTLocalMessage>()->IsOptionalSpoken(OtherAnnouncementInfo.Switch))
	{
		return true;
	}
	if (AnnouncementInfo.MessageClass == OtherAnnouncementInfo.MessageClass)
	{
		return false;
	}
	if (GetAnnouncementPriority(AnnouncementInfo) > OtherAnnouncementInfo.MessageClass->GetDefaultObject<UUTLocalMessage>()->GetAnnouncementPriority(OtherAnnouncementInfo))
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
	Announcer->PrecacheAnnouncement(GetTeamAnnouncement(11, 0));
	Announcer->PrecacheAnnouncement(GetTeamAnnouncement(12, 0));
	Announcer->PrecacheAnnouncement(GetTeamAnnouncement(21, 0));
}

FName UUTCTFMajorMessage::GetTeamAnnouncement(int32 Switch, uint8 TeamNum) const
{
	switch (Switch)
	{
	case 11: return TEXT("HalfTime"); break;
	case 12: return TEXT("OverTime"); break;
	case 21: return TEXT("FlagCanBePickedUp"); break;
	//case 25: return RallyCompleteName; break;
	}
	return NAME_None;
}

FName UUTCTFMajorMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
{
	const AUTTeamInfo* TeamInfo = Cast<AUTTeamInfo>(OptionalObject);
	uint8 TeamNum = (TeamInfo != NULL) ? TeamInfo->GetTeamNum() : 0;
	return GetTeamAnnouncement(Switch, TeamNum);
}

