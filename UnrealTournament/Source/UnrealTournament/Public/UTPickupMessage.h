// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTPickupMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTPickupMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	virtual FText ResolveMessage_Implementation(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const override;
	virtual void ClientReceive(const FClientReceiveData& ClientData) const override;
	virtual bool UseLargeFont(int32 MessageIndex) const override;
};