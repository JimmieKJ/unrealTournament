// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Tasks/BTTask_MakeNoise.h"

UBTTask_MakeNoise::UBTTask_MakeNoise(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
	, Loudnes(1.0f)
{
	NodeName = "MakeNoise";
}

EBTNodeResult::Type UBTTask_MakeNoise::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AController* MyController = Cast<AController>(OwnerComp.GetOwner());
	APawn* MyPawn = MyController ? MyController->GetPawn() : NULL;

	if (MyPawn)
	{
		MyPawn->MakeNoise(Loudnes, MyPawn);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}
	
#if WITH_EDITOR

FName UBTTask_MakeNoise::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Task.MakeNoise.Icon");
}

#endif	// WITH_EDITOR