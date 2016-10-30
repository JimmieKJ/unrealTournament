// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTLocalMessage.h"
#include "UTFlagRunMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTFlagRunMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override;
	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const override;
};



