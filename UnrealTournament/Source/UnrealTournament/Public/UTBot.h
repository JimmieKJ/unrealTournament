// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AIController.h"
#include "UTRecastNavMesh.h"

#include "UTBot.generated.h"

USTRUCT(BlueprintType)
struct FBotPersonality
{
	GENERATED_USTRUCT_BODY()

	/** overall skill modifier, generally [-1, +1]
	 * NOTE: this is applied to the bot's Skill property and shouldn't be queried directly
	 */
	UPROPERTY(EditAnywhere, Category = Personality)
	float SkillModifier;
	/** aggressiveness (not a modifier) [-1, 1] */
	UPROPERTY(EditAnywhere, Category = Personality)
	float Aggressiveness;
	/** tactical ability (both a modifier for general tactics and a standalone value for advanced or specialized tactics) [-1, 1] */
	UPROPERTY(EditAnywhere, Category = Personality)
	float Tactics;
	/** likelihood of jumping/dodging, particularly in combat */
	UPROPERTY(EditAnywhere, Category = Personality)
	float Jumpiness;
	/** reaction time (skill modifier) [-1, +1], positive is better (lower reaction time)
	 * affects enemy acquisition and incoming fire avoidance
	 */
	UPROPERTY(EditAnywhere, Category = Personality)
	float ReactionTime;
	/** favorite weapon; bot will bias towards acquiring and using this weapon */
	UPROPERTY(EditAnywhere, Category = Personality)
	TSubclassOf<class AUTWeapon> FavoriteWeapon;
};

struct UNREALTOURNAMENT_API FBestInventoryEval : public FUTNodeEvaluator
{
	float BestWeight;
	AActor* BestPickup;

	virtual float Eval(APawn* Asker, const FNavAgentProperties& AgentProps, const UUTPathNode* Node, const FVector& EntryLoc, int32 TotalDistance) override;
	virtual bool GetRouteGoal(AActor*& OutGoal, FVector& OutGoalLoc) const override;

	FBestInventoryEval()
		: BestWeight(0.0f), BestPickup(NULL)
	{}
};
struct UNREALTOURNAMENT_API FRandomDestEval : public FUTNodeEvaluator
{
	virtual float Eval(APawn* Asker, const FNavAgentProperties& AgentProps, const UUTPathNode* Node, const FVector& EntryLoc, int32 TotalDistance) override
	{
		return FMath::FRand() * 1.5f;
	}
};

UCLASS()
class UNREALTOURNAMENT_API AUTBot : public AAIController, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintReadWrite, Category = Personality)
	FBotPersonality Personality;
	/** core skill rating, generally 0 - 7 */
	UPROPERTY(BlueprintReadWrite, Category = Skill)
	float Skill;
	/** reaction time (in real seconds) for enemy positional tracking (i.e. use enemy's position this far in the past as basis) */
	UPROPERTY(BlueprintReadWrite, Category = Skill)
	float TrackingReactionTime;
	/** maximum vision range in UU */
	UPROPERTY(BlueprintReadWrite, Category = Skill)
	float SightRadius;
	/** vision angle (cosine, compared against dot product of target dir to view dir) */
	UPROPERTY(BlueprintReadWrite, Category = Skill)
	float PeripheralVision;
	/** if set bot will have a harder time noticing and targeting enemies that are significantly above or below */
	UPROPERTY(BlueprintReadWrite, Category = Skill)
	bool bSlowerZAcquire;

	/** rotation rate towards focal point */
	UPROPERTY(BlueprintReadOnly, Category = Skill)
	FRotator RotationRate;

	/** whether to call SeePawn() for friendlies */
	UPROPERTY(BlueprintReadWrite, Category = AI)
	bool bSeeFriendly;

private:
	/** current action, if any */
	UPROPERTY()
	class UUTAIAction* CurrentAction;
	/** node or Actor bot is currently moving to */
	UPROPERTY()
	FRouteCacheItem MoveTarget;
protected:
	/** path link that connects currently occupied node to MoveTarget */
	UPROPERTY()
	FUTPathLink CurrentPath;
	/** cache of internal points for current MoveTarget */
	TArray<FComponentBasedPosition> MoveTargetPoints;
	/** last reached MoveTargetPoint in current move (only valid while MoveTarget.IsValid())
	 * this is used when adjusting around obstacles
	 */
	UPROPERTY()
	FVector LastReachedMovePoint;
	/** true when moving to AdjustLoc instead of MoveTarget/MoveTargetPoints */
	UPROPERTY()
	bool bAdjusting;
	/** when bAdjusting is true, temporary intermediate move point that bot uses to get more on path or around minor avoidable obstacles */
	UPROPERTY()
	FVector AdjustLoc;
public:
	inline const FRouteCacheItem& GetMoveTarget() const
	{
		return MoveTarget;
	}
	/** get next point bot is moving to in order to reach MoveTarget, or ZeroVector if no MoveTarget */
	inline FVector GetMovePoint() const
	{
		if (!MoveTarget.IsValid())
		{
			return FVector::ZeroVector;
		}
		else if (bAdjusting)
		{
			return AdjustLoc;
		}
		else if (MoveTargetPoints.Num() <= 1)
		{
			return MoveTarget.GetLocation(GetPawn());
		}
		else
		{
			return MoveTargetPoints[0].Get();
		}
	}
	void SetMoveTarget(const FRouteCacheItem& NewMoveTarget, const TArray<FComponentBasedPosition>& NewMovePoints = TArray<FComponentBasedPosition>());
	/** set move target and force direct move to that point (don't query the navmesh) */
	inline void SetMoveTargetDirect(const FRouteCacheItem& NewMoveTarget)
	{
		TArray<FComponentBasedPosition> NewMovePoints;
		NewMovePoints.Add(FComponentBasedPosition(NewMoveTarget.GetLocation(GetPawn())));
		SetMoveTarget(NewMoveTarget, NewMovePoints);
	}
	inline void ClearMoveTarget()
	{
		MoveTarget.Clear();
		MoveTargetPoints.Empty();
		bAdjusting = false;
		CurrentPath = FUTPathLink();
		MoveTimer = -1.0f;
	}
	inline const FUTPathLink& GetCurrentPath() const
	{
		return CurrentPath;
	}
	inline UUTAIAction* GetCurrentAction() const
	{
		return CurrentAction;
	}
	/** change current action - can be NULL to set no action (bot will call WhatToDoNext() during its tick to try to acquire a new action)
	 * calling with an action in progress will reset it (calls Ended() and then Started())
	 */
	virtual void StartNewAction(UUTAIAction* NewAction);
	/** time remaining until giving up on a move in progress */
	UPROPERTY()
	float MoveTimer;
	/** cache of last found route */
	UPROPERTY()
	TArray<FRouteCacheItem> RouteCache;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = AI)
	virtual void SetEnemy(APawn* NewEnemy);
	inline APawn* GetEnemy() const
	{
		return Enemy;
	}
	/** set target that the bot will shoot instead of Enemy (game objectives and the like) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = AI)
	virtual void SetTarget(AActor* NewTarget);
	inline AActor* GetTarget() const
	{
		return (Target != NULL) ? Target : Enemy;
	}

	/** fire mode bot wants to use for next shot; this is determined early so bot can decide whether to lead, etc */
	UPROPERTY()
	uint8 NextFireMode;

	/** notification of incoming projectile that is reasonably likely to be targeted at this bot
	 * used to prepare evasive actions, if bot sees it coming and its reaction time is good enough
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = AI)
	virtual void ReceiveProjWarning(AUTProjectile* Incoming);
	/** notification of incoming instant hit shot that is reasonably likely to have targeted this bot
	 * used to prepare evasive actions for NEXT shot (kind of late for current shot) if bot is aware enough
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = AI)
	virtual void ReceiveInstantWarning(AUTCharacter* Shooter, const FVector& FireDir);
	/** sets timer for ProcessIncomingWarning() as appropriate for the bot's skill, etc
	 * pass a shooter with NULL for projectile for instant hit warnings
	 */
	virtual void SetWarningTimer(AUTProjectile* Incoming, AUTCharacter* Shooter, float TimeToImpact);
protected:
	/** projectile for ProcessIncomingWarning() call on a pending timer */
	UPROPERTY()
	AUTProjectile* WarningProj;
	/** shooter for ProcessIncomingWarning() call on a pending timer */
	UPROPERTY()
	AUTCharacter* WarningShooter;

	/** called on a timer to react to expected incoming weapons fire, either projectile in flight or about to shoot instant hit */
	UFUNCTION()
	virtual void ProcessIncomingWarning();

private:
	UPROPERTY()
	AUTCharacter* UTChar;
protected:
	/** cached reference to navigation network */
	UPROPERTY()
	AUTRecastNavMesh* NavData;
	/** current enemy, generally also Target unless there's a game objective or other special target */
	UPROPERTY(BlueprintReadOnly, Category = AI)
	APawn* Enemy;
	/** last time Enemy was set, used for acquisition/reaction time calculations */
	UPROPERTY(BlueprintReadOnly, Category = AI)
	float LastEnemyChangeTime;
	/** weapon target, automatically set to Enemy if NULL but can be set to non-Pawn destructible objects (shootable triggers, game objectives, etc) */
	UPROPERTY(BlueprintReadOnly, Category = AI)
	AActor* Target;
	/** set to force selection of new fire mode next frame for weapon targeting */
	bool bPickNewFireMode;

	/** AI actions */
	UPROPERTY()
	TSubobjectPtr<UUTAIAction> WaitForMoveAction;
	UPROPERTY()
	TSubobjectPtr<UUTAIAction> WaitForLandingAction;
	//UPROPERTY()
	//TSubobjectPtr<UUTAIAction> RangedAttackAction;

public:
	inline AUTCharacter* GetUTChar()
	{
		return UTChar;
	}

	/** set when planning on wall dodging next time we hit a wall during current fall */
	bool bPlannedWallDodge;

	virtual void SetPawn(APawn* InPawn) override;
	virtual void Possess(APawn* InPawn) override;
	virtual void Destroyed() override;
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;
	virtual void Tick(float DeltaTime) override;
	virtual void UTNotifyKilled(AController* Killer, AController* KilledPlayer, APawn* KilledPawn, const UDamageType* DamageType);

	virtual uint8 GetTeamNum() const;

	// UTCharacter notifies
	virtual void NotifyWalkingOffLedge();
	virtual void NotifyMoveBlocked(const FHitResult& Impact);
	virtual void NotifyLanded(const FHitResult& Hit);

	// causes the bot decision logic to be run within one frame
	virtual void WhatToDoNext();

	virtual bool CanSee(APawn* Other, bool bMaySkipChecks);
	virtual bool LineOfSightTo(const class AActor* Other, FVector ViewPoint = FVector(ForceInit), bool bAlternateChecks = false) const override;
	virtual void SeePawn(APawn* Other);

	virtual void NotifyTakeHit(AController* InstigatedBy, int32 Damage, FVector Momentum, const FDamageEvent& DamageEvent);

	/** called by timer and also by weapons when ready to fire. Bot sets appropriate fire mode it wants to shoot (if any) */
	virtual void CheckWeaponFiring(bool bFromWeapon = true);
	/** return if bot thinks it needs to turn to attack TargetLoc (intentionally inaccurate for low skill bots) */
	virtual bool NeedToTurn(const FVector& TargetLoc);

	/** find pickup with distance modified rating greater than value passed in
	 * handles avoiding redundant searches in the same frame so it isn't necessary to manually throttle
	 */
	virtual bool FindInventoryGoal(float MinWeight);

	/** tries to perform an evasive action in the indicated direction, most commonly a dodge but if dodge is not available or low skill, possibly strafe that way instead
	 * this function may interrupt the bot's current action
	 */
	virtual bool TryEvasiveAction(FVector DuckDir);

	void SwitchToBestWeapon();
	/** rate the passed in weapon (must be owned by this bot) */
	virtual float RateWeapon(AUTWeapon* W);
	/** returns whether the passed in class is the bot's favorite weapon (bonus to desire to pick up, select, and improved accuracy/effectiveness) */
	virtual bool IsFavoriteWeapon(TSubclassOf<AUTWeapon> TestClass);
	/** returns whether bot really wants to find a better weapon (i.e. because have none with ammo or current is a weak starting weapon) */
	virtual bool NeedsWeapon();

	virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;

protected:
	/** timer to call CheckWeaponFiring() */
	UFUNCTION()
	void CheckWeaponFiringTimed();

	virtual void ExecuteWhatToDoNext();
	bool bPendingWhatToDoNext;
	/** set during ExecuteWhatToDoNext() to catch decision loops */
	bool bExecutingWhatToDoNext;

	/** used to interleave sight checks so not all bots are checking at once */
	float SightCounter;

	/** FindInventoryGoal() transients */
	float LastFindInventoryTime;
	float LastFindInventoryWeight;
};