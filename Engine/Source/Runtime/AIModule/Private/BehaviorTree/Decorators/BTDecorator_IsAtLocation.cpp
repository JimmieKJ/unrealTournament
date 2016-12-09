// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Decorators/BTDecorator_IsAtLocation.h"
#include "GameFramework/Actor.h"
#include "Navigation/PathFollowingComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "AIController.h"

UBTDecorator_IsAtLocation::UBTDecorator_IsAtLocation(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Is At Location";
	AcceptableRadius = 50.0f;
	bUseParametrizedRadius = false;
	bUseNavAgentGoalLocation = true;

	// accept only actors and vectors
	BlackboardKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_IsAtLocation, BlackboardKey), AActor::StaticClass());
	BlackboardKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_IsAtLocation, BlackboardKey));

	// can't abort, it's not observing anything
	bAllowAbortLowerPri = false;
	bAllowAbortNone = false;
	bAllowAbortChildNodes = false;
	FlowAbortMode = EBTFlowAbortMode::None;
}

bool UBTDecorator_IsAtLocation::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	bool bHasReached = false;

	AAIController* AIOwner = OwnerComp.GetAIOwner();
	UPathFollowingComponent* PathFollowingComponent = AIOwner ? AIOwner->GetPathFollowingComponent() : nullptr;
	if (PathFollowingComponent)
	{
		const UBlackboardComponent* MyBlackboard = OwnerComp.GetBlackboardComponent();
		
		float Radius = AcceptableRadius;
		if (bUseParametrizedRadius && AIOwner && AIOwner->GetPawn() && ParametrizedAcceptableRadius.IsDynamic())
		{
			ParametrizedAcceptableRadius.BindData(AIOwner->GetPawn(), INDEX_NONE);
			Radius = ParametrizedAcceptableRadius.GetValue();
		}

		if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass())
		{
			UObject* KeyValue = MyBlackboard->GetValue<UBlackboardKeyType_Object>(BlackboardKey.GetSelectedKeyID());
			AActor* TargetActor = Cast<AActor>(KeyValue);
			if (TargetActor)
			{
				bHasReached = PathFollowingComponent->HasReached(*TargetActor, EPathFollowingReachMode::OverlapAgentAndGoal, Radius, bUseNavAgentGoalLocation);
			}
		}
		else if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
		{
			const FVector TargetLocation = MyBlackboard->GetValue<UBlackboardKeyType_Vector>(BlackboardKey.GetSelectedKeyID());
			if (FAISystem::IsValidLocation(TargetLocation))
			{
				bHasReached = PathFollowingComponent->HasReached(TargetLocation, EPathFollowingReachMode::OverlapAgent, Radius);
			}
		}
	}

	return bHasReached;
}

FString UBTDecorator_IsAtLocation::GetStaticDescription() const
{
	FString KeyDesc("invalid");
	if (BlackboardKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass() ||
		BlackboardKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
	{
		KeyDesc = BlackboardKey.SelectedKeyName.ToString();
	}

	return FString::Printf(TEXT("%s: %s"), *Super::GetStaticDescription(), *KeyDesc);
}

#if WITH_EDITOR

FName UBTDecorator_IsAtLocation::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Decorator.ReachedMoveGoal.Icon");
}

#endif	// WITH_EDITOR
