// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_Loop.generated.h"

struct FBTLoopDecoratorMemory
{
	int32 SearchId;
	uint8 RemainingExecutions;
};

/**
 * Loop decorator node.
 * A decorator node that bases its condition on whether its loop counter has been exceeded.
 */
UCLASS(HideCategories=(Condition))
class AIMODULE_API UBTDecorator_Loop : public UBTDecorator
{
	GENERATED_UCLASS_BODY()

	/** number of executions */
	UPROPERTY(Category=Decorator, EditAnywhere)
	int32 NumLoops;

	/** infinite loop */
	UPROPERTY(Category=Decorator, EditAnywhere)
	bool bInfiniteLoop;

	virtual uint16 GetInstanceMemorySize() const override;
	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
	virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR

protected:

	virtual void OnNodeActivation(FBehaviorTreeSearchData& SearchData) override;
};
