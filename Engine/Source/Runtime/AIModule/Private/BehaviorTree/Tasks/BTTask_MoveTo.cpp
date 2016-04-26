// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "Tasks/AITask_MoveTo.h"

UBTTask_MoveTo::UBTTask_MoveTo(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
{
	NodeName = "Move To";
	bNotifyTick = true;
	bNotifyTaskFinished = true;

	AcceptableRadius = GET_AI_CONFIG_VAR(AcceptanceRadius);
	bStopOnOverlap = GET_AI_CONFIG_VAR(bFinishMoveOnGoalOverlap); 
	bAllowStrafe = GET_AI_CONFIG_VAR(bAllowStrafing);
	bAllowPartialPath = GET_AI_CONFIG_VAR(bAcceptPartialPaths);

	ObservedBlackboardValueTolerance = AcceptableRadius * 0.95f;

	// accept only actors and vectors
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MoveTo, BlackboardKey), AActor::StaticClass());
	BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MoveTo, BlackboardKey));
}

EBTNodeResult::Type UBTTask_MoveTo::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	EBTNodeResult::Type NodeResult = EBTNodeResult::InProgress;

	FBTMoveToTaskMemory* MyMemory = reinterpret_cast<FBTMoveToTaskMemory*>(NodeMemory);
	MyMemory->PreviousGoalLocation = FAISystem::InvalidLocation;
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

	if (NodeResult == EBTNodeResult::InProgress && bObserveBlackboardValue)
	{
		UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
		if (ensure(BlackboardComp))
		{
			if (MyMemory->BBObserverDelegateHandle.IsValid())
			{
				UE_VLOG(MyController, LogBehaviorTree, Warning, TEXT("UBTTask_MoveTo::ExecuteTask \'%s\' Old BBObserverDelegateHandle is still valid! Removing old Observer."), *GetNodeName());
				BlackboardComp->UnregisterObserver(BlackboardKey.GetSelectedKeyID(), MyMemory->BBObserverDelegateHandle);
			}
			MyMemory->BBObserverDelegateHandle = BlackboardComp->RegisterObserver(BlackboardKey.GetSelectedKeyID(), this, FOnBlackboardChangeNotification::CreateUObject(this, &UBTTask_MoveTo::OnBlackboardValueChange));
		}
	}
	
	
	return NodeResult;
}

EBTNodeResult::Type UBTTask_MoveTo::PerformMoveTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const UBlackboardComponent* MyBlackboard = OwnerComp.GetBlackboardComponent();
	FBTMoveToTaskMemory* MyMemory = reinterpret_cast<FBTMoveToTaskMemory*>(NodeMemory);
	AAIController* MyController = OwnerComp.GetAIOwner();

	EBTNodeResult::Type NodeResult = EBTNodeResult::Failed;

	if (MyController && MyBlackboard)
	{
		if (GET_AI_CONFIG_VAR(bEnableBTAITasks))
		{
			bool bTaskReusing = false;
			UAITask_MoveTo* AIMoveTask = nullptr;

			if (MyMemory->Task.IsValid())
			{
				AIMoveTask = MyMemory->Task.Get();
				ensure(AIMoveTask->GetState() != EGameplayTaskState::Finished);
				bTaskReusing = true;
			}
			else
			{
				AIMoveTask = NewBTAITask<UAITask_MoveTo>(OwnerComp);
			}			
			
			if (AIMoveTask != nullptr)
			{
				bool bSetUp = false;
				if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass())
				{
					UObject* KeyValue = MyBlackboard->GetValue<UBlackboardKeyType_Object>(BlackboardKey.GetSelectedKeyID());
					AActor* TargetActor = Cast<AActor>(KeyValue);
					if (TargetActor)
					{
						AIMoveTask->SetUp(MyController, FVector::ZeroVector, TargetActor, AcceptableRadius, /*bUsePathfinding=*/true, FAISystem::BoolToAIOption(bStopOnOverlap), FAISystem::BoolToAIOption(bAllowPartialPath));
						NodeResult = EBTNodeResult::InProgress;
					}
					else
					{
						UE_VLOG(MyController, LogBehaviorTree, Warning, TEXT("UBTTask_MoveTo::ExecuteTask tried to go to actor while BB %s entry was empty"), *BlackboardKey.SelectedKeyName.ToString());
					}
				}
				else if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
				{
					const FVector TargetLocation = MyBlackboard->GetValue<UBlackboardKeyType_Vector>(BlackboardKey.GetSelectedKeyID());
					MyMemory->PreviousGoalLocation = TargetLocation;
					AIMoveTask->SetUp(MyController, TargetLocation, nullptr, AcceptableRadius, /*bUsePathfinding=*/true, FAISystem::BoolToAIOption(bStopOnOverlap), FAISystem::BoolToAIOption(bAllowPartialPath));
					NodeResult = EBTNodeResult::InProgress;
				}

				if (NodeResult == EBTNodeResult::InProgress)
				{
					/*WaitForMessage(OwnerComp, UBrainComponent::AIMessage_MoveFinished, RequestID);
					WaitForMessage(OwnerComp, UBrainComponent::AIMessage_RepathFailed);*/

					if (bTaskReusing == false)
					{
						MyMemory->Task = AIMoveTask;
						UE_VLOG(MyController, LogBehaviorTree, Verbose, TEXT("\'%s\' task implementing move with task %s"), *GetNodeName(), *AIMoveTask->GetName());
						AIMoveTask->ReadyForActivation();
					}
					else
					{
						ensure(AIMoveTask->IsActive());
						UE_VLOG(MyController, LogBehaviorTree, Verbose, TEXT("\'%s\' reusing AITask %s"), *GetNodeName(), *AIMoveTask->GetName());
						AIMoveTask->PerformMove();
					}
				}
			}
		}
		else
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
				MyMemory->PreviousGoalLocation = TargetLocation;

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
	}

	return NodeResult;
}

EBlackboardNotificationResult UBTTask_MoveTo::OnBlackboardValueChange(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID)
{
	UBehaviorTreeComponent* BehaviorComp = Cast<UBehaviorTreeComponent>(Blackboard.GetBrainComponent());
	if (BehaviorComp == nullptr)
	{
		return EBlackboardNotificationResult::RemoveObserver;
	}

	uint8* RawMemory = BehaviorComp->GetNodeMemory(this, BehaviorComp->FindInstanceContainingNode(this));
	FBTMoveToTaskMemory* MyMemory = reinterpret_cast<FBTMoveToTaskMemory*>(RawMemory);

	const EBTTaskStatus::Type TaskStatus = BehaviorComp->GetTaskStatus(this);
	if (TaskStatus != EBTTaskStatus::Active)
	{
		UE_VLOG(BehaviorComp, LogBehaviorTree, Error, TEXT("BT MoveTo \'%s\' task observing BB entry while no longer being active!"), *GetNodeName());

		// resetting BBObserverDelegateHandle without unregistering observer since 
		// returning EBlackboardNotificationResult::RemoveObserver here will take care of that for us
		MyMemory->BBObserverDelegateHandle.Reset();

		return EBlackboardNotificationResult::RemoveObserver;
	}
	
	// this means the move has already started. MyMemory->bWaitingForPath == true would mean we're waiting for right moment to start it anyway,
	// so we don't need to do anything due to BB value change 
	if (MyMemory != nullptr && MyMemory->bWaitingForPath == false && BehaviorComp->GetAIOwner() != nullptr)
	{
		check(BehaviorComp->GetAIOwner()->GetPathFollowingComponent());

		bool bUpdateMove = true;
		// check if new goal is almost identical to previous one
		if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
		{
			const FVector TargetLocation = Blackboard.GetValue<UBlackboardKeyType_Vector>(BlackboardKey.GetSelectedKeyID());

			bUpdateMove = (FVector::DistSquared(TargetLocation, MyMemory->PreviousGoalLocation) > ObservedBlackboardValueTolerance*ObservedBlackboardValueTolerance);
		}

		if (bUpdateMove)
		{
			// don't abort move if using AI tasks - it will mess things up
			if (GET_AI_CONFIG_VAR(bEnableBTAITasks) == false)
			{
				StopWaitingForMessages(*BehaviorComp);
				BehaviorComp->GetAIOwner()->GetPathFollowingComponent()->AbortMove(TEXT("Updating move due to BB value change"), MyMemory->MoveRequestID, /*bResetVelocity=*/false, /*bSilent=*/true);
			}

			if (BehaviorComp->GetAIOwner()->ShouldPostponePathUpdates())
			{
				// NodeTick will take care of requesting move
				MyMemory->bWaitingForPath = true;
			}
			else
			{
				const EBTNodeResult::Type NodeResult = PerformMoveTask(*BehaviorComp, RawMemory);

				if (NodeResult != EBTNodeResult::InProgress)
				{
					FinishLatentTask(*BehaviorComp, NodeResult);
				}
			}
		}
	}

	return EBlackboardNotificationResult::ContinueObserving;
}

EBTNodeResult::Type UBTTask_MoveTo::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTMoveToTaskMemory* MyMemory = reinterpret_cast<FBTMoveToTaskMemory*>(NodeMemory);
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

void UBTTask_MoveTo::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	FBTMoveToTaskMemory* MyMemory = reinterpret_cast<FBTMoveToTaskMemory*>(NodeMemory);

	if (bObserveBlackboardValue)
	{
		UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
		if (ensure(BlackboardComp) && MyMemory->BBObserverDelegateHandle.IsValid())
		{
			BlackboardComp->UnregisterObserver(BlackboardKey.GetSelectedKeyID(), MyMemory->BBObserverDelegateHandle);
		}
		MyMemory->BBObserverDelegateHandle.Reset();
	}

	if (GET_AI_CONFIG_VAR(bEnableBTAITasks))
	{
		MyMemory->Task = nullptr;
	}

	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
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
