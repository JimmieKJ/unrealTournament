// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText HasAdvantageMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText LosingAdvantageMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText HalftimeMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText AssistMessage;

	virtual bool InterruptAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const override;

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const;

	virtual FLinearColor GetMessageColor(int32 MessageIndex) const override;
	virtual FName GetTeamAnnouncement(int32 Switch, uint8 TeamIndex) const;
	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const override;
	virtual void PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const override;
};


