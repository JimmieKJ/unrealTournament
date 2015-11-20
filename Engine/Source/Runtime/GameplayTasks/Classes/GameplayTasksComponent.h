// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTaskOwnerInterface.h"
#include "GameplayTaskTypes.h"
#include "GameplayTask.h"
#include "GameplayTasksComponent.generated.h"

class FOutBunch;
class UActorChannel;

enum class EGameplayTaskEvent : uint8
{
	Add,
	Remove,
};

UENUM()
enum class EGameplayTaskRunResult : uint8
{
	/** When tried running a null-task*/
	Error,
	Failed,
	/** Successfully registered for running, but currently paused due to higher priority tasks running */
	Success_Paused,
	/** Successfully activated */
	Success_Active,
	/** Successfully activated, but finished instantly */
	Success_Finished,
};

struct FGameplayTaskEventData
{
	EGameplayTaskEvent Event;
	UGameplayTask& RelatedTask;

	FGameplayTaskEventData(EGameplayTaskEvent InEvent, UGameplayTask& InRelatedTask)
		: Event(InEvent), RelatedTask(InRelatedTask)
	{

	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnClaimedResourcesChangeSignature, FGameplayResourceSet, NewlyClaimed, FGameplayResourceSet, FreshlyReleased);

/**
*	The core ActorComponent for interfacing with the GameplayAbilities System
*/
UCLASS(ClassGroup = GameplayTasks, hidecategories = (Object, LOD, Lighting, Transform, Sockets, TextureStreaming), editinlinenew, meta = (BlueprintSpawnableComponent))
class GAMEPLAYTASKS_API UGameplayTasksComponent : public UActorComponent, public IGameplayTaskOwnerInterface
{
	GENERATED_BODY()

protected:
	/** Tasks that run on simulated proxies */
	UPROPERTY(ReplicatedUsing = OnRep_SimulatedTasks)
	TArray<UGameplayTask*> SimulatedTasks;

	UPROPERTY()
	TArray<UGameplayTask*> TaskPriorityQueue;
	
	/** Transient array of events whose main role is to avoid
	 *	long chain of recurrent calls if an activated/paused/removed task 
	 *	wants to push/pause/kill other tasks.
	 *	Note: this TaskEvents is assumed to be used in a single thread */
	TArray<FGameplayTaskEventData> TaskEvents;

	/** Array of currently active UAbilityTasks that require ticking */
	TArray<TWeakObjectPtr<UGameplayTask> > TickingTasks;

	/** Indicates what's the highest priority among currently running tasks */
	uint8 TopActivePriority;

	/** Resources used by currently active tasks */
	FGameplayResourceSet CurrentlyClaimedResources;

public:
	UPROPERTY(BlueprintReadWrite, Category = "Gameplay Tasks")
	FOnClaimedResourcesChangeSignature OnClaimedResourcesChange;

	UGameplayTasksComponent(const FObjectInitializer& ObjectInitializer);
	
	UFUNCTION()
	void OnRep_SimulatedTasks();

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(UActorChannel *Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void UpdateShouldTick();

	/** retrieves information whether this component should be ticking taken current
	*	activity into consideration*/
	virtual bool GetShouldTick() const;
	
	virtual UGameplayTasksComponent* GetGameplayTasksComponent(const UGameplayTask& Task) const override { return const_cast<UGameplayTasksComponent*>(this); }
	/** Adds a task (most often an UAbilityTask) to the list of tasks to be ticked */
	virtual void OnTaskActivated(UGameplayTask& Task) override;
	/** Removes a task (most often an UAbilityTask) task from the list of tasks to be ticked */
	virtual void OnTaskDeactivated(UGameplayTask& Task) override;

	virtual AActor* GetOwnerActor(const UGameplayTask* Task) const override { return GetOwner(); }
	virtual AActor* GetAvatarActor(const UGameplayTask* Task) const override;
		
	/** processes the task and figures out if it should get triggered instantly or wait
	 *	based on task's RequiredResources, Priority and ResourceOverlapPolicy */
	void AddTaskReadyForActivation(UGameplayTask& NewTask);

	void RemoveResourceConsumingTask(UGameplayTask& Task);
	void EndAllResourceConsumingTasksOwnedBy(const IGameplayTaskOwnerInterface& TaskOwner);

	FORCEINLINE FGameplayResourceSet GetCurrentlyUsedResources() const { return CurrentlyClaimedResources; }

	UFUNCTION(BlueprintCallable, DisplayName="Run Gameplay Task", Category = "Gameplay Tasks", meta = (AutoCreateRefTerm = "AdditionalRequiredResources, AdditionalClaimedResources", AdvancedDisplay = "AdditionalRequiredResources, AdditionalClaimedResources"))
	static EGameplayTaskRunResult K2_RunGameplayTask(TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner, UGameplayTask* Task, uint8 Priority, TArray<TSubclassOf<UGameplayTaskResource> > AdditionalRequiredResources, TArray<TSubclassOf<UGameplayTaskResource> > AdditionalClaimedResources);

	static EGameplayTaskRunResult RunGameplayTask(IGameplayTaskOwnerInterface& TaskOwner, UGameplayTask& Task, uint8 Priority, FGameplayResourceSet AdditionalRequiredResources, FGameplayResourceSet AdditionalClaimedResources);
	
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && ENABLE_VISUAL_LOG
	FString GetTickingTasksDescription() const;
	FString GetTasksPriorityQueueDescription() const;
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#if ENABLE_VISUAL_LOG
	static FString GetTaskStateName(EGameplayTaskState Value);
	void DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const;
#endif // ENABLE_VISUAL_LOG

protected:
	void RequestTicking();
	void ProcessTaskEvents();
	void UpdateTaskActivationFromIndex(int32 StartingIndex, FGameplayResourceSet ResourcesClaimedUpToIndex, FGameplayResourceSet ResourcesBlockedUpToIndex);

	void SetCurrentlyClaimedResources(FGameplayResourceSet NewClaimedSet);

private:
	/** called when a task gets ended with an external call, meaning not coming from UGameplayTasksComponent mechanics */
	void OnTaskEnded(UGameplayTask& Task);

	void AddTaskToPriorityQueue(UGameplayTask& NewTask);
	void RemoveTaskFromPriorityQueue(UGameplayTask& Task);
};