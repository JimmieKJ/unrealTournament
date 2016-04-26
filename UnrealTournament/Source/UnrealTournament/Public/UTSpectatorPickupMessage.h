// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTLocalMessage.h"
#include "UTSpectatorPickupMessage.generated.h"

/**
 * 
 */
UCLASS()
class UNREALTOURNAMENT_API UUTSpectatorPickupMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()
	
	virtual FText ResolveMessage_Implementation(int32 Switch, bool bTargetsPlayerState1, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) const override;

private:

	FText PickupFormat;
};
