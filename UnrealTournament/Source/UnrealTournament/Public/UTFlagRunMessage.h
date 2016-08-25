// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTLocalMessage.h"
#include "UTFlagRunMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTFlagRunMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText DefendersMustStop;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText DefendersMustHold;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText AttackersMustScore;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText UnhandledCondition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText AttackersMustScoreWin;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText AttackersMustScoreTime;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText AttackersMustScoreTimeWin;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText AttackersMustScoreTimeOne;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText AttackersMustScoreChanceTwo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText AttackersMustScoreTimeWinTwo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText BronzeBonusText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText SilverBonusText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText GoldBonusText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText BlueTeamText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText RedTeamText;

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override;
	virtual void GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override;
	virtual void SplitPostfixText(FText& PostfixText, FText& SecondPostfixText,int32 Switch, class UObject* OptionalObject) const;
	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const override;
};



