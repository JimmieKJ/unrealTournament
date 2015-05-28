// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"

UAIPerceptionStimuliSourceComponent::UAIPerceptionStimuliSourceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSuccessfullyRegistered = false;
}

void UAIPerceptionStimuliSourceComponent::OnRegister()
{
	Super::OnRegister();

	RegisterAsSourceForSenses.RemoveAllSwap([](TSubclassOf<UAISense> SenseClass){
		return SenseClass != nullptr;
	});

	if (bAutoRegisterAsSource)
	{
		RegisterWithPerceptionSystem();
	}
}

void UAIPerceptionStimuliSourceComponent::RegisterWithPerceptionSystem()
{
	if (bSuccessfullyRegistered)
	{
		return;
	}
	if (RegisterAsSourceForSenses.Num() == 0)
	{
		bSuccessfullyRegistered = true;
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor == nullptr)
	{
		return;
	}

	UWorld* World = OwnerActor->GetWorld();
	if (World)
	{
		UAIPerceptionSystem* PerceptionSystem = UAIPerceptionSystem::GetCurrent(World);
		if (PerceptionSystem)
		{
			for (auto& SenseClass : RegisterAsSourceForSenses)
			{
				check(SenseClass);
				PerceptionSystem->RegisterSourceForSenseClass(SenseClass, *OwnerActor);
				bSuccessfullyRegistered = true;
			}
		}
	}
}

void UAIPerceptionStimuliSourceComponent::RegisterForSense(TSubclassOf<UAISense> SenseClass)
{
	if (!SenseClass)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor == nullptr)
	{
		return;
	}

	UWorld* World = OwnerActor->GetWorld();
	if (World)
	{
		UAIPerceptionSystem* PerceptionSystem = UAIPerceptionSystem::GetCurrent(World);
		if (PerceptionSystem)
		{
			PerceptionSystem->RegisterSourceForSenseClass(SenseClass, *OwnerActor);
			bSuccessfullyRegistered = true;
		}
	}
}

void UAIPerceptionStimuliSourceComponent::UnregisterFromPerceptionSystem()
{
	if (bSuccessfullyRegistered == false)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor == nullptr)
	{
		return;
	}

	UWorld* World = OwnerActor->GetWorld();
	if (World)
	{
		UAIPerceptionSystem* PerceptionSystem = UAIPerceptionSystem::GetCurrent(World);
		if (PerceptionSystem)
		{
			for (auto& SenseClass : RegisterAsSourceForSenses)
			{
				PerceptionSystem->UnregisterSource(*OwnerActor, SenseClass);
			}
		}
	}

	bSuccessfullyRegistered = false;
}

void UAIPerceptionStimuliSourceComponent::UnregisterFromSense(TSubclassOf<UAISense> SenseClass)
{
	if (!SenseClass)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor == nullptr)
	{
		return;
	}

	UWorld* World = OwnerActor->GetWorld();
	if (World)
	{
		UAIPerceptionSystem* PerceptionSystem = UAIPerceptionSystem::GetCurrent(World);
		if (PerceptionSystem)
		{
			PerceptionSystem->UnregisterSource(*OwnerActor, SenseClass);
			RegisterAsSourceForSenses.RemoveSingleSwap(SenseClass, /*bAllowShrinking=*/false);
			bSuccessfullyRegistered = RegisterAsSourceForSenses.Num() > 0;
		}
	}
}