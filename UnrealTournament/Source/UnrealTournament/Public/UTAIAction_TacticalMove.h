// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAIAction.h"
#include "UTBot.h"

#include "UTAIAction_TacticalMove.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTAIAction_TacticalMove : public UUTAIAction
{
	GENERATED_UCLASS_BODY()

	/** minimum distance bot will strafe in a direction */
	UPROPERTY()
	float MinStrafeDist;

	/** set in Started() to whether enemy is currently visible, so we can return to our current position if our move causes us to lose sight of the enemy */
	bool bEnemyWasVisible;
	/** toggle for strafe direction */
	bool bStrafeDir;
	/** toggle for blocked moves to switch directions */
	bool bChangeDir;
	/** set for final movement (exit action after finished) */
	bool bFinalMove;
	/** set when ideal moves are not available and we forced a quick move to do something */
	bool bForcedDirection;
	/** time bForcedDirection was set */
	float ForcedDirTime;
	/** position that enemy couldn't hit us at */
	FVector HidingSpot;

	virtual void Started() override;
	virtual bool Update(float DeltaTime) override;

	/* Choose a destination for the tactical move, based on aggressiveness and the tactical situation. Make sure destination is reachable
	 * Sets bot's MoveTarget
	 */
	virtual void PickDestination();
	/** attempts to set MoveTarget to a valid location to strafe to along the specified direction
	 * if bForced is true, the function will not fail even if there are obstructions or ledges (it will go as far as it thinks it can)
	 */
	virtual bool EngageDirection(const FVector& StrafeDir, bool bForced);

	/** after bot moves for the last time and enemy is attackable, it shoots for a while stationary and sets a timer for this to end or repeat the action */
	UFUNCTION()
	virtual void FinalWaitFinished();

	/** if enemy gets out of sight, pause and then this is called to move back into view */
	UFUNCTION()
	virtual void StartFinalMove();

	virtual bool AllowWalkOffLedges() override
	{
		return false;
	}
	virtual bool NotifyMoveBlocked(const FHitResult& Impact) override;
	virtual bool NotifyHitLedge() override
	{
		if (bFinalMove)
		{
			GetOuterAUTBot()->ClearMoveTarget();
			return true;
		}
		else
		{
			return NotifyMoveBlocked(FHitResult(NULL, NULL, GetOuterAUTBot()->GetPawn()->GetActorLocation(), -GetOuterAUTBot()->GetPawn()->GetVelocity().GetSafeNormal2D()));
		}
	}
};