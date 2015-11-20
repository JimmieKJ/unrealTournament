// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_MoveDirectlyToward.generated.h"

struct FBTMoveDirectlyTowardMemory
{
	/** Move request ID */
	FAIRequestID MoveRequestID;
};

/**
 * Move Directly Toward task node.
 * Moves the AI pawn toward the specified Actor or Location (Vector) blackboard entry in a straight line, without regard to any navigation system. If you need the AI to navigate, use the "Move To" node instead.
 */
UCLASS(config=Game)
class AIMODULE_API UBTTask_MoveDirectlyToward : public UBTTask_BlackboardBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(config, Category=Node, EditAnywhere, meta=(ClampMin = "0.0"))
	float AcceptableRadius;

	/** set to true will result in not updating move destination in 
	 *	case where goal is an Actor that can change 
	 *	his location while task is being performed */
	UPROPERTY(Category=Node, EditAnywhere)
	uint32 bDisablePathUpdateOnGoalLocationChange : 1;

	UPROPERTY(Category=Node, EditAnywhere)
	uint32 bProjectVectorGoalToNavigation:1;

	UPROPERTY(Category=Node, EditAnywhere)
	uint32 bAllowStrafe : 1;

	/** if set to true agent's radius will be added to AcceptableRadius for purposes of checking
	 *	if path's end point has been reached. Will result in AI stopping on contact with destination location*/
	UPROPERTY(Category = Node, EditAnywhere)
	uint32 bStopOnOverlap : 1;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
	virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR
};
