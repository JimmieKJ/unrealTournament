// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_RunBehavior.generated.h"

/**
 * RunBehavior task allows pushing subtrees on execution stack.
 * Subtree asset can't be changed in runtime! 
 *
 * This limitation is caused by support for subtree's root level decorators,
 * which are injected into parent tree, and structure of running tree
 * cannot be modified in runtime (see: BTNode: ExecutionIndex, MemoryOffset)
 *
 * Use RunBehaviorDynamic task for subtrees that need to be changed in runtime.
 */

UCLASS()
class AIMODULE_API UBTTask_RunBehavior : public UBTTaskNode
{
	GENERATED_UCLASS_BODY()

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual void CleanupMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryClear::Type CleanupType) const override;
	virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR

	/** called on instance startup, prepares root level nodes to use */
	void InjectNodes(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, int32& InstancedIndex) const;

	/** @returns number of injected nodes */
	int32 GetInjectedNodesCount() const;

	/** @returns subtree asset */
	UBehaviorTree* GetSubtreeAsset() const;

protected:

	/** behavior to run */
	UPROPERTY(Category = Node, EditAnywhere)
	UBehaviorTree* BehaviorAsset;
};

//////////////////////////////////////////////////////////////////////////
// Inlines

FORCEINLINE UBehaviorTree* UBTTask_RunBehavior::GetSubtreeAsset() const
{
	return BehaviorAsset;
}

FORCEINLINE int32 UBTTask_RunBehavior::GetInjectedNodesCount() const
{
	return BehaviorAsset ? BehaviorAsset->RootDecorators.Num() : 0;
}
