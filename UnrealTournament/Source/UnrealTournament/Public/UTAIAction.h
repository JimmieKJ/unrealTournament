// a discrete action performed by AI, such as "hunt player" or "grab flag"
// replacement for what were AI states in UT3 AI code
// only one action may be in use at a time and an action is generally active until it returns true from Update() or is explicitly aborted/replaced by outside code
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTBot.h"

#include "UTAIAction.generated.h"

UCLASS(NotPlaceable, CustomConstructor, Within = UTBot)
class UNREALTOURNAMENT_API UUTAIAction : public UObject
{
	GENERATED_UCLASS_BODY()

	UUTAIAction(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	virtual UWorld* GetWorld() const override
	{
		return GetOuterAUTBot()->GetWorld();
	}

	/** ticks the action; returns true on completion */
	virtual bool Update(float DeltaTime) PURE_VIRTUAL(UUTAIAction::Update, return true;);

	/** called when the action is assigned to the AI */
	virtual void Started()
	{}
	/** called when the action is stopped */
	virtual void Ended(bool bAborted)
	{
		GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	}
};