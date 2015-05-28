// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "AIController.h"
#include "Actions/PawnActionsComponent.h"
#include "BehaviorTree/Tasks/BTTask_PushPawnAction.h"

UBTTask_PushPawnAction::UBTTask_PushPawnAction(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Push PawnAction";
}

EBTNodeResult::Type UBTTask_PushPawnAction::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UPawnAction* ActionCopy = Action ? DuplicateObject<UPawnAction>(Action, &OwnerComp) : nullptr;
	if (ActionCopy == nullptr)
	{
		return EBTNodeResult::Failed;
	}

	return PushAction(OwnerComp, *ActionCopy);
}

FString UBTTask_PushPawnAction::GetStaticDescription() const
{
	//return FString::Printf(TEXT("Push Action: %s"), Action ? *Action->GetDisplayName() : TEXT("None"));
	return FString::Printf(TEXT("Push Action: %s"), *GetNameSafe(Action));
}
