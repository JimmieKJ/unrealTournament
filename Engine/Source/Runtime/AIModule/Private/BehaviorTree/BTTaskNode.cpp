// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/BTTaskNode.h"

UBTTaskNode::UBTTaskNode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "UnknownTask";
	bNotifyTick = false;
	bNotifyTaskFinished = false;
}

EBTNodeResult::Type UBTTaskNode::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	return EBTNodeResult::Succeeded;
}

EBTNodeResult::Type UBTTaskNode::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	return EBTNodeResult::Aborted;
}

EBTNodeResult::Type UBTTaskNode::WrappedExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const UBTNode* NodeOb = bCreateNodeInstance ? GetNodeInstance(OwnerComp, NodeMemory) : this;
	return NodeOb ? ((UBTTaskNode*)NodeOb)->ExecuteTask(OwnerComp, NodeMemory) : EBTNodeResult::Failed;
}

EBTNodeResult::Type UBTTaskNode::WrappedAbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	const UBTNode* NodeOb = bCreateNodeInstance ? GetNodeInstance(OwnerComp, NodeMemory) : this;
	return NodeOb ? ((UBTTaskNode*)NodeOb)->AbortTask(OwnerComp, NodeMemory) : EBTNodeResult::Aborted;
}

void UBTTaskNode::WrappedTickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) const
{
	if (bNotifyTick)
	{
		const UBTNode* NodeOb = bCreateNodeInstance ? GetNodeInstance(OwnerComp, NodeMemory) : this;
		if (NodeOb)
		{
			((UBTTaskNode*)NodeOb)->TickTask(OwnerComp, NodeMemory, DeltaSeconds);
		}
	}
}

void UBTTaskNode::WrappedOnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) const
{
	if (bNotifyTaskFinished)
	{
		const UBTNode* NodeOb = bCreateNodeInstance ? GetNodeInstance(OwnerComp, NodeMemory) : this;
		if (NodeOb)
		{
			((UBTTaskNode*)NodeOb)->OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
		}
	}
}

void UBTTaskNode::ReceivedMessage(UBrainComponent* BrainComp, const FAIMessage& Message)
{
	UBehaviorTreeComponent* OwnerComp = static_cast<UBehaviorTreeComponent*>(BrainComp);
	check(OwnerComp);
	
	const uint16 InstanceIdx = OwnerComp->FindInstanceContainingNode(this);
	if (OwnerComp->InstanceStack.IsValidIndex(InstanceIdx))
	{
		uint8* NodeMemory = GetNodeMemory<uint8>(OwnerComp->InstanceStack[InstanceIdx]);
		OnMessage(*OwnerComp, NodeMemory, Message.MessageName, Message.RequestID, Message.Status == FAIMessage::Success);
	}
	else
	{
		UE_VLOG(OwnerComp->GetOwner(), LogBehaviorTree, Warning, TEXT("UBTTaskNode::ReceivedMessage called while %s node no longer in active BT")
			, *GetNodeName());
	}
}

void UBTTaskNode::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	// empty in base class
}

void UBTTaskNode::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	// empty in base class
}

void UBTTaskNode::OnMessage(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, FName Message, int32 RequestID, bool bSuccess)
{
	const EBTTaskStatus::Type Status = OwnerComp.GetTaskStatus(this);
	if (Status == EBTTaskStatus::Active)
	{
		FinishLatentTask(OwnerComp, bSuccess ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
	}
	else if (Status == EBTTaskStatus::Aborting)
	{
		FinishLatentAbort(OwnerComp);
	}
}

void UBTTaskNode::FinishLatentTask(UBehaviorTreeComponent& OwnerComp, EBTNodeResult::Type TaskResult) const
{
	// OnTaskFinished must receive valid template node
	UBTTaskNode* TemplateNode = (UBTTaskNode*)OwnerComp.FindTemplateNode(this);
	OwnerComp.OnTaskFinished(TemplateNode, TaskResult);
}

void UBTTaskNode::FinishLatentAbort(UBehaviorTreeComponent& OwnerComp) const
{
	// OnTaskFinished must receive valid template node
	UBTTaskNode* TemplateNode = (UBTTaskNode*)OwnerComp.FindTemplateNode(this);
	OwnerComp.OnTaskFinished(TemplateNode, EBTNodeResult::Aborted);
}

void UBTTaskNode::WaitForMessage(UBehaviorTreeComponent& OwnerComp, FName MessageType) const
{
	// messages delegates should be called on node instances (if they exists)
	OwnerComp.RegisterMessageObserver(this, MessageType);
}

void UBTTaskNode::WaitForMessage(UBehaviorTreeComponent& OwnerComp, FName MessageType, int32 RequestID) const
{
	// messages delegates should be called on node instances (if they exists)
	OwnerComp.RegisterMessageObserver(this, MessageType, RequestID);
}
	
void UBTTaskNode::StopWaitingForMessages(UBehaviorTreeComponent& OwnerComp) const
{
	// messages delegates should be called on node instances (if they exists)
	OwnerComp.UnregisterMessageObserversFrom(this);
}

#if WITH_EDITOR

FName UBTTaskNode::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Task.Icon");
}

#endif	// WITH_EDITOR

//----------------------------------------------------------------------//
// DEPRECATED
//----------------------------------------------------------------------//
EBTNodeResult::Type UBTTaskNode::WrappedExecuteTask(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory) const
{
	return OwnerComp ? WrappedExecuteTask(*OwnerComp, NodeMemory) : EBTNodeResult::Failed;
}
EBTNodeResult::Type UBTTaskNode::WrappedAbortTask(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory) const
{
	return OwnerComp ? WrappedAbortTask(*OwnerComp, NodeMemory) : EBTNodeResult::Failed;
}
void UBTTaskNode::WrappedTickTask(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, float DeltaSeconds) const
{
	if (OwnerComp)
	{
		WrappedTickTask(*OwnerComp, NodeMemory, DeltaSeconds);
	}
}
void UBTTaskNode::WrappedOnTaskFinished(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) const
{
	if (OwnerComp)
	{
		WrappedOnTaskFinished(*OwnerComp, NodeMemory, TaskResult);
	}
}
void UBTTaskNode::FinishLatentTask(UBehaviorTreeComponent* OwnerComp, EBTNodeResult::Type TaskResult) const
{
	if (OwnerComp)
	{
		FinishLatentTask(*OwnerComp, TaskResult);
	}
}
void UBTTaskNode::FinishLatentAbort(UBehaviorTreeComponent* OwnerComp) const
{
	if (OwnerComp)
	{
		FinishLatentAbort(*OwnerComp);
	}
}
EBTNodeResult::Type UBTTaskNode::ExecuteTask(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory)
{
	if (OwnerComp)
	{
		return ExecuteTask(*OwnerComp, NodeMemory);
	}
	return EBTNodeResult::Failed;
}
EBTNodeResult::Type UBTTaskNode::AbortTask(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory)
{
	if (OwnerComp)
	{
		return AbortTask(*OwnerComp, NodeMemory);
	}
	return EBTNodeResult::Failed;
}
void UBTTaskNode::TickTask(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (OwnerComp)
	{
		TickTask(*OwnerComp, NodeMemory, DeltaSeconds);
	}
}
void UBTTaskNode::OnMessage(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, FName Message, int32 RequestID, bool bSuccess)
{
	if (OwnerComp)
	{
		OnMessage(*OwnerComp, NodeMemory, Message, RequestID, bSuccess);
	}
}
void UBTTaskNode::OnTaskFinished(UBehaviorTreeComponent* OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	if (OwnerComp)
	{
		OnTaskFinished(*OwnerComp, NodeMemory, TaskResult);
	}
}
void UBTTaskNode::WaitForMessage(UBehaviorTreeComponent* OwnerComp, FName MessageType) const
{
	if (OwnerComp)
	{
		WaitForMessage(*OwnerComp, MessageType);
	}
}
void UBTTaskNode::WaitForMessage(UBehaviorTreeComponent* OwnerComp, FName MessageType, int32 RequestID) const
{
	if (OwnerComp)
	{
		WaitForMessage(*OwnerComp, MessageType, RequestID);
	}
}