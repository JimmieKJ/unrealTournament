// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAnnouncer.h"

#include "UTPowerupUseMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTPowerupUseMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override;
	virtual void GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override;

	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const override;
	virtual void PrecacheAnnouncements_Implementation(class UUTAnnouncer* Announcer) const override;
	virtual bool ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const override;
	virtual float GetAnnouncementDelay(int32 Switch) override;
	virtual bool InterruptAnnouncement(const FAnnouncementInfo AnnouncementInfo, const FAnnouncementInfo OtherAnnouncementInfo) const override;
	virtual float GetAnnouncementPriority(const FAnnouncementInfo AnnouncementInfo) const override;
};
