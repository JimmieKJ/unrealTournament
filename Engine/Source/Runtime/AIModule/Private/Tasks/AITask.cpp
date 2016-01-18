// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "GameplayTasksComponent.h"


UAITask::UAITask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Priority = uint8(EAITaskPriority::AutonomousAI);
}

AAIController* UAITask::GetAIControllerForActor(AActor* Actor)
{
	AAIController* Result = Cast<AAIController>(Actor);

	if (Result == nullptr)
	{
		APawn* AsPawn = Cast<APawn>(Actor);
		if (AsPawn != nullptr)
		{
			Result = Cast<AAIController>(AsPawn->GetController());
		}
	}

	return Result;
}

void UAITask::Activate()
{
	Super::Activate();

	if (OwnerController == nullptr)
	{
		AActor* OwnerActor = GetOwnerActor();
		if (OwnerActor)
		{
			OwnerController = GetAIControllerForActor(OwnerActor);
		}
		else
		{
			// ERROR!
		}
	}
}

void UAITask::InitAITask(AAIController& AIOwner, IGameplayTaskOwnerInterface& InTaskOwner, uint8 InPriority)
{
	OwnerController = &AIOwner;
	InitTask(InTaskOwner, InPriority);

	if (InPriority == uint8(EAITaskPriority::AutonomousAI))
	{
		AddRequiredResource<UAIResource_Logic>();
	}
}

void UAITask::RequestAILogicLocking()
{
	AddClaimedResource<UAIResource_Logic>();
}