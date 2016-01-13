// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTShowdownRewardMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTShowdownRewardMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()
public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FName FinishIt;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FName LastMan;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FName OverCharge;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FName Termination;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText FinishItMsg;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FText LastManMsg;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FText OverChargeMsg;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FText TerminationMsg;

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override;
	virtual FLinearColor GetMessageColor_Implementation(int32 MessageIndex) const override;
	virtual bool UseLargeFont(int32 MessageIndex) const override;
	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const override;
	virtual void PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const override;
	virtual bool ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const override;
};