// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTAnnouncer.h"
#include "UTShowdownRewardMessage.h"

UUTShowdownRewardMessage::UUTShowdownRewardMessage(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsStatusAnnouncement = false;
	bIsPartiallyUnique = true;
	MessageArea = FName(TEXT("Announcements"));
	MessageSlot = FName(TEXT("VictimMessage"));

	FinishItMsg = NSLOCTEXT("ShowdownRewardMessage", "FinishItMsg", "FINISH IT!");
	LastManMsg = NSLOCTEXT("ShowdownRewardMessage", "LastManMsg", "Last Man Standing");
	OverChargeMsg = NSLOCTEXT("ShowdownRewardMessage", "OverChargeMsg", "OVERCHARGE AVAILABLE!");
	TerminationMsg = NSLOCTEXT("ShowdownRewardMessage", "TerminationMsg", "{Player1Name} TERMINATED!");
	AnnihilationMsg = NSLOCTEXT("ShowdownRewardMessage", "AnnihilationMsg", "ANNIHILATION!");
	FinalLifeMsg = NSLOCTEXT("ShowdownRewardMessage", "FinalLifeMsg", "FINAL LIFE!");
	FinishIt = FName(TEXT("RW_FinishIt"));
	LastMan = FName(TEXT("RW_LMS"));
	OverCharge = FName(TEXT("Overload"));
	Termination = FName(TEXT("RW_Termination"));
	Annihilation = FName(TEXT("RW_Annihilation"));
	FinalLife = FName(TEXT("RW_EndIt"));
	LivesRemainingPrefix = NSLOCTEXT("CTFGameMessage", "LivesRemainingPrefix", "");
	LivesRemainingPostfix = NSLOCTEXT("CTFGameMessage", "LivesRemainingPostfix", " lives remaining.");
	AnnouncementDelay = 0.5f;

	static ConstructorHelpers::FObjectFinder<USoundBase> TerminationSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/Terminated.Terminated'"));
	TerminationSound = TerminationSoundFinder.Object;
}

void UUTShowdownRewardMessage::ClientReceive(const FClientReceiveData& ClientData) const
{
	Super::ClientReceive(ClientData);
	if (TerminationSound && (ClientData.MessageIndex == 3))
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(ClientData.LocalPC);
		if (PC != NULL)
		{
			if (TerminationSound != NULL)
			{
				PC->ClientPlaySound(TerminationSound);
			}
		}
	}
}

float UUTShowdownRewardMessage::GetAnnouncementDelay(int32 Switch)
{
	return (Switch == 3) ? 0.5f : 0.f;
}

void UUTShowdownRewardMessage::GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const
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

FText UUTShowdownRewardMessage::GetText(int32 Switch, bool bTargetsPlayerState1, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const 
{
	if (Switch > 30)
	{
		return BuildEmphasisText(Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
	}
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
	case 4:
		return AnnihilationMsg;
	case 5:
		return FinalLifeMsg;
	default:
		return FText();
	}
}

bool UUTShowdownRewardMessage::ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const
{
	return IsLocalForAnnouncement(ClientData, true, true) ||(ClientData.MessageIndex == 2) || (ClientData.MessageIndex == 3) || (ClientData.MessageIndex == 4);
}

FLinearColor UUTShowdownRewardMessage::GetMessageColor_Implementation(int32 MessageIndex) const 
{
	return FLinearColor::White;
}

FName UUTShowdownRewardMessage::GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const
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
	case 4:
		return Annihilation;
	case 5:
		return FinalLife;
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
	Announcer->PrecacheAnnouncement(Annihilation);
}
