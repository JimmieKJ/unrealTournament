// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTSquadAI.h"

#include "UTAsymCTFSquadAI.generated.h"

UCLASS()
class AUTAsymCTFSquadAI : public AUTSquadAI
{
	GENERATED_BODY()
public:
	AUTAsymCTFSquadAI(const FObjectInitializer& OI)
		: Super(OI)
	{
		LastRallyTime = -100000.0f;
	}

	// the single flag that must be capped
	UPROPERTY()
	AUTCarriedObject* Flag;
	// cached distance from flag start point to capture point
	UPROPERTY()
	float TotalFlagRunDistance;
	// cached distance from capture point to furthest defense point
	UPROPERTY()
	float FurthestDefensePointDistance;
	// cached PathNode that the flag was most recently at, used for flag approach pathing
	UPROPERTY()
	const UUTPathNode* LastFlagNode;
	// alternate routes for attacker team flag approach (since default list is used for trying to score once holding flag)
	UPROPERTY()
	TArray<FAlternateRoute> FlagRetrievalRoutes;

	/** set when flag carrier wants to rally (used to avoid decision oscillation when on the edge of various thresholds) */
	UPROPERTY()
	bool bWantRally;
	/** time of last successful rally */
	UPROPERTY()
	float LastRallyTime;

	/** whether we're on the attacking team currently
	 * note: this has nothing to do with squad orders (can be on attacking team and have orders to 'defend' which in this context means the flag carrier)
	 */
	bool IsAttackingTeam() const;

	virtual void Initialize(AUTTeamInfo* InTeam, FName InOrders) override;
	virtual bool MustKeepEnemy(AUTBot* B, APawn* TheEnemy) override;
	virtual void ModifyAggression(AUTBot* B, float& Aggressiveness) override;
	virtual bool ShouldUseTranslocator(AUTBot* B) override;
	virtual float ModifyEnemyRating(float CurrentRating, const FBotEnemyInfo& EnemyInfo, AUTBot* B) override;
	virtual bool CheckSquadObjectives(AUTBot* B) override;
	virtual int32 GetDefensePointPriority(AUTBot* B, class AUTDefensePoint* Point);

	/** use given alternate routes to attempt to reach flag; handles adjusting route for flag incremental return behavior */
	virtual bool TryPathToFlag(AUTBot* B, TArray<FAlternateRoute>& Routes, const FString& SuccessGoalString);
	/** return whether flag carrier should go for a rally point instead of the enemy base */
	virtual bool ShouldStartRally(AUTBot* B);
	/** return action for flag carrier to take */
	virtual bool SetFlagCarrierAction(AUTBot* B);
	/** tell bot best action to recover friendly flag (assumed not at home base) */
	virtual bool HuntEnemyFlag(AUTBot* B);

	virtual void NotifyObjectiveEvent(AActor* InObjective, AController* InstigatedBy, FName EventName) override;
	virtual bool HasHighPriorityObjective(AUTBot* B);

	virtual bool TryPathTowardObjective(AUTBot* B, AActor* Goal, bool bAllowDetours, const FString& SuccessGoalString) override;

	virtual void GetPossibleEnemyGoals(AUTBot* B, const FBotEnemyInfo* EnemyInfo, TArray<FPredictedGoal>& Goals) override;
};