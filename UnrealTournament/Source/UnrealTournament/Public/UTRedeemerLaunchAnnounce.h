// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAnnouncer.h"
#include "UTDamageType.h"

#include "UTRedeemerLaunchAnnounce.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTRedeemerLaunchAnnounce : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const override;
	virtual void PrecacheAnnouncements_Implementation(class UUTAnnouncer* Announcer) const override;
	virtual bool ShouldPlayAnnouncement(const FClientReceiveData& ClientData) const override;
};
