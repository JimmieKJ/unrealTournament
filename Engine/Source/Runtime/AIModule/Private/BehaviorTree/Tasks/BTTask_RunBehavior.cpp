// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Tasks/BTTask_RunBehavior.h"

UBTTask_RunBehavior::UBTTask_RunBehavior(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Run Behavior";
}

EBTNodeResult::Type UBTTask_RunBehavior::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UE_CVLOG(BehaviorAsset == nullptr, OwnerComp.GetAIOwner(), LogBehaviorTree, Error, TEXT("\'%s\' is missing BehaviorAsset!"), *GetNodeName());

	const bool bPushed = BehaviorAsset != nullptr && OwnerComp.PushInstance(*BehaviorAsset);
	return bPushed ? EBTNodeResult::InProgress : EBTNodeResult::Failed;
}

FString UBTTask_RunBehavior::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s: %s"), *Super::GetStaticDescription(), *GetNameSafe(BehaviorAsset));
}

#if WITH_EDITOR

FName UBTTask_RunBehavior::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Task.RunBehavior.Icon");
}

#endif	// WITH_EDITOR
