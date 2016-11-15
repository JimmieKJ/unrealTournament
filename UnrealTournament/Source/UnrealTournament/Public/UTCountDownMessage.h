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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
		FText EndingInText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText GoldBonusMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText SilverBonusMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText RoundPrefix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText RoundPostfix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		TArray<FName> RoundName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		USoundBase* TimeWarningSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		USoundBase* TimeEndingSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FName GoldBonusName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FName SilverBonusName;

	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const override
	{
		if (Switch >= 1000)
		{
			if (Switch == 4007)
			{
				return GoldBonusName;
			}
			else if (Switch == 3007)
			{
				return SilverBonusName;
			}
			else if ((Switch < 2001+RoundName.Num()) && (Switch > 2000))
			{
				return RoundName[Switch - 2001];
			}
			return NAME_None;
		}
		return FName(*FString::Printf(TEXT("CD%i"), Switch));
	}
	virtual void GetArgs(FFormatNamedArguments& Args, int32 Switch = 0, bool bTargetsPlayerState1 = false,class APlayerState* RelatedPlayerState_1 = NULL,class APlayerState* RelatedPlayerState_2 = NULL,class UObject* OptionalObject = NULL) const;
	virtual FText GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const override;
	virtual void PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const override;
	virtual float GetScaleInSize_Implementation(int32 MessageIndex) const override;
	virtual float GetLifeTime(int32 Switch) const override;
	virtual void GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override;
	virtual void ClientReceive(const FClientReceiveData& ClientData) const override;
	virtual bool IsOptionalSpoken(int32 MessageIndex) const override;
};

