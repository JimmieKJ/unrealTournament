// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAIAction.h"
#include "UTBot.h"

#include "UTAIAction_WaitForMove.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTAIAction_WaitForMove : public UUTAIAction
{
	GENERATED_UCLASS_BODY()

	UUTAIAction_WaitForMove(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
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