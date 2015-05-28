// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Decorators/BTDecorator_TimeLimit.h"

UBTDecorator_TimeLimit::UBTDecorator_TimeLimit(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "TimeLimit";
	TimeLimit = 5.0f;
	bNotifyBecomeRelevant = true;
	bNotifyTick = true;
	bTickIntervals = true;

	// time limit always abort current branch
	bAllowAbortLowerPri = false;
	bAllowAbortNone = false;
	FlowAbortMode = EBTFlowAbortMode::Self;
}

void UBTDecorator_TimeLimit::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTAuxiliaryMemory* DecoratorMemory = GetSpecialNodeMemory<FBTAuxiliaryMemory>(NodeMemory);
	DecoratorMemory->NextTickRemainingTime = TimeLimit;
	DecoratorMemory->AccumulatedDeltaTime = 0.0f;
}

void UBTDecorator_TimeLimit::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	OwnerComp.RequestExecution(this);
}

FString UBTDecorator_TimeLimit::GetStaticDescription() const
{
	// basic info: result after time
	return FString::Printf(TEXT("%s: %s after %.1fs"), *Super::GetStaticDescription(),
		*UBehaviorTreeTypes::DescribeNodeResult(EBTNodeResult::Failed), TimeLimit);
}

void UBTDecorator_TimeLimit::DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const
{
	Super::DescribeRuntimeValues(OwnerComp, NodeMemory, Verbosity, Values);

	FBTAuxiliaryMemory* DecoratorMemory = GetSpecialNodeMemory<FBTAuxiliaryMemory>(NodeMemory);
	if (OwnerComp.GetWorld())
	{
		const float TimeLeft = DecoratorMemory->NextTickRemainingTime > 0 ? DecoratorMemory->NextTickRemainingTime : 0;
		Values.Add(FString::Printf(TEXT("%s in %ss"),
			*UBehaviorTreeTypes::DescribeNodeResult(EBTNodeResult::Failed),
			*FString::SanitizeFloat(TimeLeft)));
	}
}

#if WITH_EDITOR

FName UBTDecorator_TimeLimit::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Decorator.TimeLimit.Icon");
}

#endif	// WITH_EDITOR