// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAIAction.h"
#include "UTBot.h"

#include "UTAIAction_RangedAttack.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTAIAction_RangedAttack : public UUTAIAction
{
	GENERATED_UCLASS_BODY()

	/** if the bot is skilled enough, finds another spot that the bot can also shoot the target from and sets MoveTarget to it
	 * @return whether a valid MoveTarget was found
	 */
	virtual bool FindStrafeDest();

	virtual void Started() override;
	virtual void Ended(bool bAborted) override;
	virtual bool Update(float DeltaTime) override;

	UFUNCTION()
	virtual void FirstShotTimer();
	UFUNCTION()
	virtual void EndTimer();

	virtual void EnemyNotVisible() override
	{
		AUTWeapon* MyWeap = (GetUTChar() != NULL) ? GetUTChar()->GetWeapon() : NULL;
		bool bCanTryIndirect = (MyWeap != NULL && (GetOuterAUTBot()->Skill >= 2.0f || GetOuterAUTBot()->IsFavoriteWeapon(MyWeap->GetClass())));
		// abort if can't attack predicted enemy loc or enemy info is too outdated to trust its accuracy
		if (GetTarget() == GetEnemy() && /*!Pawn.RecommendLongRangedAttack() &&*/ (GetWorld()->TimeSeconds - GetOuterAUTBot()->GetEnemyInfo(GetEnemy(), true)->LastFullUpdateTime > 1.0f || MyWeap == NULL || !MyWeap->CanAttack(GetEnemy(), GetOuterAUTBot()->GetEnemyLocation(GetEnemy(), true), bCanTryIndirect)))
		{
			GetOuterAUTBot()->WhatToDoNext();
		}
	}
};