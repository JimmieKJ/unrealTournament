// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "GameplayAbilityTargetActor.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"

UAbilityTask_WaitTargetData::UAbilityTask_WaitTargetData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

UAbilityTask_WaitTargetData* UAbilityTask_WaitTargetData::WaitTargetData(UObject* WorldContextObject, FName TaskInstanceName, TEnumAsByte<EGameplayTargetingConfirmation::Type> ConfirmationType, TSubclassOf<AGameplayAbilityTargetActor> InTargetClass)
{
	auto MyObj = NewTask<UAbilityTask_WaitTargetData>(WorldContextObject, TaskInstanceName);		//Register for task list here, providing a given FName as a key
	MyObj->TargetClass = InTargetClass;
	MyObj->ConfirmationType = ConfirmationType;
	return MyObj;
}

// ---------------------------------------------------------------------------------------

bool UAbilityTask_WaitTargetData::BeginSpawningActor(UObject* WorldContextObject, TSubclassOf<AGameplayAbilityTargetActor> TargetClass, AGameplayAbilityTargetActor*& SpawnedActor)
{
	SpawnedActor = nullptr;

	UGameplayAbility* MyAbility = Ability.Get();
	if (MyAbility)
	{
		const AGameplayAbilityTargetActor* CDO = CastChecked<AGameplayAbilityTargetActor>(TargetClass->GetDefaultObject());
		MyTargetActor = CDO;

		bool Replicates = CDO->GetReplicates();
		bool StaticFunc = CDO->StaticTargetFunction;
		bool IsLocallyControlled = MyAbility->GetCurrentActorInfo()->IsLocallyControlled();

		if (Replicates && StaticFunc)
		{
			// We can't replicate a staticFunc target actor, since we are just calling a static function and not spawning an actor at all!
			ABILITY_LOG(Fatal, TEXT("AbilityTargetActor class %s can't be Replicating and Static"), *TargetClass->GetName());
			Replicates = false;
		}

		// Spawn the actor if this is a locally controlled ability (always) or if this is a replicating targeting mode.
		// (E.g., server will spawn this target actor to replicate to all non owning clients)
		if (Replicates || IsLocallyControlled || CDO->ShouldProduceTargetDataOnServer)
		{
			if (StaticFunc)
			{
				// This is just a static function that should instantly give us back target data
				FGameplayAbilityTargetDataHandle Data = CDO->StaticGetTargetData(MyAbility->GetWorld(), MyAbility->GetCurrentActorInfo(), MyAbility->GetCurrentActivationInfo());
				OnTargetDataReadyCallback(Data);
			}
			else
			{
				UClass* Class = *TargetClass;
				if (Class != NULL)
				{
					UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
					SpawnedActor = World->SpawnActorDeferred<AGameplayAbilityTargetActor>(Class, FVector::ZeroVector, FRotator::ZeroRotator, NULL, NULL, true);
				}

				// If we spawned the target actor, always register the callbacks for when the data is ready.
				SpawnedActor->TargetDataReadyDelegate.AddUObject(this, &UAbilityTask_WaitTargetData::OnTargetDataReadyCallback);
				SpawnedActor->CanceledDelegate.AddUObject(this, &UAbilityTask_WaitTargetData::OnTargetDataCancelledCallback);

				MyTargetActor = SpawnedActor;

				AGameplayAbilityTargetActor* TargetActor = CastChecked<AGameplayAbilityTargetActor>(SpawnedActor);
				if (TargetActor)
				{
					TargetActor->MasterPC = MyAbility->GetCurrentActorInfo()->PlayerController.Get();
				}
			}
		}		

		// If not locally controlled (server for remote client), see if TargetData was already sent
		// else register callback for when it does get here.
		if (!IsLocallyControlled)
		{
			// Register with the TargetData callbacks if we are expecting client to send them
			if (!CDO->ShouldProduceTargetDataOnServer)
			{
				// Problem here - if there's targeting data just sitting around, fire events don't get hooked up because of this if-else, even if we don't end the task.
				if (AbilitySystemComponent->ReplicatedTargetData.IsValid(0))
				{
					ValidData.Broadcast(AbilitySystemComponent->ReplicatedTargetData);
					if (ConfirmationType == EGameplayTargetingConfirmation::CustomMulti)
					{
						//Since multifire is supported, we still need to hook up the callbacks
						AbilitySystemComponent->ReplicatedTargetDataDelegate.AddUObject(this, &UAbilityTask_WaitTargetData::OnTargetDataReplicatedCallback);
						AbilitySystemComponent->ReplicatedTargetDataCancelledDelegate.AddDynamic(this, &UAbilityTask_WaitTargetData::OnTargetDataReplicatedCancelledCallback);
					}
					else
					{
						EndTask();
					}
				}
				else
				{
					AbilitySystemComponent->ReplicatedTargetDataDelegate.AddUObject(this, &UAbilityTask_WaitTargetData::OnTargetDataReplicatedCallback);
					AbilitySystemComponent->ReplicatedTargetDataCancelledDelegate.AddDynamic(this, &UAbilityTask_WaitTargetData::OnTargetDataReplicatedCancelledCallback);
				}
			}
		}
	}
	return (SpawnedActor != nullptr);
}

void UAbilityTask_WaitTargetData::FinishSpawningActor(UObject* WorldContextObject, AGameplayAbilityTargetActor* SpawnedActor)
{
	if (SpawnedActor && !SpawnedActor->IsPendingKill())
	{
		check(MyTargetActor == SpawnedActor);

		const FTransform SpawnTransform = AbilitySystemComponent->GetOwner()->GetTransform();

		SpawnedActor->FinishSpawning(SpawnTransform);

		// User ability activation is inhibited while this is active
		AbilitySystemComponent->SetUserAbilityActivationInhibited(true);
		AbilitySystemComponent->SpawnedTargetActors.Push(SpawnedActor);

		SpawnedActor->StartTargeting(Ability.Get());

		if (SpawnedActor->ShouldProduceTargetData())
		{
			// If instant confirm, then stop targeting immediately.
			// Note this is kind of bad: we should be able to just call a static func on the CDO to do this. 
			// But then we wouldn't get to set ExposeOnSpawnParameters.
			if (ConfirmationType == EGameplayTargetingConfirmation::Instant)
			{
				SpawnedActor->ConfirmTargeting();
			}
			else if (ConfirmationType == EGameplayTargetingConfirmation::UserConfirmed)
			{
				// If not locally controlled, check if we already have a confirm/cancel repped to us
				if (!Ability->GetCurrentActorInfo()->IsLocallyControlled())
				{
					if (AbilitySystemComponent->ReplicatedConfirmAbility)
					{
						SpawnedActor->ConfirmTargeting();
						return;
					}
					else if (AbilitySystemComponent->ReplicatedCancelAbility)
					{
						SpawnedActor->CancelTargeting();
						return;
					}
				}

				// Bind to the Cancel/Confirm Delegates (called from local confirm or from repped confirm)
				SpawnedActor->BindToConfirmCancelInputs();
			}
		}
	}
}

// ---------------------------------------------------------------------------------------

/** Valid TargetData was replicated to use (we are server, was sent from client) */
void UAbilityTask_WaitTargetData::OnTargetDataReplicatedCallback(FGameplayAbilityTargetDataHandle Data)
{
	check(AbilitySystemComponent.IsValid());

	/** 
	 *  Call into the TargetActor to sanitize/verify the data. If this returns false, we are rejecting
	 *	the replicated target data and will treat this as a cancel.
	 *	
	 *	This can also be used for bandwidth optimizations. OnReplicatedTargetDataReceived could do an actual
	 *	trace/check/whatever server side and use that data. So rather than having the client send that data
	 *	explicitly, the client is basically just sending a 'confirm' and the server is now going to do the work
	 *	in OnReplicatedTargetDataReceived.
	 */
	if (MyTargetActor.IsValid() && !MyTargetActor->OnReplicatedTargetDataReceived(Data))
	{
		Cancelled.Broadcast(Data);
	}
	else
	{
		ValidData.Broadcast(Data);
	}

	if (ConfirmationType != EGameplayTargetingConfirmation::CustomMulti)
	{
		EndTask();
	}
}

/** Client canceled this Targeting Task (we are the server) */
void UAbilityTask_WaitTargetData::OnTargetDataReplicatedCancelledCallback()
{
	check(AbilitySystemComponent.IsValid());

	Cancelled.Broadcast(FGameplayAbilityTargetDataHandle());
	
	EndTask();
}

/** The TargetActor we spawned locally has called back with valid target data */
void UAbilityTask_WaitTargetData::OnTargetDataReadyCallback(FGameplayAbilityTargetDataHandle Data)
{
	check(AbilitySystemComponent.IsValid());

	FScopedPredictionWindow	ScopedPrediction(AbilitySystemComponent.Get(), ShouldReplicateDataToServer());
	
	if (ShouldReplicateDataToServer())
	{
		AbilitySystemComponent->ServerSetReplicatedTargetData(Data, AbilitySystemComponent->ScopedPredictionKey);
	}

	ValidData.Broadcast(Data);

	if (ConfirmationType != EGameplayTargetingConfirmation::CustomMulti)
	{
		EndTask();
	}
}

/** The TargetActor we spawned locally has called back with a cancel event (they still include the 'last/best' targetdata but the consumer of this may want to discard it) */
void UAbilityTask_WaitTargetData::OnTargetDataCancelledCallback(FGameplayAbilityTargetDataHandle Data)
{
	check(AbilitySystemComponent.IsValid());

	AbilitySystemComponent->ServerSetReplicatedTargetDataCancelled();
	Cancelled.Broadcast(Data);

	EndTask();
}

/** Called when the ability is asked to confirm from an outside node. What this means depends on the individual task. By default, this does nothing other than ending if bEndTask is true. */
void UAbilityTask_WaitTargetData::ExternalConfirm(bool bEndTask)
{
	check(AbilitySystemComponent.IsValid());
	if (MyTargetActor.IsValid())
	{
		AGameplayAbilityTargetActor* CachedTargetActor = MyTargetActor.Get();
		if (CachedTargetActor->ShouldProduceTargetData())
		{
			CachedTargetActor->ConfirmTargetingAndContinue();
		}
	}
	Super::ExternalConfirm(bEndTask);
}

/** Called when the ability is asked to confirm from an outside node. What this means depends on the individual task. By default, this does nothing other than ending if bEndTask is true. */
void UAbilityTask_WaitTargetData::ExternalCancel()
{
	check(AbilitySystemComponent.IsValid());
	Cancelled.Broadcast(FGameplayAbilityTargetDataHandle());
	Super::ExternalCancel();
}

void UAbilityTask_WaitTargetData::OnDestroy(bool AbilityEnded)
{
	AbilitySystemComponent->ConsumeAbilityTargetData();
	AbilitySystemComponent->ConsumeAbilityConfirmCancel();
	AbilitySystemComponent->SetUserAbilityActivationInhibited(false);

	AbilitySystemComponent->ReplicatedTargetDataDelegate.RemoveUObject(this, &UAbilityTask_WaitTargetData::OnTargetDataReplicatedCallback);
	AbilitySystemComponent->ReplicatedTargetDataCancelledDelegate.RemoveDynamic(this, &UAbilityTask_WaitTargetData::OnTargetDataReplicatedCancelledCallback);

	if (MyTargetActor.IsValid() && !MyTargetActor->HasAnyFlags(RF_ClassDefaultObject))
	{
		MyTargetActor->Destroy();
	}

	Super::OnDestroy(AbilityEnded);
}

bool UAbilityTask_WaitTargetData::ShouldReplicateDataToServer() const
{
	// Send TargetData to the server IFF we are the client and this isn't a GameplayTargetActor that can produce data on the server	
	const FGameplayAbilityActorInfo* Info = Ability->GetCurrentActorInfo();
	if (!Info->IsNetAuthority() && !TargetClass->GetDefaultObject<AGameplayAbilityTargetActor>()->ShouldProduceTargetDataOnServer)
	{
		return true;
	}

	return false;
}


// --------------------------------------------------------------------------------------

