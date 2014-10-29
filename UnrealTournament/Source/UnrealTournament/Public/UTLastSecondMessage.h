// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAnnouncer.h"

#include "UTLastSecondMessage.generated.h"

UCLASS()
class UUTLastSecondMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const override;
	virtual void ClientReceive(const FClientReceiveData& ClientData) const override;
};
