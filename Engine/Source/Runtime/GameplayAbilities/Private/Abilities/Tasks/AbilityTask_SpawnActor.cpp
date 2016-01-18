// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "GameplayAbilityTargetActor.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_SpawnActor.h"


UAbilityTask_SpawnActor::UAbilityTask_SpawnActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

UAbilityTask_SpawnActor* UAbilityTask_SpawnActor::SpawnActor(UObject* WorldContextObject, FGameplayAbilityTargetDataHandle TargetData, TSubclassOf<AActor> InClass)
{
	auto MyObj = NewAbilityTask<UAbilityTask_SpawnActor>(WorldContextObject);
	MyObj->CachedTargetDataHandle = TargetData;
	return MyObj;
}

// ---------------------------------------------------------------------------------------

bool UAbilityTask_SpawnActor::BeginSpawningActor(UObject* WorldContextObject, FGameplayAbilityTargetDataHandle TargetData, TSubclassOf<AActor> InClass, AActor*& SpawnedActor)
{
	if (Ability.IsValid() && Ability.Get()->GetCurrentActorInfo()->IsNetAuthority())
	{
		UWorld* const World = GEngine->GetWorldFromContextObject(WorldContextObject);
		if (World)
		{
			SpawnedActor = World->SpawnActorDeferred<AActor>(InClass, FTransform::Identity, NULL, NULL, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		}
	}
	
	if (SpawnedActor == nullptr)
	{
		DidNotSpawn.Broadcast(nullptr);
		return false;
	}

	return true;
}

void UAbilityTask_SpawnActor::FinishSpawningActor(UObject* WorldContextObject, FGameplayAbilityTargetDataHandle TargetData, AActor* SpawnedActor)
{
	if (SpawnedActor)
	{
		FTransform SpawnTransform = AbilitySystemComponent->GetOwner()->GetTransform();

		if (FGameplayAbilityTargetData* LocationData = CachedTargetDataHandle.Get(0))		//Hardcode to use data 0. It's OK if data isn't useful/valid.
		{
			//Set location. Rotation is unaffected.
			if (LocationData->HasHitResult())
			{
				SpawnTransform.SetLocation(LocationData->GetHitResult()->Location);
			}
			else if (LocationData->HasEndPoint())
			{
				SpawnTransform = LocationData->GetEndPointTransform();
			}
		}

		SpawnedActor->FinishSpawning(SpawnTransform);

		Success.Broadcast(SpawnedActor);
	}

	EndTask();
}

// ---------------------------------------------------------------------------------------

