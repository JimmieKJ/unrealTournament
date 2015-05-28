// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "AIController.h"
#include "Actions/PawnActionsComponent.h"
#include "BehaviorTree/Tasks/BTTask_PawnActionBase.h"

UBTTask_PawnActionBase::UBTTask_PawnActionBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Action Base";
}

EBTNodeResult::Type UBTTask_PawnActionBase::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	return (OwnerComp.GetAIOwner() != nullptr
		&& OwnerComp.GetAIOwner()->GetActionsComp() != nullptr
		&& OwnerComp.GetAIOwner()->GetActionsComp()->AbortActionsInstigatedBy(this, EAIRequestPriority::Logic) > 0) 
		? EBTNodeResult::InProgress : EBTNodeResult::Aborted;
}

EBTNodeResult::Type UBTTask_PawnActionBase::PushAction(UBehaviorTreeComponent& OwnerComp, UPawnAction& Action)
{
	EBTNodeResult::Type NodeResult = EBTNodeResult::Failed;

	AAIController* AIOwner = Cast<AAIController>(OwnerComp.GetOwner());
	if (AIOwner)
	{
		if (Action.HasActionObserver())
		{
			UE_VLOG(AIOwner, LogBehaviorTree, Warning, TEXT("Action %s already had an observer, it will be overwritten!"), *Action.GetName());
		}

		Action.SetActionObserver(FPawnActionEventDelegate::CreateUObject(this, &UBTTask_PawnActionBase::OnActionEvent));

		const bool bResult = AIOwner->PerformAction(Action, EAIRequestPriority::Logic, this);		
		if (bResult)
		{
			// don't bother with handling action events, it will be processed in next tick
			NodeResult = EBTNodeResult::InProgress;
		}
	}

	return NodeResult;
}

void UBTTask_PawnActionBase::OnActionEvent(UPawnAction& Action, EPawnActionEventType::Type Event)
{
	ActionEventHandler(this, Action, Event);
}

void UBTTask_PawnActionBase::ActionEventHandler(UBTTaskNode* TaskNode, UPawnAction& Action, EPawnActionEventType::Type Event)
{
	AAIController* AIOwner = Cast<AAIController>(Action.GetController());
	UBehaviorTreeComponent* OwnerComp = AIOwner ? Cast<UBehaviorTreeComponent>(AIOwner->BrainComponent) : nullptr;
	if (OwnerComp == nullptr)
	{
		UE_VLOG(Action.GetController(), LogBehaviorTree, Warning, TEXT("%s: Unhandled PawnAction event: %s, can't find owning component!"),
			*UBehaviorTreeTypes::DescribeNodeHelper(TaskNode),
			*UPawnActionsComponent::DescribeEventType(Event));

		return;
	}

	const EBTTaskStatus::Type TaskStatus = OwnerComp->GetTaskStatus(TaskNode);
	bool bHandled = false;

	if (TaskStatus == EBTTaskStatus::Active)
	{
		if (Event == EPawnActionEventType::FinishedExecution || Event == EPawnActionEventType::FailedToStart)
		{
			const bool bSucceeded = (Action.GetResult() == EPawnActionResult::Success);
			TaskNode->FinishLatentTask(*OwnerComp, bSucceeded ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
			
			bHandled = true;
		}
	}
	else if (TaskStatus == EBTTaskStatus::Aborting)
	{
		if (Event == EPawnActionEventType::FinishedAborting ||
			Event == EPawnActionEventType::FinishedExecution || Event == EPawnActionEventType::FailedToStart)
		{
			TaskNode->FinishLatentAbort(*OwnerComp);

			bHandled = true;
		}
	}

	if (!bHandled)
	{
		UE_VLOG(Action.GetController(), LogBehaviorTree, Verbose, TEXT("%s: Unhandled PawnAction event: %s TaskStatus: %s"),
			*UBehaviorTreeTypes::DescribeNodeHelper(TaskNode),
			*UPawnActionsComponent::DescribeEventType(Event),
			*UBehaviorTreeTypes::DescribeTaskStatus(TaskStatus));
	}
}
