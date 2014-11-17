// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAIAction.h"
#include "UTBot.h"

#include "UTAIAction_WaitForLanding.generated.h"

UCLASS(MinimalAPI, CustomConstructor)
class UUTAIAction_WaitForLanding : public UUTAIAction
{
	GENERATED_UCLASS_BODY()

	UUTAIAction_WaitForLanding(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	virtual bool Update(float DeltaTime) override
	{
		return GetOuterAUTBot()->GetCharacter() == NULL || GetOuterAUTBot()->GetCharacter()->GetCharacterMovement()->MovementMode != MOVE_Falling;
	}
};