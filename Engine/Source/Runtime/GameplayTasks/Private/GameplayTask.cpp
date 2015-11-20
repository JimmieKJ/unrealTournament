// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayTasksPrivatePCH.h"
#include "GameplayTasksComponent.h"
#include "GameplayTaskResource.h"
#include "GameplayTask.h"

UGameplayTask::UGameplayTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = false;
	bSimulatedTask = false;
	bIsSimulating = false;
	bOwnedByTasksComponent = false;
	bClaimRequiredResources = true;
	TaskState = EGameplayTaskState::Uninitialized;
	ResourceOverlapPolicy = ETaskResourceOverlapPolicy::StartOnTop;
	Priority = FGameplayTasks::DefaultPriority;

	SetFlags(RF_StrongRefOnFrame);
}

IGameplayTaskOwnerInterface* UGameplayTask::ConvertToTaskOwner(UObject& OwnerObject)
{
	IGameplayTaskOwnerInterface* OwnerInterface = Cast<IGameplayTaskOwnerInterface>(&OwnerObject);

	if (OwnerInterface == nullptr)
	{
		AActor* AsActor = Cast<AActor>(&OwnerObject);
		if (AsActor)
		{
			OwnerInterface = AsActor->FindComponentByClass<UGameplayTasksComponent>();
		}
	}
	return OwnerInterface;
}

IGameplayTaskOwnerInterface* UGameplayTask::ConvertToTaskOwner(AActor& OwnerActor)
{
	IGameplayTaskOwnerInterface* OwnerInterface = Cast<IGameplayTaskOwnerInterface>(&OwnerActor);

	if (OwnerInterface == nullptr)
	{
		OwnerInterface = OwnerActor.FindComponentByClass<UGameplayTasksComponent>();
	}
	return OwnerInterface;
}

void UGameplayTask::ReadyForActivation()
{
	if (TasksComponent.IsValid())
	{
		if (RequiresPriorityOrResourceManagement() == false)
		{
			PerformActivation();
		}
		else
		{
			TasksComponent->AddTaskReadyForActivation(*this);
		}
	}
	else
	{
		EndTask();
	}
}

void UGameplayTask::InitTask(IGameplayTaskOwnerInterface& InTaskOwner, uint8 InPriority)
{
	Priority = InPriority;
	TaskOwner = InTaskOwner;
	UGameplayTasksComponent* GTComponent = InTaskOwner.GetGameplayTasksComponent(*this);
	TasksComponent = GTComponent;

	bOwnedByTasksComponent = (TaskOwner == GTComponent);
	
	TaskState = EGameplayTaskState::AwaitingActivation;

	if (bClaimRequiredResources)
	{
		ClaimedResources.AddSet(RequiredResources);
	}

	InTaskOwner.OnTaskInitialized(*this);
	if (bOwnedByTasksComponent == false && GTComponent != nullptr)
	{
		GTComponent->OnTaskInitialized(*this);
	}
}

void UGameplayTask::InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent)
{
	TasksComponent = &InGameplayTasksComponent;
	bIsSimulating = true;
}

UWorld* UGameplayTask::GetWorld() const
{
	if (TasksComponent.IsValid())
	{
		return TasksComponent.Get()->GetWorld();
	}

	return nullptr;
}

AActor* UGameplayTask::GetOwnerActor() const
{
	if (TaskOwner.IsValid())
	{
		return TaskOwner->GetOwnerActor(this);		
	}
	else if (TasksComponent.IsValid())
	{
		return TasksComponent->GetOwnerActor(this);
	}

	return nullptr;
}

AActor* UGameplayTask::GetAvatarActor() const
{
	if (TaskOwner.IsValid())
	{
		return TaskOwner->GetAvatarActor(this);
	}
	else if (TasksComponent.IsValid())
	{
		return TasksComponent->GetAvatarActor(this);
	}

	return nullptr;
}

void UGameplayTask::TaskOwnerEnded()
{
	UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Verbose
		, TEXT("%s TaskOwnerEnded called, current State: %s")
		, *GetName(), *GetTaskStateName());

	if (TaskState != EGameplayTaskState::Finished && !IsPendingKill())
	{
		OnDestroy(true);
	}
}

void UGameplayTask::EndTask()
{
	UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Verbose
		, TEXT("%s EndTask called, current State: %s")
		, *GetName(), *GetTaskStateName());

	if (TaskState != EGameplayTaskState::Finished && !IsPendingKill())
	{
		OnDestroy(false);
	}
}

void UGameplayTask::ExternalConfirm(bool bEndTask)
{
	UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Verbose
		, TEXT("%s ExternalConfirm called, bEndTask = %s, State : %s")
		, *GetName(), bEndTask ? TEXT("TRUE") : TEXT("FALSE"), *GetTaskStateName());

	if (bEndTask)
	{
		EndTask();
	}
}

void UGameplayTask::ExternalCancel()
{
	UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Verbose
		, TEXT("%s ExternalCancel called, current State: %s")
		, *GetName(), *GetTaskStateName());

	EndTask();
}

void UGameplayTask::OnDestroy(bool bOwnerFinished)
{
	ensure(TaskState != EGameplayTaskState::Finished && !IsPendingKill());

	TaskState = EGameplayTaskState::Finished;

	// First of all notify the TaskComponent
	if (TasksComponent.IsValid())
	{
		TasksComponent->OnTaskDeactivated(*this);
	}

	// Remove ourselves from the owner's task list, if the owner isn't ending
	if (bOwnedByTasksComponent == false && bOwnerFinished == false && TaskOwner.IsValid() == true)
	{
		TaskOwner->OnTaskDeactivated(*this);
	}

	MarkPendingKill();
}

FString UGameplayTask::GetDebugString() const
{
	return FString::Printf(TEXT("Generic %s"), *GetName());
}

void UGameplayTask::AddRequiredResource(TSubclassOf<UGameplayTaskResource> RequiredResource)
{
	check(RequiredResource);
	const uint8 ResourceID = UGameplayTaskResource::GetResourceID(RequiredResource);
	RequiredResources.AddID(ResourceID);	
}

void UGameplayTask::AddRequiredResourceSet(const TArray<TSubclassOf<UGameplayTaskResource> >& RequiredResourceSet)
{
	for (auto Resource : RequiredResourceSet)
	{
		if (Resource)
		{
			const uint8 ResourceID = UGameplayTaskResource::GetResourceID(Resource);
			RequiredResources.AddID(ResourceID);
		}
	}
}

void UGameplayTask::AddRequiredResourceSet(FGameplayResourceSet RequiredResourceSet)
{
	RequiredResources.AddSet(RequiredResourceSet);
}

void UGameplayTask::AddClaimedResource(TSubclassOf<UGameplayTaskResource> ClaimedResource)
{
	check(ClaimedResource);
	const uint8 ResourceID = UGameplayTaskResource::GetResourceID(ClaimedResource);
	ClaimedResources.AddID(ResourceID);
}

void UGameplayTask::AddClaimedResourceSet(const TArray<TSubclassOf<UGameplayTaskResource> >& AdditionalResourcesToClaim)
{
	for (auto ResourceClass : AdditionalResourcesToClaim)
	{
		if (ResourceClass)
		{
			ClaimedResources.AddID(UGameplayTaskResource::GetResourceID(ResourceClass));
		}
	}
}

void UGameplayTask::AddClaimedResourceSet(FGameplayResourceSet AdditionalResourcesToClaim)
{
	ClaimedResources.AddSet(AdditionalResourcesToClaim);
}

void UGameplayTask::PerformActivation()
{
	if (TaskState == EGameplayTaskState::Active)
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Warning
			, TEXT("%s PerformActivation called while TaskState is already Active. Bailing out.")
			, *GetName());
		return;
	}

	TaskState = EGameplayTaskState::Active;

	Activate();

	TasksComponent->OnTaskActivated(*this);

	if (bOwnedByTasksComponent == false && TaskOwner.IsValid())
	{
		TaskOwner->OnTaskActivated(*this);
	}
}

void UGameplayTask::Activate()
{
	UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Verbose
		, TEXT("%s Activate called, current State: %s")
		, *GetName(), *GetTaskStateName());
}

void UGameplayTask::Pause()
{
	UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Verbose
		, TEXT("%s Pause called, current State: %s")
		, *GetName(), *GetTaskStateName());

	TaskState = EGameplayTaskState::Paused;

	TasksComponent->OnTaskDeactivated(*this);

	if (bOwnedByTasksComponent == false && TaskOwner.IsValid())
	{
		TaskOwner->OnTaskDeactivated(*this);
	}
}

void UGameplayTask::Resume()
{
	UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Verbose
		, TEXT("%s Resume called, current State: %s")
		, *GetName(), *GetTaskStateName());

	TaskState = EGameplayTaskState::Active;

	TasksComponent->OnTaskActivated(*this);

	if (bOwnedByTasksComponent == false && TaskOwner.IsValid())
	{
		TaskOwner->OnTaskActivated(*this);
	}
}

//----------------------------------------------------------------------//
// GameplayTasksComponent-related functions
//----------------------------------------------------------------------//
void UGameplayTask::ActivateInTaskQueue()
{
	switch(TaskState)
	{
	case EGameplayTaskState::Uninitialized:
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Error
			, TEXT("UGameplayTask::ActivateInTaskQueue Task %s passed for activation withouth having InitTask called on it!")
			, *GetName());
		break;
	case EGameplayTaskState::AwaitingActivation:
		PerformActivation();
		break;
	case EGameplayTaskState::Paused:
		// resume
		Resume();
		break;
	case EGameplayTaskState::Active:
		// nothing to do here
		break;
	case EGameplayTaskState::Finished:
		// If a task has finished, and it's being revived let's just treat the same as AwaitingActivation
		PerformActivation();
		break;
	default:
		checkNoEntry(); // looks like unhandled value! Probably a new enum entry has been added
		break;
	}
}

void UGameplayTask::PauseInTaskQueue()
{
	switch (TaskState)
	{
	case EGameplayTaskState::Uninitialized:
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Error
			, TEXT("UGameplayTask::PauseInTaskQueue Task %s passed for pausing withouth having InitTask called on it!")
			, *GetName());
		break;
	case EGameplayTaskState::AwaitingActivation:
		// nothing to do here. Don't change the state to indicate this task has never been run before
		break;
	case EGameplayTaskState::Paused:
		// nothing to do here. Already paused
		break;
	case EGameplayTaskState::Active:
		// pause!
		Pause();
		break;
	case EGameplayTaskState::Finished:
		// nothing to do here. But sounds odd, so let's log this, just in case
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log
			, TEXT("UGameplayTask::PauseInTaskQueue Task %s being pause while already marked as Finished")
			, *GetName());
		break;
	default:
		checkNoEntry(); // looks like unhandled value! Probably a new enum entry has been added
		break;
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
//----------------------------------------------------------------------//
// debug
//----------------------------------------------------------------------//
FString UGameplayTask::GenerateDebugDescription() const
{
	return RequiresPriorityOrResourceManagement() == false ? GetName()
		: FString::Printf(TEXT("%s: P:%d %s")
			, *GetName(), int32(Priority), *RequiredResources.GetDebugDescription());
}

FString UGameplayTask::GetTaskStateName() const
{
	static const UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EGameplayTaskState"));
	check(Enum);
	return Enum->GetEnumName(int32(TaskState));
}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)