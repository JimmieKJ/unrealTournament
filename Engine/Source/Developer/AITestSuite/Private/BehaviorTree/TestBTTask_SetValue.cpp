// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AITestSuitePrivatePCH.h"
#include "BehaviorTree/TestBTTask_SetValue.h"
#include "BehaviorTree/BlackboardComponent.h"

UTestBTTask_SetValue::UTestBTTask_SetValue(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "SetValue";
	TaskResult = EBTNodeResult::Succeeded;
	KeyName = TEXT("Int");
	Value = 1;
}

EBTNodeResult::Type UTestBTTask_SetValue::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	OwnerComp.GetBlackboardComponent()->SetValue<UBlackboardKeyType_Int>(KeyName, Value);
	return TaskResult;
}
