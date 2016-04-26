// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTTeamShowdownGameMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTTeamShowdownGameMessage : public UUTLocalMessage
{
	GENERATED_BODY()
public:
	UUTTeamShowdownGameMessage(const FObjectInitializer& OI)
		: Super(OI)
	{
		bIsStatusAnnouncement = true;
		bIsPartiallyUnique = true;
		MessageArea = FName(TEXT("Announcements"));
		MessageSlot = FName(TEXT("GameMessages"));

		WinByNumbersMsg = NSLOCTEXT("ShowdownMessage", "WinByNumbersMsg", "{0} Team had more players alive and wins the round.");
		WinByHealthMsg = NSLOCTEXT("ShowdownMessage", "WinByHealthMsg", "{0} Team had the most total remaining health and wins the round.");
		TieMsg = NSLOCTEXT("ShowdownMessage", "TieMsg", "Both teams had the same total remaining health and both get a point.");
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText WinByNumbersMsg;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText WinByHealthMsg;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText TieMsg;

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override
	{
		AUTTeamInfo* Team = Cast<AUTTeamInfo>(OptionalObject);
		switch (Switch)
		{
			case 0:
				return FText::Format(WinByNumbersMsg, (Team != NULL) ? Team->TeamName : FText());
			case 1:
				return FText::Format(WinByHealthMsg, (Team != NULL) ? Team->TeamName : FText());
			case 2:
				return TieMsg;
			default:
				return FText();
		}
	}

	virtual FLinearColor GetMessageColor_Implementation(int32 MessageIndex) const override
	{
		return FLinearColor::White;
	}
};