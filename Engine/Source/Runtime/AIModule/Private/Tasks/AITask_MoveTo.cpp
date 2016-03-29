// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "GameplayTasksComponent.h"
#include "Tasks/AITask_MoveTo.h"

UAITask_MoveTo::UAITask_MoveTo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsPausable = true;

	RealGoalLocation = FAISystem::InvalidLocation;
	MoveGoalActor = nullptr;
	MoveAcceptanceRadius = GET_AI_CONFIG_VAR(AcceptanceRadius);
	bShouldStopOnOverlap = GET_AI_CONFIG_VAR(bFinishMoveOnGoalOverlap);
	bShouldAcceptPartialPath = GET_AI_CONFIG_VAR(bAcceptPartialPaths);
	bShouldUsePathfinding = true;

	AddRequiredResource(UAIResource_Movement::StaticClass());
}

UAITask_MoveTo* UAITask_MoveTo::AIMoveTo(AAIController* Controller, FVector InGoalLocation, AActor* InGoalActor, float AcceptanceRadius, EAIOptionFlag::Type StopOnOverlap, EAIOptionFlag::Type AcceptPartialPath, bool bUsePathfinding, bool bLockAILogic)
{
	if (Controller == nullptr)
	{
		return nullptr; 
	}

	UAITask_MoveTo* MyTask = NewTask<UAITask_MoveTo>(*static_cast<IGameplayTaskOwnerInterface*>(Controller));
	if (MyTask)
	{
		MyTask->SetUp(Controller, InGoalLocation, InGoalActor, AcceptanceRadius, bUsePathfinding, StopOnOverlap, AcceptPartialPath);
		MyTask->Priority = uint8(EAITaskPriority::High);
		if (bLockAILogic)
		{
			MyTask->RequestAILogicLocking();
		}
	}

	return MyTask;
}

void UAITask_MoveTo::SetUp(AAIController* Controller, FVector InGoalLocation, AActor* InGoalActor, float AcceptanceRadius, bool bUsePathfinding, EAIOptionFlag::Type StopOnOverlap, EAIOptionFlag::Type AcceptPartialPath)
{
	OwnerController = Controller;

	if (InGoalActor != nullptr)
	{
		MoveGoalActor = InGoalActor;
	}
	else
	{
		RealGoalLocation = InGoalLocation;
	}

	if (AcceptanceRadius >= 0)
	{
		MoveAcceptanceRadius = AcceptanceRadius;
	}
	
	bShouldStopOnOverlap = FAISystem::PickAIOption(StopOnOverlap, bShouldStopOnOverlap);
	bShouldUsePathfinding = bUsePathfinding;

	if (AcceptPartialPath != EAIOptionFlag::Default)
	{
		bShouldAcceptPartialPath = (AcceptPartialPath == EAIOptionFlag::Enable);
	}
}

void UAITask_MoveTo::PostInitProperties()
{
	Super::PostInitProperties();

	if (MoveGoalActor == nullptr)
	{
		RealGoalLocation = MoveGoalLocation;
	}
}

void UAITask_MoveTo::HandleMoveFinished(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
	if (RequestID == MoveRequestID)
	{
		EndTask();
		OnMoveFinished.Broadcast(Result);
	}
	else
	{
		// @todo report issue to the owner component
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Warning, TEXT("%s got movement-finished-notification but RequestID doesn't match. Possible task leak"), *GetName());
	}
}

void UAITask_MoveTo::Activate()
{
	Super::Activate();

	PerformMove();
}

void UAITask_MoveTo::PerformMove()
{
	FAIMoveRequest MoveRequest = (MoveGoalActor != nullptr) ? FAIMoveRequest(MoveGoalActor) : FAIMoveRequest(RealGoalLocation);

	MoveRequest.SetAllowPartialPath(bShouldAcceptPartialPath)
		.SetAcceptanceRadius(MoveAcceptanceRadius)
		.SetStopOnOverlap(bShouldStopOnOverlap)
		.SetUsePathfinding(bShouldUsePathfinding);

	if (PathFollowingDelegateHandle.IsValid())
	{
		OwnerController->GetPathFollowingComponent()->OnMoveFinished.Remove(PathFollowingDelegateHandle);
	}

	const EPathFollowingRequestResult::Type RequestResult = OwnerController->MoveTo(MoveRequest);

	switch (RequestResult)
	{
	case EPathFollowingRequestResult::Failed:
		{
			EndTask();
			OnRequestFailed.Broadcast();
		}
		break;
	case EPathFollowingRequestResult::AlreadyAtGoal:
		{
			EndTask();
			OnMoveFinished.Broadcast(EPathFollowingResult::Success);
		}
		break;
	case EPathFollowingRequestResult::RequestSuccessful:
		if (OwnerController->GetPathFollowingComponent())
		{
			MoveRequestID = OwnerController->GetPathFollowingComponent()->GetCurrentRequestId();

			if (TaskState == EGameplayTaskState::Finished)
			{
				UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("%s re-Activating Finished task!")
					, *GetName());
			}
			else if (PathFollowingDelegateHandle.IsValid() == false)
			{
				PathFollowingDelegateHandle = OwnerController->GetPathFollowingComponent()->OnMoveFinished.AddUObject(this, &UAITask_MoveTo::HandleMoveFinished);
			}
		}
		break;
	default:
		checkNoEntry();
		break;
	}
}

void UAITask_MoveTo::Pause()
{
	if (OwnerController)
	{
		OwnerController->GetPathFollowingComponent()->OnMoveFinished.Remove(PathFollowingDelegateHandle);
		PathFollowingDelegateHandle.Reset();
		OwnerController->PauseMove(MoveRequestID);
	}
	Super::Pause();
}

void UAITask_MoveTo::Resume()
{
	PerformMove();
	Super::Resume();
}

void UAITask_MoveTo::OnDestroy(bool bOwnerFinished)
{
	Super::OnDestroy(bOwnerFinished);
	if (OwnerController && OwnerController->GetPathFollowingComponent())
	{
		OwnerController->GetPathFollowingComponent()->OnMoveFinished.Remove(PathFollowingDelegateHandle);
		OwnerController->GetPathFollowingComponent()->AbortMove(TEXT("UAITask_MoveTo finishing"), MoveRequestID);
	}
}
