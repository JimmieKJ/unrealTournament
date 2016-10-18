// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "Engine/Blueprint.h"
#include "Blueprint/AIAsyncTaskBlueprintProxy.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"

//----------------------------------------------------------------------//
// UAIAsyncTaskBlueprintProxy
//----------------------------------------------------------------------//

UAIAsyncTaskBlueprintProxy::UAIAsyncTaskBlueprintProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MyWorld = Cast<UWorld>(GetOuter());
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		SetFlags(RF_StrongRefOnFrame);
		UAISystem* const AISystem = UAISystem::GetCurrentSafe(MyWorld.Get());
		if (AISystem)
		{
			AISystem->AddReferenceFromProxyObject(this);
		}
	}
}

void UAIAsyncTaskBlueprintProxy::OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type MovementResult)
{
	if (RequestID.IsEquivalent(MoveRequestId) && AIController.IsValid(true))
	{
		AIController->ReceiveMoveCompleted.RemoveDynamic(this, &UAIAsyncTaskBlueprintProxy::OnMoveCompleted);

		if (MovementResult == EPathFollowingResult::Success)
		{
			OnSuccess.Broadcast(MovementResult);
		}
		else
		{
			OnFail.Broadcast(MovementResult);
		}

		UAISystem* const AISystem = UAISystem::GetCurrentSafe(MyWorld.Get());
		if (AISystem)
		{
			AISystem->RemoveReferenceToProxyObject(this);
		}
	}
}

void UAIAsyncTaskBlueprintProxy::OnNoPath()
{
	OnFail.Broadcast(EPathFollowingResult::Aborted);
	UAISystem* const AISystem = UAISystem::GetCurrentSafe(MyWorld.Get());
	if (AISystem)
	{
		AISystem->RemoveReferenceToProxyObject(this);
	}
}

void UAIAsyncTaskBlueprintProxy::OnAtGoal()
{
	OnSuccess.Broadcast(EPathFollowingResult::Success);
	UAISystem* const AISystem = UAISystem::GetCurrentSafe(MyWorld.Get());
	if (AISystem)
	{
		AISystem->RemoveReferenceToProxyObject(this);
	}
}

void UAIAsyncTaskBlueprintProxy::BeginDestroy()
{
	UAISystem* const AISystem = UAISystem::GetCurrentSafe(MyWorld.Get());
	if (AISystem)
	{
		AISystem->RemoveReferenceToProxyObject(this);
	}
	Super::BeginDestroy();
}

//----------------------------------------------------------------------//
// UAIAsyncTaskBlueprintProxy
//----------------------------------------------------------------------//

UAIBlueprintHelperLibrary::UAIBlueprintHelperLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAIAsyncTaskBlueprintProxy* UAIBlueprintHelperLibrary::CreateMoveToProxyObject(UObject* WorldContextObject, APawn* Pawn, FVector Destination, AActor* TargetActor, float AcceptanceRadius, bool bStopOnOverlap)
{
	check(WorldContextObject);
	if (!Pawn)
	{
		return NULL;
	}
	UAIAsyncTaskBlueprintProxy* MyObj = NULL;
	AAIController* AIController = Cast<AAIController>(Pawn->GetController());
	if (AIController)
	{
		UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
		MyObj = NewObject<UAIAsyncTaskBlueprintProxy>(World);

		FAIMoveRequest MoveReq;
		MoveReq.SetUsePathfinding(true);
		MoveReq.SetAcceptanceRadius(AcceptanceRadius);
		MoveReq.SetReachTestIncludesAgentRadius(bStopOnOverlap);
		if (TargetActor)
		{
			MoveReq.SetGoalActor(TargetActor);
		}
		else
		{
			MoveReq.SetGoalLocation(Destination);
		}
		
		FPathFollowingRequestResult ResultData = AIController->MoveTo(MoveReq);
		switch (ResultData.Code)
		{
		case EPathFollowingRequestResult::RequestSuccessful:
			MyObj->AIController = AIController;
			MyObj->AIController->ReceiveMoveCompleted.AddDynamic(MyObj, &UAIAsyncTaskBlueprintProxy::OnMoveCompleted);
			MyObj->MoveRequestId = ResultData.MoveId;
			break;

		case EPathFollowingRequestResult::AlreadyAtGoal:
			World->GetTimerManager().SetTimer(MyObj->TimerHandle_OnInstantFinish, MyObj, &UAIAsyncTaskBlueprintProxy::OnAtGoal, 0.1f, false);
			break;

		case EPathFollowingRequestResult::Failed:
		default:
			World->GetTimerManager().SetTimer(MyObj->TimerHandle_OnInstantFinish, MyObj, &UAIAsyncTaskBlueprintProxy::OnNoPath, 0.1f, false);
			break;
		}
	}
	return MyObj;
}

void UAIBlueprintHelperLibrary::SendAIMessage(APawn* Target, FName Message, UObject* MessageSource, bool bSuccess)
{
	FAIMessage::Send(Target, FAIMessage(Message, MessageSource, bSuccess));
}

APawn* UAIBlueprintHelperLibrary::SpawnAIFromClass(UObject* WorldContextObject, TSubclassOf<APawn> PawnClass, UBehaviorTree* BehaviorTree, FVector Location, FRotator Rotation, bool bNoCollisionFail)
{
	APawn* NewPawn = NULL;

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World && *PawnClass)
	{
		FActorSpawnParameters ActorSpawnParams;
		ActorSpawnParams.SpawnCollisionHandlingOverride = bNoCollisionFail ? ESpawnActorCollisionHandlingMethod::AlwaysSpawn : ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

		NewPawn = World->SpawnActor<APawn>(*PawnClass, Location, Rotation, ActorSpawnParams);

		if (NewPawn != NULL)
		{
			if (NewPawn->Controller == NULL)
			{	// NOTE: SpawnDefaultController ALSO calls Possess() to possess the pawn (if a controller is successfully spawned).
				NewPawn->SpawnDefaultController();
			}

			if (BehaviorTree != NULL)
			{
				AAIController* AIController = Cast<AAIController>(NewPawn->Controller);

				if (AIController != NULL)
				{
					AIController->RunBehaviorTree(BehaviorTree);
				}
			}
		}
	}

	return NewPawn;
}

AAIController* UAIBlueprintHelperLibrary::GetAIController(AActor* ControlledActor)
{
	APawn* AsPawn = Cast<APawn>(ControlledActor);
	if (AsPawn != nullptr)
	{
		return Cast<AAIController>(AsPawn->GetController());
	}
	return Cast<AAIController>(ControlledActor);
}

UBlackboardComponent* UAIBlueprintHelperLibrary::GetBlackboard(AActor* Target)
{
	UBlackboardComponent* BlackboardComp = nullptr;

	if (Target != nullptr)
	{
		APawn* TargetPawn = Cast<APawn>(Target);
		if (TargetPawn && TargetPawn->GetController())
		{
			BlackboardComp = TargetPawn->GetController()->FindComponentByClass<UBlackboardComponent>();
		}

		if (BlackboardComp == nullptr)
		{
			BlackboardComp = Target->FindComponentByClass<UBlackboardComponent>();
		}
	}

	return BlackboardComp;
}

void UAIBlueprintHelperLibrary::LockAIResourcesWithAnimation(UAnimInstance* AnimInstance, bool bLockMovement, bool LockAILogic)
{
	if (AnimInstance == NULL)
	{
		return;
	}

	APawn* PawnOwner = AnimInstance->TryGetPawnOwner();
	if (PawnOwner)
	{
		AAIController* OwningAI = Cast<AAIController>(PawnOwner->Controller);
		if (OwningAI)
		{
			if (bLockMovement && OwningAI->GetPathFollowingComponent())
			{
				OwningAI->GetPathFollowingComponent()->LockResource(EAIRequestPriority::HardScript);
			}
			if (LockAILogic && OwningAI->BrainComponent)
			{
				OwningAI->BrainComponent->LockResource(EAIRequestPriority::HardScript);
			}
		}
	}
}

void UAIBlueprintHelperLibrary::UnlockAIResourcesWithAnimation(UAnimInstance* AnimInstance, bool bUnlockMovement, bool UnlockAILogic)
{
	if (AnimInstance == NULL)
	{
		return;
	}

	APawn* PawnOwner = AnimInstance->TryGetPawnOwner();
	if (PawnOwner)
	{
		AAIController* OwningAI = Cast<AAIController>(PawnOwner->Controller);
		if (OwningAI)
		{
			if (bUnlockMovement && OwningAI->GetPathFollowingComponent())
			{
				OwningAI->GetPathFollowingComponent()->ClearResourceLock(EAIRequestPriority::HardScript);
			}
			if (UnlockAILogic && OwningAI->BrainComponent)
			{
				OwningAI->BrainComponent->ClearResourceLock(EAIRequestPriority::HardScript);
			}
		}
	}
}

bool UAIBlueprintHelperLibrary::IsValidAILocation(FVector Location)
{
	return FAISystem::IsValidLocation(Location);
}

bool UAIBlueprintHelperLibrary::IsValidAIDirection(FVector DirectionVector)
{
	return FAISystem::IsValidDirection(DirectionVector);
}

bool UAIBlueprintHelperLibrary::IsValidAIRotation(FRotator Rotation)
{
	return FAISystem::IsValidRotation(Rotation);
}
