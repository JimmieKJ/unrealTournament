// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTFlagRunGameMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTFlagRunGameMessage : public UUTCTFGameMessage
{
	GENERATED_UCLASS_BODY()

	virtual FName GetTeamAnnouncement(int32 Switch, uint8 TeamIndex, const UObject* OptionalObject = nullptr, const class APlayerState* RelatedPlayerState_1 = nullptr, const class APlayerState* RelatedPlayerState_2 = nullptr) const override;
	virtual float GetAnnouncementSpacing_Implementation(int32 Switch, const UObject* OptionalObject) const override;
};
