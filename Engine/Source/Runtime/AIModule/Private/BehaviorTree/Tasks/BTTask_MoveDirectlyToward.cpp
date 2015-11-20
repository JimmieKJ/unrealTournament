// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Tasks/BTTask_MoveDirectlyToward.h"
#include "Tasks/AITask_MoveTo.h"

UBTTask_MoveDirectlyToward::UBTTask_MoveDirectlyToward(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
	, bProjectVectorGoalToNavigation(true)
{
	NodeName = "MoveDirectlyToward";

	AcceptableRadius = GET_AI_CONFIG_VAR(AcceptanceRadius);
	bStopOnOverlap = GET_AI_CONFIG_VAR(bFinishMoveOnGoalOverlap);
	bAllowStrafe = GET_AI_CONFIG_VAR(bAllowStrafing);

	// accept only actors and vectors
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MoveDirectlyToward, BlackboardKey), AActor::StaticClass());
	BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MoveDirectlyToward, BlackboardKey));
}

EBTNodeResult::Type UBTTask_MoveDirectlyToward::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const UBlackboardComponent* MyBlackboard = OwnerComp.GetBlackboardComponent();
	FBTMoveDirectlyTowardMemory* MyMemory = reinterpret_cast<FBTMoveDirectlyTowardMemory*>(NodeMemory);
	AAIController* MyController = OwnerComp.GetAIOwner();
	EBTNodeResult::Type NodeResult = EBTNodeResult::Failed;

	if (MyController && MyBlackboard)
	{
		if (GET_AI_CONFIG_VAR(bEnableBTAITasks))
		{
			UAITask_MoveTo* AIMoveTask = NewBTAITask<UAITask_MoveTo>(OwnerComp);

			if (AIMoveTask != nullptr)
			{
				bool bSetUp = false;
				if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass())
				{
					UObject* KeyValue = MyBlackboard->GetValue<UBlackboardKeyType_Object>(BlackboardKey.GetSelectedKeyID());
					AActor* TargetActor = Cast<AActor>(KeyValue);
					if (TargetActor)
					{
						AIMoveTask->SetUp(MyController, FVector::ZeroVector, TargetActor, AcceptableRadius, /*bUsePathfinding=*/false, FAISystem::BoolToAIOption(bStopOnOverlap));
						NodeResult = EBTNodeResult::InProgress;
					}
					else
					{
						UE_VLOG(MyController, LogBehaviorTree, Warning, TEXT("UBTTask_MoveDirectlyToward::ExecuteTask tried to go to actor while BB %s entry was empty"), *BlackboardKey.SelectedKeyName.ToString());
					}
				}
				else if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
				{
					const FVector TargetLocation = MyBlackboard->GetValue<UBlackboardKeyType_Vector>(BlackboardKey.GetSelectedKeyID());
					AIMoveTask->SetUp(MyController, TargetLocation, nullptr, AcceptableRadius, /*bUsePathfinding=*/false, FAISystem::BoolToAIOption(bStopOnOverlap));
					NodeResult = EBTNodeResult::InProgress;
				}

				if (NodeResult == EBTNodeResult::InProgress)
				{
					AIMoveTask->ReadyForActivation();
				}
			}
		}
		else
		{
			EPathFollowingRequestResult::Type RequestResult = EPathFollowingRequestResult::Failed;

			if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass())
			{
				UObject* KeyValue = MyBlackboard->GetValue<UBlackboardKeyType_Object>(BlackboardKey.GetSelectedKeyID());
				AActor* TargetActor = Cast<AActor>(KeyValue);
				if (TargetActor)
				{
					RequestResult = bDisablePathUpdateOnGoalLocationChange ?
						MyController->MoveToLocation(TargetActor->GetActorLocation(), AcceptableRadius, bStopOnOverlap, /*bUsePathfinding=*/false, /*bProjectDestinationToNavigation=*/bProjectVectorGoalToNavigation, bAllowStrafe) :
						MyController->MoveToActor(TargetActor, AcceptableRadius, bStopOnOverlap, /*bUsePathfinding=*/false, bAllowStrafe);
				}
			}
			else if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
			{
				const FVector TargetLocation = MyBlackboard->GetValue<UBlackboardKeyType_Vector>(BlackboardKey.GetSelectedKeyID());
				RequestResult = MyController->MoveToLocation(TargetLocation, AcceptableRadius, bStopOnOverlap, /*bUsePathfinding=*/false, /*bProjectDestinationToNavigation=*/bProjectVectorGoalToNavigation, bAllowStrafe);
			}

			if (RequestResult == EPathFollowingRequestResult::RequestSuccessful)
			{
				const FAIRequestID RequestID = MyController->GetCurrentMoveRequestID();

				MyMemory->MoveRequestID = RequestID;
				WaitForMessage(OwnerComp, UBrainComponent::AIMessage_MoveFinished, RequestID);

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

EBTNodeResult::Type UBTTask_MoveDirectlyToward::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTMoveDirectlyTowardMemory* MyMemory = reinterpret_cast<FBTMoveDirectlyTowardMemory*>(NodeMemory);
	AAIController* MyController = OwnerComp.GetAIOwner();

	if (MyController && MyController->GetPathFollowingComponent())
	{
		MyController->GetPathFollowingComponent()->AbortMove(TEXT("BehaviorTree abort"), MyMemory->MoveRequestID);
	}

	return Super::AbortTask(OwnerComp, NodeMemory);
}

FString UBTTask_MoveDirectlyToward::GetStaticDescription() const
{
	FString KeyDesc("invalid");
	if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass() ||
		BlackboardKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
	{
		KeyDesc = BlackboardKey.SelectedKeyName.ToString();
	}

	return FString::Printf(TEXT("%s: %s"), *Super::GetStaticDescription(), *KeyDesc);
}

void UBTTask_MoveDirectlyToward::DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const
{
	Super::DescribeRuntimeValues(OwnerComp, NodeMemory, Verbosity, Values);

	const UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	FBTMoveDirectlyTowardMemory* MyMemory = reinterpret_cast<FBTMoveDirectlyTowardMemory*>(NodeMemory);

	if (MyMemory->MoveRequestID && BlackboardComp)
	{
		FString KeyValue = BlackboardComp->DescribeKeyValue(BlackboardKey.GetSelectedKeyID(), EBlackboardDescription::OnlyValue);
		Values.Add(FString::Printf(TEXT("move target: %s"), *KeyValue));
	}
}

uint16 UBTTask_MoveDirectlyToward::GetInstanceMemorySize() const
{
	return sizeof(FBTMoveDirectlyTowardMemory);
}

#if WITH_EDITOR

FName UBTTask_MoveDirectlyToward::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Task.MoveDirectlyToward.Icon");
}

#endif	// WITH_EDITOR
