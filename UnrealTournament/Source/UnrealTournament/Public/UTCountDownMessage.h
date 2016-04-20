// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAnnouncer.h"

#include "UTCountDownMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTCountDownMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText CountDownText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText RedFlagDelayMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText BlueFlagDelayMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText GoldBonusMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText SilverBonusMessage;

	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const override
	{
		return FName(*FString::Printf(TEXT("CD%i"), Switch));
	}
	virtual void GetArgs(FFormatNamedArguments& Args, int32 Switch = 0, bool bTargetsPlayerState1 = false,class APlayerState* RelatedPlayerState_1 = NULL,class APlayerState* RelatedPlayerState_2 = NULL,class UObject* OptionalObject = NULL) const;
	virtual FText GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const override;
	virtual void PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const override;
	virtual float GetScaleInSize_Implementation(int32 MessageIndex) const override;
	virtual float GetLifeTime(int32 Switch) const override;
	virtual void GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override;

};

