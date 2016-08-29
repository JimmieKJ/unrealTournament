// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GameplayTasksPrivatePCH.h"
#include "GameplayTaskResource.h"
#include "Tasks/GameplayTask_ClaimResource.h"

UGameplayTask_ClaimResource::UGameplayTask_ClaimResource(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UGameplayTask_ClaimResource* UGameplayTask_ClaimResource::ClaimResource(TScriptInterface<IGameplayTaskOwnerInterface> InTaskOwner, TSubclassOf<UGameplayTaskResource> ResourceClass, const uint8 Priority, const FName TaskInstanceName)
{
	return InTaskOwner.GetInterface() ? ClaimResource(*InTaskOwner, ResourceClass, Priority, TaskInstanceName) : nullptr;
}

UGameplayTask_ClaimResource* UGameplayTask_ClaimResource::ClaimResource(IGameplayTaskOwnerInterface& InTaskOwner, const TSubclassOf<UGameplayTaskResource> ResourceClass, const uint8 Priority, const FName TaskInstanceName)
{
	if (!ResourceClass)
	{
		return nullptr;
	}

	UGameplayTask_ClaimResource* MyTask = NewObject<UGameplayTask_ClaimResource>();
	if (MyTask)
	{
		MyTask->InitTask(InTaskOwner, Priority);
		MyTask->InstanceName = TaskInstanceName;
		MyTask->AddClaimedResource(ResourceClass);
	}

	return MyTask;
}
