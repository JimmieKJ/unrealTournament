// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

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
		MessageArea = FName(TEXT("GameMessages"));

		WinByHealthMsg = NSLOCTEXT("ShowdownMessage", "WinByHealthMsg", "{0} had the most total remaining health and wins the round.");
		TieByHealthMsg = NSLOCTEXT("ShowdownMessage", "TieByHealthMsg", "Both players had the same total remaining health and both get a point.");
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText WinByHealthMsg;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText TieByHealthMsg;

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

	virtual FLinearColor GetMessageColor(int32 MessageIndex) const override
	{
		return FLinearColor::White;
	}
	virtual bool UseLargeFont(int32 MessageIndex) const override
	{
		return true;
	}
};