// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFGameMessage.generated.h"

UCLASS()
class UUTCTFGameMessage : public UUTCarriedObjectMessage
{
	GENERATED_UCLASS_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText ReturnMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText ReturnedMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText CaptureMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText DroppedMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText HasMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText KilledMessage;

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const;

	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const override
	{
		uint8 TeamNum = 0;

		const AUTTeamInfo* TeamInfo = Cast<AUTTeamInfo>(OptionalObject);
		if (TeamInfo != NULL)
		{
			TeamNum = TeamInfo->GetTeamNum();
		}

		switch (Switch)
		{
			case 0 : return TeamNum == 0 ? TEXT("RedFlagReturned") : TEXT("BlueFlagReturned"); break;
			case 1 : return TeamNum == 0 ? TEXT("RedFlagReturned") : TEXT("BlueFlagReturned"); break;
			case 2 : return TeamNum == 0 ? TEXT("BlueTeamScores") : TEXT("RedTeamScores"); break;
			case 3 : return TeamNum == 0 ? TEXT("RedFlagDropped") : TEXT("BlueFlagDropped"); break;
			case 4 : return TeamNum == 0 ? TEXT("RedFlagTaken") : TEXT("BlueFlagTaken"); break;
		}

		return NAME_None;
	}

};


