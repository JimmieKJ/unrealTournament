// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"

UBTTask_MoveTo::UBTTask_MoveTo(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
	, AcceptableRadius(50.f)
	, bAllowStrafe(false)
	, bAllowPartialPath(true)
	, bStopOnOverlap(true)
{
	NodeName = "Move To";
	bNotifyTick = true;

	// accept only actors and vectors
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MoveTo, BlackboardKey), AActor::StaticClass());
	BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MoveTo, BlackboardKey));
}

EBTNodeResult::Type UBTTask_MoveTo::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	EBTNodeResult::Type NodeResult = EBTNodeResult::InProgress;

	FBTMoveToTaskMemory* MyMemory = (FBTMoveToTaskMemory*)NodeMemory;
	AAIController* MyController = OwnerComp.GetAIOwner();

	MyMemory->bWaitingForPath = MyController->ShouldPostponePathUpdates();
	if (!MyMemory->bWaitingForPath)
	{
		NodeResult = PerformMoveTask(OwnerComp, NodeMemory);
	}
	else
	{
		UE_VLOG(MyController, LogBehaviorTree, Log, TEXT("Pathfinding requests are freezed, waiting..."));
	}
	
	return NodeResult;
}

EBTNodeResult::Type UBTTask_MoveTo::PerformMoveTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const UBlackboardComponent* MyBlackboard = OwnerComp.GetBlackboardComponent();
	FBTMoveToTaskMemory* MyMemory = (FBTMoveToTaskMemory*)NodeMemory;
	AAIController* MyController = OwnerComp.GetAIOwner();

	EBTNodeResult::Type NodeResult = EBTNodeResult::Failed;

	if (MyController && MyBlackboard)
	{
		EPathFollowingRequestResult::Type RequestResult = EPathFollowingRequestResult::Failed;
		
		FAIMoveRequest MoveReq;
		MoveReq.SetNavigationFilter(FilterClass);
		MoveReq.SetAllowPartialPath(bAllowPartialPath);
		MoveReq.SetAcceptanceRadius(AcceptableRadius);
		MoveReq.SetCanStrafe(bAllowStrafe);
		MoveReq.SetStopOnOverlap(bStopOnOverlap);

		if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass())
		{
			UObject* KeyValue = MyBlackboard->GetValue<UBlackboardKeyType_Object>(BlackboardKey.GetSelectedKeyID());
			AActor* TargetActor = Cast<AActor>(KeyValue);
			if (TargetActor)
			{
				MoveReq.SetGoalActor(TargetActor);

				RequestResult = MyController->MoveTo(MoveReq);
			}
			else
			{
				UE_VLOG(MyController, LogBehaviorTree, Warning, TEXT("UBTTask_MoveTo::ExecuteTask tried to go to actor while BB %s entry was empty"), *BlackboardKey.SelectedKeyName.ToString());
			}
		}
		else if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
		{
			const FVector TargetLocation = MyBlackboard->GetValue<UBlackboardKeyType_Vector>(BlackboardKey.GetSelectedKeyID());
			MoveReq.SetGoalLocation(TargetLocation);

			RequestResult = MyController->MoveTo(MoveReq);
		}

		if (RequestResult == EPathFollowingRequestResult::RequestSuccessful)
		{
			const FAIRequestID RequestID = MyController->GetCurrentMoveRequestID();

			MyMemory->MoveRequestID = RequestID;
			WaitForMessage(OwnerComp, UBrainComponent::AIMessage_MoveFinished, RequestID);
			WaitForMessage(OwnerComp, UBrainComponent::AIMessage_RepathFailed);

			NodeResult = EBTNodeResult::InProgress;
		}
		else if (RequestResult == EPathFollowingRequestResult::AlreadyAtGoal)
		{
			NodeResult = EBTNodeResult::Succeeded;
		}
	}

	return NodeResult;
}

EBTNodeResult::Type UBTTask_MoveTo::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTMoveToTaskMemory* MyMemory = (FBTMoveToTaskMemory*)NodeMemory;
	if (!MyMemory->bWaitingForPath)
	{
		AAIController* MyController = OwnerComp.GetAIOwner();

		if (MyController && MyController->GetPathFollowingComponent())
		{
			MyController->GetPathFollowingComponent()->AbortMove(TEXT("BehaviorTree abort"), MyMemory->MoveRequestID);
		}
	}

	return Super::AbortTask(OwnerComp, NodeMemory);
}

void UBTTask_MoveTo::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTMoveToTaskMemory* MyMemory = (FBTMoveToTaskMemory*)NodeMemory;
	if (MyMemory->bWaitingForPath && !OwnerComp.IsPaused())
	{
		AAIController* MyController = OwnerComp.GetAIOwner();
		if (MyController && !MyController->ShouldPostponePathUpdates())
		{
			UE_VLOG(MyController, LogBehaviorTree, Log, TEXT("Pathfinding requests are unlocked!"));
			MyMemory->bWaitingForPath = false;

			const EBTNodeResult::Type NodeResult = PerformMoveTask(OwnerComp, NodeMemory);
			if (NodeResult != EBTNodeResult::InProgress)
			{
				FinishLatentTask(OwnerComp, NodeResult);
			}
		}
	}
}

void UBTTask_MoveTo::OnMessage(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, FName Message, int32 SenderID, bool bSuccess)
{
	// AIMessage_RepathFailed means task has failed
	bSuccess &= (Message != UBrainComponent::AIMessage_RepathFailed);
	Super::OnMessage(OwnerComp, NodeMemory, Message, SenderID, bSuccess);
}

FString UBTTask_MoveTo::GetStaticDescription() const
{
	FString KeyDesc("invalid");
	if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass() ||
		BlackboardKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
	{
		KeyDesc = BlackboardKey.SelectedKeyName.ToString();
	}

	return FString::Printf(TEXT("%s: %s"), *Super::GetStaticDescription(), *KeyDesc);
}

void UBTTask_MoveTo::DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const
{
	Super::DescribeRuntimeValues(OwnerComp, NodeMemory, Verbosity, Values);

	const UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	FBTMoveToTaskMemory* MyMemory = (FBTMoveToTaskMemory*)NodeMemory;

	if (MyMemory->MoveRequestID && BlackboardComp)
	{
		FString KeyValue = BlackboardComp->DescribeKeyValue(BlackboardKey.GetSelectedKeyID(), EBlackboardDescription::OnlyValue);
		Values.Add(FString::Printf(TEXT("move target: %s"), *KeyValue));
	}
}

uint16 UBTTask_MoveTo::GetInstanceMemorySize() const
{
	return sizeof(FBTMoveToTaskMemory);
}

#if WITH_EDITOR

FName UBTTask_MoveTo::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Task.MoveTo.Icon");
}

#endif	// WITH_EDITOR
