// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AITestSuitePrivatePCH.h"
#include "MockAI_BT.h"

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
TArray<int32> UMockAI_BT::ExecutionLog;

UMockAI_BT::UMockAI_BT(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		UseBlackboardComponent();
		UseBrainComponent<UBehaviorTreeComponent>();

		BTComp = Cast<UBehaviorTreeComponent>(BrainComp);
	}
}

bool UMockAI_BT::IsRunning() const
{
	return BTComp && BTComp->IsRunning() && BTComp->GetRootTree();
}

void UMockAI_BT::RunBT(UBehaviorTree& BTAsset, EBTExecutionMode::Type RunType)
{
	if (BTAsset.BlackboardAsset)
	{
		BBComp->InitializeBlackboard(*BTAsset.BlackboardAsset);
	}
	BBComp->CacheBrainComponent(*BTComp);
	BTComp->CacheBlackboardComponent(BBComp);

	UWorld* World = FAITestHelpers::GetWorld();

	BBComp->RegisterComponentWithWorld(World);
	BTComp->RegisterComponentWithWorld(World);

	BTComp->StartTree(BTAsset, RunType);
}
