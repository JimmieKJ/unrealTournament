// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAIAction.h"
#include "UTBot.h"

#include "UTAIAction_WaitForMove.generated.h"

UCLASS(MinimalAPI, CustomConstructor)
class UUTAIAction_WaitForMove : public UUTAIAction
{
	GENERATED_UCLASS_BODY()

	UUTAIAction_WaitForMove(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	float LastReevalEnemyTime;

	virtual bool Update(float DeltaTime) override
	{
		return !GetOuterAUTBot()->GetMoveTarget().IsValid();
	}

	virtual void EnemyNotVisible() override
	{
		if (GetWorld()->TimeSeconds - LastReevalEnemyTime > 1.0f)
		{
			GetOuterAUTBot()->PickNewEnemy();
			LastReevalEnemyTime = GetWorld()->TimeSeconds;
		}
	}
};