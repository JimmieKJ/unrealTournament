// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayTasksPrivatePCH.h"
#include "Tasks/GameplayTask_SpawnActor.h"

UGameplayTask_SpawnActor* UGameplayTask_SpawnActor::SpawnActor(TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner, FVector SpawnLocation, FRotator SpawnRotation, TSubclassOf<AActor> Class, bool bSpawnOnlyOnAuthority)
{
	if (TaskOwner.GetInterface() == nullptr)
	{
		return nullptr;
	}

	bool bCanSpawn = true;
	if (bSpawnOnlyOnAuthority == true)
	{
		AActor* TaskOwnerActor = TaskOwner->GetOwnerActor(nullptr);
		if (TaskOwnerActor)
		{
			bCanSpawn = (TaskOwnerActor->Role == ROLE_Authority);
		}
		else
		{
			// @todo add warning here
		}
	}

	UGameplayTask_SpawnActor* MyTask = nullptr;
	if (bCanSpawn)
	{
		MyTask = NewTask<UGameplayTask_SpawnActor>(TaskOwner);
		if (MyTask)
		{
			MyTask->CachedSpawnLocation = SpawnLocation;
			MyTask->CachedSpawnRotation = SpawnRotation;
			MyTask->ClassToSpawn = Class;
		}
	}
	return MyTask;
}

bool UGameplayTask_SpawnActor::BeginSpawningActor(UObject* WorldContextObject, AActor*& SpawnedActor)
{
	UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World)
	{
		SpawnedActor = World->SpawnActorDeferred<AActor>(ClassToSpawn, FTransform(CachedSpawnRotation, CachedSpawnLocation), NULL, NULL, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	}
	
	if (SpawnedActor == nullptr)
	{
		DidNotSpawn.Broadcast(nullptr);
		return false;
	}

	return true;
}

void UGameplayTask_SpawnActor::FinishSpawningActor(UObject* WorldContextObject, AActor* SpawnedActor)
{
	if (SpawnedActor)
	{
		const FTransform SpawnTransform(CachedSpawnRotation, CachedSpawnLocation);		
		SpawnedActor->FinishSpawning(SpawnTransform);
		Success.Broadcast(SpawnedActor);
	}

	EndTask();
}
