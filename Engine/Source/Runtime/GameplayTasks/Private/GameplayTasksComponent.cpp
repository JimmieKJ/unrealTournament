// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayTasksPrivatePCH.h"
#include "GameplayTasksComponent.h"
#include "GameplayTask.h"
#include "MessageLog.h"

#define LOCTEXT_NAMESPACE "GameplayTasksComponent"

namespace
{
	FORCEINLINE const TCHAR* GetGameplayTaskEventName(EGameplayTaskEvent Event)
	{
		/*static const UEnum* GameplayTaskEventEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EGameplayTaskEvent"));
		return GameplayTaskEventEnum->GetEnumText(static_cast<int32>(Event)).ToString();*/

		return Event == EGameplayTaskEvent::Add ? TEXT("Add") : TEXT("Remove");
	}
}

UGameplayTasksComponent::UGameplayTasksComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = true;

	bReplicates = true;
	TopActivePriority = 0;
}
	
void UGameplayTasksComponent::OnTaskActivated(UGameplayTask& Task)
{
	if (Task.IsTickingTask())
	{
		check(TickingTasks.Contains(&Task) == false);
		TickingTasks.Add(&Task);

		// If this is our first ticking task, set this component as active so it begins ticking
		if (TickingTasks.Num() == 1)
		{
			UpdateShouldTick();
		}
	}
	if (Task.IsSimulatedTask())
	{
		check(SimulatedTasks.Contains(&Task) == false);
		SimulatedTasks.Add(&Task);
	}
}

void UGameplayTasksComponent::OnTaskDeactivated(UGameplayTask& Task)
{
	if (Task.IsTickingTask())
	{
		// If we are removing our last ticking task, set this component as inactive so it stops ticking
		TickingTasks.RemoveSingleSwap(&Task);
	}

	if (Task.IsSimulatedTask())
	{
		SimulatedTasks.RemoveSingleSwap(&Task);
	}

	UpdateShouldTick();

	// Resource-using task
	if (Task.RequiresPriorityOrResourceManagement() == true && Task.GetState() == EGameplayTaskState::Finished)
	{
		OnTaskEnded(Task);
	}
}

void UGameplayTasksComponent::OnTaskEnded(UGameplayTask& Task)
{
	ensure(Task.RequiresPriorityOrResourceManagement() == true);
	RemoveResourceConsumingTask(Task);
}

void UGameplayTasksComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	// Intentionally not calling super: We do not want to replicate bActive which controls ticking. We sometimes need to tick on client predictively.
	DOREPLIFETIME_CONDITION(UGameplayTasksComponent, SimulatedTasks, COND_SkipOwner);
}

bool UGameplayTasksComponent::ReplicateSubobjects(UActorChannel* Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	
	if (!RepFlags->bNetOwner)
	{
		for (UGameplayTask* SimulatedTask : SimulatedTasks)
		{
			if (SimulatedTask && !SimulatedTask->IsPendingKill())
			{
				WroteSomething |= Channel->ReplicateSubobject(SimulatedTask, *Bunch, *RepFlags);
			}
		}
	}

	return WroteSomething;
}

void UGameplayTasksComponent::OnRep_SimulatedTasks()
{
	for (UGameplayTask* SimulatedTask : SimulatedTasks)
	{
		// Temp check 
		if (SimulatedTask && SimulatedTask->IsTickingTask() && TickingTasks.Contains(SimulatedTask) == false)
		{
			SimulatedTask->InitSimulatedTask(*this);
			if (TickingTasks.Num() == 0)
			{
				UpdateShouldTick();
			}

			TickingTasks.Add(SimulatedTask);
		}
	}
}

void UGameplayTasksComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	SCOPE_CYCLE_COUNTER(STAT_TickGameplayTasks);

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Because we have no control over what a task may do when it ticks, we must be careful.
	// Ticking a task may kill the task right here. It could also potentially kill another task
	// which was waiting on the original task to do something. Since when a tasks is killed, it removes
	// itself from the TickingTask list, we will make a copy of the tasks we want to service before ticking any

	int32 NumTickingTasks = TickingTasks.Num();
	int32 NumActuallyTicked = 0;
	switch (NumTickingTasks)
	{
	case 0:
		break;
	case 1:
		if (TickingTasks[0].IsValid())
		{
			TickingTasks[0]->TickTask(DeltaTime);
			NumActuallyTicked++;
		}
		break;
	default:
	{
		TArray<TWeakObjectPtr<UGameplayTask> > LocalTickingTasks = TickingTasks;
		for (TWeakObjectPtr<UGameplayTask>& TickingTask : LocalTickingTasks)
		{
			if (TickingTask.IsValid())
			{
				TickingTask->TickTask(DeltaTime);
				NumActuallyTicked++;
			}
		}
	}
		break;
	};

	// Stop ticking if no more active tasks
	if (NumActuallyTicked == 0)
	{
		TickingTasks.SetNum(0, false);
		UpdateShouldTick();
	}
}

bool UGameplayTasksComponent::GetShouldTick() const
{
	return TickingTasks.Num() > 0;
}

void UGameplayTasksComponent::RequestTicking()
{
	if (bIsActive == false)
	{
		SetActive(true);
	}
}

void UGameplayTasksComponent::UpdateShouldTick()
{
	const bool bShouldTick = GetShouldTick();	
	if (bIsActive != bShouldTick)
	{
		SetActive(bShouldTick);
	}
}

AActor* UGameplayTasksComponent::GetAvatarActor(const UGameplayTask* Task) const
{
	return GetOwner();
}

//----------------------------------------------------------------------//
// Priority and resources handling
//----------------------------------------------------------------------//
void UGameplayTasksComponent::AddTaskReadyForActivation(UGameplayTask& NewTask)
{
	UE_VLOG(this, LogGameplayTasks, Log, TEXT("AddTaskReadyForActivation %s"), *NewTask.GetName());

	ensure(NewTask.RequiresPriorityOrResourceManagement() == true);
	
	TaskEvents.Add(FGameplayTaskEventData(EGameplayTaskEvent::Add, NewTask));
	// trigger the actual processing only if it was the first event added to the list
	if (TaskEvents.Num() == 1)
	{
		ProcessTaskEvents();
	}
}

void UGameplayTasksComponent::RemoveResourceConsumingTask(UGameplayTask& Task)
{
	UE_VLOG(this, LogGameplayTasks, Log, TEXT("RemoveResourceConsumingTask %s"), *Task.GetName());

	TaskEvents.Add(FGameplayTaskEventData(EGameplayTaskEvent::Remove, Task));
	// trigger the actual processing only if it was the first event added to the list
	if (TaskEvents.Num() == 1)
	{
		ProcessTaskEvents();
	}
}

void UGameplayTasksComponent::EndAllResourceConsumingTasksOwnedBy(const IGameplayTaskOwnerInterface& TaskOwner)// , bool bNotifyOwner)
{
	for (int32 TaskIndex = TaskPriorityQueue.Num() - 1; TaskIndex >= 0; --TaskIndex)
	{
		if (TaskPriorityQueue[TaskIndex] == nullptr)
		{
			TaskPriorityQueue.RemoveAt(TaskIndex, 1, /*bAllowShrinking=*/false);
		}
		else if (TaskPriorityQueue[TaskIndex]->GetTaskOwner() == &TaskOwner)
		{
			UGameplayTask* Task = TaskPriorityQueue[TaskIndex];
			TaskPriorityQueue.RemoveAt(TaskIndex, 1, /*bAllowShrinking=*/false);
			Task->TaskOwnerEnded();
		}
	}

	UpdateTaskActivationFromIndex(0, FGameplayResourceSet(), FGameplayResourceSet());
}

void UGameplayTasksComponent::ProcessTaskEvents()
{
	static const int32 ErronousIterationsLimit = 100;

	// note that this function allows called functions to add new elements to 
	// TaskEvents array that the main loop is iterating over. It's a feature
	for (int32 EventIndex = 0; EventIndex < TaskEvents.Num(); ++EventIndex)
	{
		UE_VLOG(this, LogGameplayTasks, Verbose, TEXT("UGameplayTasksComponent::ProcessTaskEvents: %s event %s")
			, *TaskEvents[EventIndex].RelatedTask.GetName(), GetGameplayTaskEventName(TaskEvents[EventIndex].Event));

		if (TaskEvents[EventIndex].RelatedTask.IsPendingKill())
		{
			UE_VLOG(this, LogGameplayTasks, Verbose, TEXT("%s is PendingKill"), *TaskEvents[EventIndex].RelatedTask.GetName());
			// we should ignore it, but just in case run the removal code.
			RemoveTaskFromPriorityQueue(TaskEvents[EventIndex].RelatedTask);
			continue;
		}

		switch (TaskEvents[EventIndex].Event)
		{
		case EGameplayTaskEvent::Add:
			if (TaskEvents[EventIndex].RelatedTask.TaskState != EGameplayTaskState::Finished)
			{
				AddTaskToPriorityQueue(TaskEvents[EventIndex].RelatedTask);
			}
			else
			{
				UE_VLOG(this, LogGameplayTasks, Error, TEXT("UGameplayTasksComponent::ProcessTaskEvents trying to add a finished task to priority queue!"));
			}
			break;
		case EGameplayTaskEvent::Remove:
			RemoveTaskFromPriorityQueue(TaskEvents[EventIndex].RelatedTask);
			break;
		default:
			checkNoEntry();
			break;
		}

		if (EventIndex >= ErronousIterationsLimit)
		{
			UE_VLOG(this, LogGameplayTasks, Error, TEXT("UGameplayTasksComponent::ProcessTaskEvents has exceeded warning-level of iterations. Check your GameplayTasks for logic loops!"));
		}
	}

	TaskEvents.Reset();
}

void UGameplayTasksComponent::AddTaskToPriorityQueue(UGameplayTask& NewTask)
{
	// The generic idea is as follows:
	// 1. Find insertion/removal point X
	//		While looking for it add up all ResourcesUsedByActiveUpToX and ResourcesRequiredUpToX
	//		ResourcesUsedByActiveUpToX - resources required by ACTIVE tasks
	//		ResourcesRequiredUpToX - resources required by both active and inactive tasks on the way to X
	// 2. Insert Task at X
	// 3. Starting from X proceed down the queue
	//	a. If ConsideredTask.Resources overlaps with ResourcesRequiredUpToX then PAUSE it
	//	b. Else, ACTIVATE ConsideredTask and add ConsideredTask.Resources to ResourcesUsedByActiveUpToX
	//	c. Add ConsideredTask.Resources to ResourcesRequiredUpToX
	// 4. Set this->CurrentlyUsedResources to ResourcesUsedByActiveUpToX
	//
	// Points 3-4 are implemented as a separate function, UpdateTaskActivationFromIndex,
	// since it's a common code between adding and removing tasks
	
	const bool bStartOnTopOfSamePriority = NewTask.GetResourceOverlapPolicy() == ETaskResourceOverlapPolicy::StartOnTop;

	FGameplayResourceSet ResourcesClaimedUpToX;
	FGameplayResourceSet ResourcesBlockedUpToIndex;
	int32 InsertionPoint = INDEX_NONE;
	
	if (TaskPriorityQueue.Num() > 0)
	{
		for (int32 TaskIndex = 0; TaskIndex < TaskPriorityQueue.Num(); ++TaskIndex)
		{
			ensure(TaskPriorityQueue[TaskIndex]);
			if (TaskPriorityQueue[TaskIndex] == nullptr)
			{
				continue;
			}

			if ((bStartOnTopOfSamePriority && TaskPriorityQueue[TaskIndex]->GetPriority() <= NewTask.GetPriority())
				|| (!bStartOnTopOfSamePriority && TaskPriorityQueue[TaskIndex]->GetPriority() < NewTask.GetPriority()))
			{
				TaskPriorityQueue.Insert(&NewTask, TaskIndex);
				InsertionPoint = TaskIndex;
				break;
			}

			const FGameplayResourceSet ClaimedResources = TaskPriorityQueue[TaskIndex]->GetClaimedResources();
			if (TaskPriorityQueue[TaskIndex]->IsActive())
			{
				ResourcesClaimedUpToX.AddSet(ClaimedResources);
			}
			ResourcesBlockedUpToIndex.AddSet(ClaimedResources);
		}
	}
	
	if (InsertionPoint == INDEX_NONE)
	{
		TaskPriorityQueue.Add(&NewTask);
		InsertionPoint = TaskPriorityQueue.Num() - 1;
	}

	UpdateTaskActivationFromIndex(InsertionPoint, ResourcesClaimedUpToX, ResourcesBlockedUpToIndex);
}

void UGameplayTasksComponent::RemoveTaskFromPriorityQueue(UGameplayTask& Task)
{	
	const int32 RemovedTaskIndex = TaskPriorityQueue.Find(&Task);
	if (RemovedTaskIndex != INDEX_NONE)
	{
		if (TaskPriorityQueue.Num() > 1)
		{
			// sum up resources up to TaskIndex
			FGameplayResourceSet ResourcesClaimedUpToX;
			FGameplayResourceSet ResourcesBlockedUpToIndex;
			for (int32 TaskIndex = 0; TaskIndex < RemovedTaskIndex; ++TaskIndex)
			{
				ensure(TaskPriorityQueue[TaskIndex]);
				if (TaskPriorityQueue[TaskIndex] == nullptr)
				{
					continue;
				}

				const FGameplayResourceSet ClaimedResources = TaskPriorityQueue[TaskIndex]->GetClaimedResources();
				if (TaskPriorityQueue[TaskIndex]->IsActive())
				{
					ResourcesClaimedUpToX.AddSet(ClaimedResources);
				}				
				ResourcesBlockedUpToIndex.AddSet(ClaimedResources);
			}

			// don't forget to actually remove the task from the queue
			TaskPriorityQueue.RemoveAt(RemovedTaskIndex, 1, /*bAllowShrinking=*/false);

			// if it wasn't the last item then proceed as usual
			if (RemovedTaskIndex < TaskPriorityQueue.Num())
			{
				UpdateTaskActivationFromIndex(RemovedTaskIndex, ResourcesClaimedUpToX, ResourcesBlockedUpToIndex);
			}
			else
			{
				// no need to do extra processing. This was the last task, so 
				// ResourcesUsedByActiveUpToX is the CurrentlyUsedResources
				SetCurrentlyClaimedResources(ResourcesClaimedUpToX);
			}
		}
		else
		{
			TaskPriorityQueue.Pop(/*bAllowShrinking=*/false);
			SetCurrentlyClaimedResources(FGameplayResourceSet());
		}
	}
	else
	{
		// take a note and ignore
		UE_VLOG(this, LogGameplayTasks, Verbose, TEXT("RemoveTaskFromPriorityQueue for %s called, but it's not in the queue. Might have been already removed"), *Task.GetName());
	}
}

void UGameplayTasksComponent::UpdateTaskActivationFromIndex(int32 StartingIndex, FGameplayResourceSet ResourcesClaimedUpToIndex, FGameplayResourceSet ResourcesBlockedUpToIndex)
{
	if (TaskPriorityQueue.Num() > 0)
	{
		check(TaskPriorityQueue.IsValidIndex(StartingIndex));

		TArray<UGameplayTask*> TaskPriorityQueueCopy = TaskPriorityQueue;
		for (int32 TaskIndex = StartingIndex; TaskIndex < TaskPriorityQueueCopy.Num(); ++TaskIndex)
		{
			ensure(TaskPriorityQueueCopy[TaskIndex]);
			if (TaskPriorityQueueCopy[TaskIndex] == nullptr)
			{
				continue;
			}

			const FGameplayResourceSet RequiredResources = TaskPriorityQueueCopy[TaskIndex]->GetRequiredResources();
			const FGameplayResourceSet ClaimedResources = TaskPriorityQueueCopy[TaskIndex]->GetClaimedResources();
			if (RequiredResources.GetOverlap(ResourcesBlockedUpToIndex).IsEmpty())
			{
				TaskPriorityQueueCopy[TaskIndex]->ActivateInTaskQueue();
				ResourcesClaimedUpToIndex.AddSet(ClaimedResources);
			}
			else
			{
				TaskPriorityQueueCopy[TaskIndex]->PauseInTaskQueue();
			}
			ResourcesBlockedUpToIndex.AddSet(ClaimedResources);
		}
	}
	
	SetCurrentlyClaimedResources(ResourcesClaimedUpToIndex);
}

void UGameplayTasksComponent::SetCurrentlyClaimedResources(FGameplayResourceSet NewClaimedSet)
{
	if (CurrentlyClaimedResources != NewClaimedSet)
	{
		FGameplayResourceSet ReleasedResources = FGameplayResourceSet(CurrentlyClaimedResources).RemoveSet(NewClaimedSet);
		FGameplayResourceSet ClaimedResources = FGameplayResourceSet(NewClaimedSet).RemoveSet(CurrentlyClaimedResources);
		CurrentlyClaimedResources = NewClaimedSet;
		OnClaimedResourcesChange.Broadcast(ClaimedResources, ReleasedResources);
	}
}

//----------------------------------------------------------------------//
// debugging
//----------------------------------------------------------------------//
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && ENABLE_VISUAL_LOG
FString UGameplayTasksComponent::GetTickingTasksDescription() const
{
	FString TasksDescription;
	for (auto& Task : TickingTasks)
	{
		if (Task.IsValid())
		{
			TasksDescription += FString::Printf(TEXT("\n%s %s"), *GetTaskStateName(Task->GetState()), *Task->GetDebugDescription());
		}
		else
		{
			TasksDescription += TEXT("\nNULL");
		}
	}
	return TasksDescription;
}

FString UGameplayTasksComponent::GetTasksPriorityQueueDescription() const
{
	FString TasksDescription;
	for (auto Task : TaskPriorityQueue)
	{
		if (Task != nullptr)
		{
			TasksDescription += FString::Printf(TEXT("\n%s %s"), *GetTaskStateName(Task->GetState()), *Task->GetDebugDescription());
		}
		else
		{
			TasksDescription += TEXT("\nNULL");
		}
	}
	return TasksDescription;
}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if ENABLE_VISUAL_LOG
void UGameplayTasksComponent::DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const
{
	static const FString CategoryName = TEXT("GameplayTasks");
	static const FString TickingTasksName = TEXT("Ticking tasks");
	static const FString PriorityQueueName = TEXT("Priority Queue");

	if (IsPendingKill())
	{
		return;
	}

	FVisualLogStatusCategory StatusCategory(CategoryName);

	StatusCategory.Add(TickingTasksName, GetTickingTasksDescription());
	StatusCategory.Add(PriorityQueueName, GetTasksPriorityQueueDescription());

	Snapshot->Status.Add(StatusCategory);
}

FString UGameplayTasksComponent::GetTaskStateName(EGameplayTaskState Value)
{
	static const UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EGameplayTaskState"));
	check(Enum);
	return Enum->GetEnumName(int32(Value));
}
#endif // ENABLE_VISUAL_LOG

EGameplayTaskRunResult UGameplayTasksComponent::RunGameplayTask(IGameplayTaskOwnerInterface& TaskOwner, UGameplayTask& Task, uint8 Priority, FGameplayResourceSet AdditionalRequiredResources, FGameplayResourceSet AdditionalClaimedResources)
{
	const FText NoneText = FText::FromString(TEXT("None"));

	if (Task.GetState() == EGameplayTaskState::Paused || Task.GetState() == EGameplayTaskState::Active)
	{
		// return as success if already running for the same owner, failure otherwise 
		return Task.GetTaskOwner() == &TaskOwner
			? (Task.GetState() == EGameplayTaskState::Paused ? EGameplayTaskRunResult::Success_Paused : EGameplayTaskRunResult::Success_Active)
			: EGameplayTaskRunResult::Error;
	}

	// this is a valid situation if the task has been created via "Construct Object" mechanics
	if (Task.GetState() == EGameplayTaskState::Uninitialized)
	{
		Task.InitTask(TaskOwner, Priority);
	}

	Task.AddRequiredResourceSet(AdditionalRequiredResources);
	Task.AddClaimedResourceSet(AdditionalClaimedResources);
	Task.ReadyForActivation();

	switch (Task.GetState())
	{
	case EGameplayTaskState::AwaitingActivation:
	case EGameplayTaskState::Paused:
		return EGameplayTaskRunResult::Success_Paused;
		break;
	case EGameplayTaskState::Active:
		return EGameplayTaskRunResult::Success_Active;
		break;
	case EGameplayTaskState::Finished:
		return EGameplayTaskRunResult::Success_Active;
		break;
	}

	return EGameplayTaskRunResult::Error;
}

//----------------------------------------------------------------------//
// BP API
//----------------------------------------------------------------------//
EGameplayTaskRunResult UGameplayTasksComponent::K2_RunGameplayTask(TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner, UGameplayTask* Task, uint8 Priority, TArray<TSubclassOf<UGameplayTaskResource> > AdditionalRequiredResources, TArray<TSubclassOf<UGameplayTaskResource> > AdditionalClaimedResources)
{
	const FText NoneText = FText::FromString(TEXT("None"));

	if (TaskOwner.GetInterface() == nullptr)
	{
		FMessageLog("PIE").Error(FText::Format(
			LOCTEXT("RunGameplayTaskNullOwner", "Tried running a gameplay task {0} while owner is None!"),
			Task ? FText::FromName(Task->GetFName()) : NoneText));
		return EGameplayTaskRunResult::Error;
	}

	IGameplayTaskOwnerInterface& OwnerInstance = *TaskOwner;

	if (Task == nullptr)
	{
		FMessageLog("PIE").Error(FText::Format(
			LOCTEXT("RunNullGameplayTask", "Tried running a None task for {0}"),
			FText::FromString(Cast<UObject>(&OwnerInstance)->GetName())
			));
		return EGameplayTaskRunResult::Error;
	}

	if (Task->GetState() == EGameplayTaskState::Paused || Task->GetState() == EGameplayTaskState::Active)
	{
		FMessageLog("PIE").Warning(FText::Format(
			LOCTEXT("RunNullGameplayTask", "Tried running a None task for {0}"),
			FText::FromString(Cast<UObject>(&OwnerInstance)->GetName())
			));
		// return as success if already running for the same owner, failure otherwise 
		return Task->GetTaskOwner() == &OwnerInstance 
			? (Task->GetState() == EGameplayTaskState::Paused ? EGameplayTaskRunResult::Success_Paused : EGameplayTaskRunResult::Success_Active)
			: EGameplayTaskRunResult::Error;
	}

	// this is a valid situation if the task has been created via "Construct Object" mechanics
	if (Task->GetState() == EGameplayTaskState::Uninitialized)
	{
		Task->InitTask(OwnerInstance, Priority);
	}

	Task->AddRequiredResourceSet(AdditionalRequiredResources);
	Task->AddClaimedResourceSet(AdditionalClaimedResources);
	Task->ReadyForActivation();

	switch (Task->GetState())
	{
	case EGameplayTaskState::AwaitingActivation:
	case EGameplayTaskState::Paused:
		return EGameplayTaskRunResult::Success_Paused;
		break;
	case EGameplayTaskState::Active:
		return EGameplayTaskRunResult::Success_Active;
		break;
	case EGameplayTaskState::Finished:
		return EGameplayTaskRunResult::Success_Active;
		break;
	}

	return EGameplayTaskRunResult::Error;
}

//----------------------------------------------------------------------//
// FGameplayResourceSet
//----------------------------------------------------------------------//
FString FGameplayResourceSet::GetDebugDescription() const
{
	static const int32 FlagsCount = sizeof(FFlagContainer)* 8;
	TCHAR Description[FlagsCount + 1];
	FFlagContainer FlagsCopy = Flags;
	int32 FlagIndex = 0;
	for (; FlagIndex < FlagsCount && FlagsCopy != 0; ++FlagIndex)
	{
		Description[FlagIndex] = (FlagsCopy & (1 << FlagIndex)) ? TCHAR('1') : TCHAR('0');
		FlagsCopy &= ~(1 << FlagIndex);
	}
	Description[FlagIndex] = TCHAR('\0');
	return FString(Description);
}

#undef LOCTEXT_NAMESPACE