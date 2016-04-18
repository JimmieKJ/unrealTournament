// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAnnouncer.h"

#include "UTCTFRewardMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTCTFRewardMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText AssistMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText DeniedMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText BlueTeamName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText RedTeamName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText TeamScorePrefix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText TeamScorePostfix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText HatTrickMessage;

	/** Message when someone else got a hat trick. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText OtherHatTrickMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText StoutDefensePrefix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText StoutDefensePostfix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText TeamScoreBonusPrefix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText TeamScoreBonusPostfix;

	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const override;
	virtual bool ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const override;
	virtual FLinearColor GetMessageColor_Implementation(int32 MessageIndex) const override;
	virtual void PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const override;
	virtual float GetAnnouncementDelay(int32 Switch) override;
	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override;
	virtual void GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override;
};
