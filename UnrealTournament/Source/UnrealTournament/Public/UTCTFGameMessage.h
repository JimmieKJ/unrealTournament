// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFGameMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTCTFGameMessage : public UUTCarriedObjectMessage
{
	GENERATED_UCLASS_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText ReturnMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText ReturnedMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText CaptureMessagePrefix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText CaptureMessagePostfix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText DroppedMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText HasMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText YouHaveMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText KilledMessagePrefix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText KilledMessagePostfix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText HasAdvantageMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText LosingAdvantageMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText HalftimeMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText OvertimeMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText NoReturnMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText LastLifeMessage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
	FText DefaultPowerupMessage;

	virtual bool InterruptAnnouncement(const FAnnouncementInfo AnnouncementInfo, const FAnnouncementInfo OtherAnnouncementInfo) const override;
	virtual bool CancelByAnnouncement_Implementation(int32 Switch, const UObject* OptionalObject, TSubclassOf<UUTLocalMessage> OtherMessageClass, int32 OtherSwitch, const UObject* OtherOptionalObject) const override;

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override;
	virtual bool ShouldPlayDuringIntermission(int32 MessageIndex) const override;

	virtual FLinearColor GetMessageColor_Implementation(int32 MessageIndex) const override;
	virtual FName GetTeamAnnouncement(int32 Switch, uint8 TeamIndex, const UObject* OptionalObject = nullptr, const class APlayerState* RelatedPlayerState_1 = nullptr, const class APlayerState* RelatedPlayerState_2 = nullptr) const;
	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const override;
	virtual float GetAnnouncementSpacing_Implementation(int32 Switch, const UObject* OptionalObject) const override;
	virtual void PrecacheAnnouncements_Implementation(UUTAnnouncer* Announcer) const override;
	virtual int32 GetFontSizeIndex(int32 MessageIndex) const override;
	virtual float GetScaleInSize_Implementation(int32 MessageIndex) const override;
	virtual float GetAnnouncementPriority(const FAnnouncementInfo AnnouncementInfo) const override;
	virtual void GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override;

	virtual FString GetAnnouncementUMGClassname(int32 Switch, const UObject* OptionalObject) const override;
	virtual float GetLifeTime(int32 Switch) const override;
};


