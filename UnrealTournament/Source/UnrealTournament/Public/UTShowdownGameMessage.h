// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTAnnouncer.h"
#include "UTShowdownGameMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTShowdownGameMessage : public UUTLocalMessage
{
	GENERATED_BODY()
public:
	UUTShowdownGameMessage(const FObjectInitializer& OI)
		: Super(OI)
	{
		bIsStatusAnnouncement = true;
		bIsPartiallyUnique = true;
		MessageArea = FName(TEXT("Announcements"));
		MessageSlot = FName(TEXT("GameMessages"));

		WinByHealthMsg = NSLOCTEXT("ShowdownMessage", "WinByHealthMsg", "{0} had the most total remaining health and wins the round.");
		TieByHealthMsg = NSLOCTEXT("ShowdownMessage", "TieByHealthMsg", "Both players had the same total remaining health and both get a point.");
		OnDeckAnnounce = FName(TEXT("SelectYourSpawn"));
		RedTeamRound = FName(TEXT("RedTeamWinsRound"));
		BlueTeamRound = FName(TEXT("BlueTeamWinsRound"));
		NewRoundIn = FName(TEXT("NewRoundIn"));
		FinalRound = FName(TEXT("FinalRound"));
		FiveKillsLeft = FName(TEXT("DM_Kills5"));
		bPlayDuringInstantReplay = false;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText WinByHealthMsg;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText TieByHealthMsg;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FName OnDeckAnnounce;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FName RedTeamRound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FName BlueTeamRound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FName NewRoundIn;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FName FinalRound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FName FiveKillsLeft;

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override
	{
		switch (Switch)
		{
			case 0:
				return FText::Format(WinByHealthMsg, FText::FromString(RelatedPlayerState_1 != NULL ? RelatedPlayerState_1->PlayerName : TEXT("Player")));
			case 1:
				return TieByHealthMsg;
			default:
				return FText();
		}
	}

	virtual FLinearColor GetMessageColor_Implementation(int32 MessageIndex) const override
	{
		return FLinearColor::White;
	}

	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const override
	{
		switch (Switch)
		{
		case 2:
			return OnDeckAnnounce;
		case 3:
			return RedTeamRound;
		case 4:
			return BlueTeamRound;
		case 5:
			return NewRoundIn;
		case 6:
			return FinalRound;
		case 7:
			return FiveKillsLeft;
		default:
			return NAME_None;
		}
	}

	virtual void PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const override
	{
		Announcer->PrecacheAnnouncement(OnDeckAnnounce);
		Announcer->PrecacheAnnouncement(RedTeamRound);
		Announcer->PrecacheAnnouncement(BlueTeamRound);
		Announcer->PrecacheAnnouncement(NewRoundIn);
		Announcer->PrecacheAnnouncement(FinalRound);
		Announcer->PrecacheAnnouncement(FiveKillsLeft);
	}
};