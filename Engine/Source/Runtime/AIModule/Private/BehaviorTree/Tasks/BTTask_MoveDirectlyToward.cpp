// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Tasks/BTTask_MoveDirectlyToward.h"

UBTTask_MoveDirectlyToward::UBTTask_MoveDirectlyToward(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
	, AcceptableRadius(50.f)
	, bProjectVectorGoalToNavigation(true)
	, bAllowStrafe(true)
	, bStopOnOverlap(false)
{
	NodeName = "MoveDirectlyToward";

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
