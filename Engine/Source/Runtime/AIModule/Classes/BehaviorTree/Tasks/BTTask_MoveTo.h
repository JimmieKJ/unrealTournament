// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_MoveTo.generated.h"

class UNavigationQueryFilter;

struct FBTMoveToTaskMemory
{
	/** Move request ID */
	FAIRequestID MoveRequestID;

	uint8 bWaitingForPath : 1;
};

/**
 * Move To task node.
 * Moves the AI pawn toward the specified Actor or Location blackboard entry using the navigation system.
 */
UCLASS(config=Game)
class AIMODULE_API UBTTask_MoveTo : public UBTTask_BlackboardBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(config, Category=Node, EditAnywhere, meta=(ClampMin = "0.0"))
	float AcceptableRadius;

	/** "None" will result in default filter being used */
	UPROPERTY(Category=Node, EditAnywhere)
	TSubclassOf<UNavigationQueryFilter> FilterClass;

	UPROPERTY(Category=Node, EditAnywhere)
	uint32 bAllowStrafe : 1;
	/** if set, use incomplete path when goal can't be reached */
	UPROPERTY(Category = Node, EditAnywhere, AdvancedDisplay)
	uint32 bAllowPartialPath : 1;

	/** if set to true agent's radius will be added to AcceptableRadius for purposes of checking 
	 *	if path's end point has been reached. Will result in AI stopping on contact with destination location*/
	UPROPERTY(Category=Node, EditAnywhere, config)
	uint32 bStopOnOverlap : 1;
	
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override;

	virtual void OnMessage(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, FName Message, int32 RequestID, bool bSuccess) override;

	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
	virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR

protected:

	EBTNodeResult::Type PerformMoveTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory);
};
