// a discrete action performed by AI, such as "hunt player" or "grab flag"
// replacement for what were AI states in UT3 AI code
// only one action may be in use at a time and an action is generally active until it returns true from Update() or is explicitly aborted/replaced by outside code
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTBot.h"

#include "UTAIAction.generated.h"

UCLASS(NotPlaceable, Abstract, CustomConstructor, Within = UTBot)
class UNREALTOURNAMENT_API UUTAIAction : public UObject
{
	GENERATED_UCLASS_BODY()

	UUTAIAction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	virtual UWorld* GetWorld() const override
	{
		return GetOuterAUTBot()->GetWorld();
	}
	
	// convenience accessors
	inline APawn* GetPawn() const
	{
		return GetOuterAUTBot()->GetPawn();
	}
	inline ACharacter* GetCharacter() const
	{
		return GetOuterAUTBot()->GetCharacter();
	}
	inline AUTCharacter* GetUTChar() const
	{
		return GetOuterAUTBot()->GetUTChar();
	}
	inline AUTSquadAI* GetSquad() const
	{
		return GetOuterAUTBot()->GetSquad();
	}
	inline APawn* GetEnemy() const
	{
		return GetOuterAUTBot()->GetEnemy();
	}
	inline AActor* GetTarget() const
	{
		return GetOuterAUTBot()->GetTarget();
	}
	inline AUTWeapon* GetWeapon() const
	{
		return (GetUTChar() != NULL) ? GetUTChar()->GetWeapon() : NULL;
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

	/** return whether or not the bot can fall (or jump) off ledges during this action */
	virtual bool AllowWalkOffLedges()
	{
		return true;
	}

	/** notification of Pawn's move being blocked by a wall - return true to override default handling */
	virtual bool NotifyMoveBlocked(const FHitResult& Impact)
	{
		return false;
	}

	/** called repeatedly while enemy is valid but not visible; combat actions often react to this by changing position, aborting, etc */
	virtual void EnemyNotVisible()
	{}

	/** called to optionally set bot's focus when no target/enemy to look at */
	virtual bool SetFocusForNoTarget()
	{
		return false;
	}
};