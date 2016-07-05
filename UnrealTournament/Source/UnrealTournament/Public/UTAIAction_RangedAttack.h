// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAIAction.h"
#include "UTBot.h"

#include "UTAIAction_RangedAttack.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTAIAction_RangedAttack : public UUTAIAction
{
	GENERATED_UCLASS_BODY()

	/** set if ranged attack target is bot's enemy and it was attackable (directly or indirectly) from our position at the start of the action.
	 * generally the action gets aborted if this was true and becomes false
	 * unused when attacking non-Enemy targets
	 */
	bool bEnemyWasAttackable;

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

	/** true only if Target == Enemy and it can be directly or indirectly attacked from the bot's current position */
	bool IsEnemyAttackable() const
	{
		AUTWeapon* MyWeap = (GetUTChar() != NULL) ? GetUTChar()->GetWeapon() : NULL;
		bool bCanTryIndirect = (MyWeap != NULL && (GetOuterAUTBot()->Skill >= 2.0f || GetOuterAUTBot()->IsFavoriteWeapon(MyWeap->GetClass())));
		// abort if can't attack predicted enemy loc or enemy info is too outdated to trust its accuracy
		return (GetTarget() == GetEnemy() && /*!Pawn.RecommendLongRangedAttack() &&*/ (GetWorld()->TimeSeconds - GetOuterAUTBot()->GetEnemyInfo(GetEnemy(), true)->LastFullUpdateTime > 1.0f || MyWeap == NULL || !MyWeap->CanAttack(GetEnemy(), GetOuterAUTBot()->GetEnemyLocation(GetEnemy(), true), bCanTryIndirect)));
	}

	virtual void EnemyNotVisible() override
	{
		if (bEnemyWasAttackable && !IsEnemyAttackable())
		{
			GetOuterAUTBot()->WhatToDoNext();
		}
	}

	virtual bool SetFocusForNoTarget() override
	{
		if (GetOuterAUTBot()->IsStopped())
		{
			GetOuterAUTBot()->SetFocus(GetEnemy());
			return true;
		}
		else
		{
			return false;
		}
	}
};