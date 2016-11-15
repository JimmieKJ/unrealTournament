// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTSquadAI.h"

#include "UTAsymCTFSquadAI.generated.h"

UCLASS()
class AUTAsymCTFSquadAI : public AUTSquadAI
{
	GENERATED_BODY()
public:

	// the single flag that must be capped
	UPROPERTY()
	AUTCarriedObject* Flag;
	// cached distance from flag start point to capture point
	UPROPERTY()
	float TotalFlagRunDistance;
	// cached PathNode that the flag was most recently at, used for defender flag approach pathing
	UPROPERTY()
	const UUTPathNode* LastFlagNode;

	/** whether we're on the attacking team currently
	 * note: this has nothing to do with squad orders (can be on attacking team and have orders to 'defend' which in this context means the flag carrier)
	 */
	bool IsAttackingTeam() const;

	virtual void Initialize(AUTTeamInfo* InTeam, FName InOrders) override;
	virtual bool MustKeepEnemy(APawn* TheEnemy) override;
	virtual bool ShouldUseTranslocator(AUTBot* B) override;
	virtual float ModifyEnemyRating(float CurrentRating, const FBotEnemyInfo& EnemyInfo, AUTBot* B) override;
	virtual bool CheckSquadObjectives(AUTBot* B) override;
	virtual int32 GetDefensePointPriority(AUTBot* B, class AUTDefensePoint* Point);

	/** return action for flag carrier to take */
	virtual bool SetFlagCarrierAction(AUTBot* B);
	/** tell bot best action to recover friendly flag (assumed not at home base) */
	virtual bool HuntEnemyFlag(AUTBot* B);

	virtual void NotifyObjectiveEvent(AActor* InObjective, AController* InstigatedBy, FName EventName) override;
	virtual bool HasHighPriorityObjective(AUTBot* B);

	virtual bool TryPathTowardObjective(AUTBot* B, AActor* Goal, bool bAllowDetours, const FString& SuccessGoalString) override;

	virtual void GetPossibleEnemyGoals(AUTBot* B, const FBotEnemyInfo* EnemyInfo, TArray<FPredictedGoal>& Goals) override;
};