// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
// ActorComponent.cpp: Actor component implementation.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "TickableAttributeSetInterface.h"
#include "GameplayPrediction.h"

#include "Net/UnrealNetwork.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "MapErrors.h"
#define LOCTEXT_NAMESPACE "AbilitySystemComponent"

/** Enable to log out all render state create, destroy and updatetransform events */
#define LOG_RENDER_STATE 0

void UAbilitySystemComponent::InitializeComponent()
{
	Super::InitializeComponent();

	/** Allocate an AbilityActorInfo. Note: this goes through a global function and is a SharedPtr so projects can make their own AbilityActorInfo */
	AbilityActorInfo = TSharedPtr<FGameplayAbilityActorInfo>(UAbilitySystemGlobals::Get().AllocAbilityActorInfo());
	
	// Look for DSO AttributeSets (note we are currently requiring all attribute sets to be subobjects of the same owner. This doesn't *have* to be the case forever.
	AActor *Owner = GetOwner();
	InitAbilityActorInfo(Owner, Owner);	// Default init to our outer owner

	TArray<UObject*> ChildObjects;
	GetObjectsWithOuter(Owner, ChildObjects, false, RF_PendingKill);
	for (UObject* Obj : ChildObjects)
	{
		UAttributeSet* Set = Cast<UAttributeSet>(Obj);
		if (Set)  
		{
			UObject* AT = Set->GetArchetype();	
			SpawnedAttributes.Add(Set);
		}
	}
}

void UAbilitySystemComponent::UninitializeComponent()
{
	Super::UninitializeComponent();
	
	for (UAttributeSet* Set : SpawnedAttributes)
	{
		if (Set)
		{
			Set->MarkPendingKill();
		}
	}
}

void UAbilitySystemComponent::OnComponentDestroyed()
{
	// If we haven't already begun being destroyed
	if ((GetFlags() & RF_BeginDestroyed) == 0)
	{
		// Cancel all abilities before we are destroyed.
		CancelAbilities();

		// Mark pending kill any remainging instanced abilities
		// (CancelAbilities() will only MarkPending kill InstancePerExecution abilities).
		for (FGameplayAbilitySpec& Spec : ActivatableAbilities)
		{
			if (Spec.Ability && Spec.Ability->HasAnyFlags(RF_ClassDefaultObject) == false && Spec.Ability->IsPendingKill() == false)
			{
				Spec.Ability->MarkPendingKill();
			}
		}
	}

	// Call the super at the end, after we've done what we needed to do
	Super::OnComponentDestroyed();
}

void UAbilitySystemComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	SCOPE_CYCLE_COUNTER(STAT_TickAbilityTasks);

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (IsOwnerActorAuthoritative())
	{
		AnimMontage_UpdateReplicatedData();
	}

	// Because we have no control over what a task may do when it ticks, we must be careful.
	// Ticking a task may kill the task right here. It could also potentially kill another task
	// which was waiting on the original task to do something. Since when a tasks is killed, it removes
	// itself from the TickingTask list, we will make a copy of the tasks we want to service before ticking any

	int32 NumTickingTasks = TickingTasks.Num();
	int32 NumActuallyTicked = 0;
	switch(NumTickingTasks)
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
				TArray<TWeakObjectPtr<UAbilityTask> >	LocalTickingTasks = TickingTasks;
				for (TWeakObjectPtr<UAbilityTask>& TickingTask : LocalTickingTasks)
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

	for (UAttributeSet* AttributeSet : SpawnedAttributes)
	{
		ITickableAttributeSetInterface* TickableSet = Cast<ITickableAttributeSetInterface>(AttributeSet);
		if (TickableSet)
		{
			TickableSet->Tick(DeltaTime);
		}
	}
}

void UAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	check(AbilityActorInfo.IsValid());
	bool AvatarChanged = (InAvatarActor != AbilityActorInfo->AvatarActor);

	AbilityActorInfo->InitFromActor(InOwnerActor, InAvatarActor, this);

	OwnerActor = InOwnerActor;
	AvatarActor = InAvatarActor;

	if (AvatarChanged)
	{
		for (FGameplayAbilitySpec& Spec : ActivatableAbilities)
		{
			Spec.Ability->OnAvatarSet(AbilityActorInfo.Get(), Spec);
		}
	}
}

void UAbilitySystemComponent::UpdateShouldTick()
{
	bool bHasTickingTasks = (TickingTasks.Num() != 0);
	bool bHasReplicatedMontageInfoToUpdate = (IsOwnerActorAuthoritative() && RepAnimMontageInfo.IsStopped == false);
	bool bHasTickingAttributes = false;
	for (const UAttributeSet* AttributeSet : SpawnedAttributes)
	{
		const ITickableAttributeSetInterface* TickableAttributeSet = Cast<const ITickableAttributeSetInterface>(AttributeSet);
		if (TickableAttributeSet && TickableAttributeSet->ShouldTick())
		{
			bHasTickingAttributes = true;
		}
	}

	if (bHasTickingTasks || bHasReplicatedMontageInfoToUpdate || bHasTickingAttributes)
	{
		SetActive(true);
	}
	else
	{
		SetActive(false);
	}
}

void UAbilitySystemComponent::SetAvatarActor(AActor* InAvatarActor)
{
	check(AbilityActorInfo.IsValid());
	InitAbilityActorInfo(OwnerActor, InAvatarActor);
}

void UAbilitySystemComponent::ClearActorInfo()
{
	check(AbilityActorInfo.IsValid());
	AbilityActorInfo->ClearActorInfo();
	OwnerActor = NULL;
	AvatarActor = NULL;
}

void UAbilitySystemComponent::OnRep_OwningActor()
{
	check(AbilityActorInfo.IsValid());

	if (OwnerActor != AbilityActorInfo->OwnerActor || AvatarActor != AbilityActorInfo->AvatarActor)
	{
		if (OwnerActor != NULL)
		{
			InitAbilityActorInfo(OwnerActor, AvatarActor);
		}
		else
		{
			ClearActorInfo();
		}
	}
}

void UAbilitySystemComponent::RefreshAbilityActorInfo()
{
	check(AbilityActorInfo.IsValid());
	AbilityActorInfo->InitFromActor(AbilityActorInfo->OwnerActor.Get(), AbilityActorInfo->AvatarActor.Get(), this);
}

FGameplayAbilitySpecHandle UAbilitySystemComponent::GiveAbility(FGameplayAbilitySpec Spec)
{	
	check(Spec.Ability);
	check(IsOwnerActorAuthoritative());	// Should be called on authority
	
	FGameplayAbilitySpec& OwnedSpec = ActivatableAbilities[ActivatableAbilities.Add(Spec)];
	
	if (OwnedSpec.Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor)
	{
		OwnedSpec.Ability = CreateNewInstanceOfAbility(OwnedSpec, Spec.Ability);
	}
	
	OnGiveAbility(OwnedSpec);

	return OwnedSpec.Handle;
}

void UAbilitySystemComponent::ClearAllAbilities()
{
	check(IsOwnerActorAuthoritative());	// Should be called on authority

	// Note we aren't marking any old abilities pending kill. This shouldn't matter since they will be garbage collected.
	ActivatableAbilities.Empty(ActivatableAbilities.Num());

	CheckForClearedAbilities();
}

void UAbilitySystemComponent::ClearAbility(const FGameplayAbilitySpecHandle& Handle)
{
	check(IsOwnerActorAuthoritative()); // Should be called on authority

	for (int Idx = 0; Idx < ActivatableAbilities.Num(); ++Idx)
	{
		check(ActivatableAbilities[Idx].Handle.IsValid());
		if (ActivatableAbilities[Idx].Handle == Handle)
		{
			ActivatableAbilities.RemoveAtSwap(Idx);

			CheckForClearedAbilities();
			return;
		}
	}
}

void UAbilitySystemComponent::OnGiveAbility(const FGameplayAbilitySpec Spec)
{
	if (!Spec.Ability)
	{
		return;
	}

	for (const FAbilityTriggerData& TriggerData : Spec.Ability->AbilityTriggers)
	{
		FGameplayTag EventTag = TriggerData.TriggerTag;

		if (GameplayEventTriggeredAbilities.Contains(EventTag))
		{
			GameplayEventTriggeredAbilities[EventTag].AddUnique(Spec.Handle);	// Fixme: is this right? Do we want to trigger the ability directly of the spec?
		}
		else
		{
			TArray<FGameplayAbilitySpecHandle> Triggers;
			Triggers.Add(Spec.Handle);
			GameplayEventTriggeredAbilities.Add(EventTag, Triggers);
		}
	}

	Spec.Ability->OnGiveAbility(AbilityActorInfo.Get(), Spec);
}

void UAbilitySystemComponent::CheckForClearedAbilities()
{
	for (auto& Triggered : GameplayEventTriggeredAbilities)
	{
		// Make sure all triggered abilities still exist, if not remove
		for (int32 i = 0; i < Triggered.Value.Num(); i++)
		{
			FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Triggered.Value[i]);

			if (!Spec)
			{
				Triggered.Value.RemoveAt(i);
				i--;
			}
		}
		
		// We leave around the empty trigger stub, it's likely to be added again
	}
}


FGameplayAbilitySpec* UAbilitySystemComponent::FindAbilitySpecFromHandle(FGameplayAbilitySpecHandle Handle)
{
	SCOPE_CYCLE_COUNTER(STAT_FindAbilitySpecFromHandle);

	for (FGameplayAbilitySpec& Spec : ActivatableAbilities)
	{
		if (Spec.Handle == Handle)
		{
			return &Spec;
		}
	}

	return nullptr;
}

FGameplayAbilitySpec* UAbilitySystemComponent::FindAbilitySpecFromInputID(int32 InputID)
{
	if (InputID != INDEX_NONE)
	{
		for (FGameplayAbilitySpec& Spec : ActivatableAbilities)
		{
			if (Spec.InputID == InputID)
			{
				return &Spec;
			}
		}
	}
	return nullptr;
}

UGameplayAbility* UAbilitySystemComponent::CreateNewInstanceOfAbility(FGameplayAbilitySpec& Spec, UGameplayAbility* Ability)
{
	check(Ability);
	check(Ability->HasAllFlags(RF_ClassDefaultObject));

	AActor* OwnerActor = GetOwner();
	check(OwnerActor);

	UGameplayAbility * AbilityInstance = ConstructObject<UGameplayAbility>(Ability->GetClass(), OwnerActor);
	check(AbilityInstance);

	// Add it to one of our instance lists so that it doesn't GC.
	if (AbilityInstance->GetReplicationPolicy() != EGameplayAbilityReplicationPolicy::ReplicateNo)
	{
		Spec.ReplicatedInstances.Add(AbilityInstance);
		AllReplicatedInstancedAbilities.Add(AbilityInstance);
	}
	else
	{
		Spec.NonReplicatedInstances.Add(AbilityInstance);
	}
	
	return AbilityInstance;
}

void UAbilitySystemComponent::NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability)
{
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	check(Spec);
	check(Ability);

	// If AnimatingAbility ended, clear the pointer
	if (LocalAnimMontageInfo.AnimatingAbility == Ability)
	{
		ClearAnimatingAbility(Ability);
	}

	Spec->ActiveCount--;
	
	/** If this is instanced per execution, mark pending kill and remove it from our instanced lists if we are the authority */
	if (Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution)
	{
		check(Ability->HasAnyFlags(RF_ClassDefaultObject) == false);	// Should never be calling this on a CDO for an instanced ability!

		if (Ability->GetReplicationPolicy() != EGameplayAbilityReplicationPolicy::ReplicateNo)
		{
			if (GetOwnerRole() == ROLE_Authority)
			{
				Spec->ReplicatedInstances.Remove(Ability);
				AllReplicatedInstancedAbilities.Remove(Ability);
				Ability->MarkPendingKill();
			}
		}
		else
		{
			Spec->NonReplicatedInstances.Remove(Ability);
			Ability->MarkPendingKill();
		}
	}
}

void UAbilitySystemComponent::CancelAbilities(const FGameplayTagContainer* WithTags, const FGameplayTagContainer* WithoutTags, UGameplayAbility* Ignore)
{
	FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	FGameplayAbilityActivationInfo ActivationInfo;

	for (FGameplayAbilitySpec& Spec : ActivatableAbilities)
	{
		bool WithTagPass = (!WithTags || Spec.Ability->AbilityTags.MatchesAny(*WithTags, false));
		bool WithoutTagPass = (!WithoutTags || !Spec.Ability->AbilityTags.MatchesAny(*WithoutTags, false));

		if (Spec.IsActive() && Spec.Ability && WithTagPass && WithoutTagPass)
		{
			if (Spec.Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution)
			{
				// We need to cancel spawned instance, not the CDO
				TArray<UGameplayAbility*> AbilitiesToCancel = Spec.GetAbilityInstances();
				for (UGameplayAbility* InstanceAbility : AbilitiesToCancel)
				{
					if (InstanceAbility && Ignore != InstanceAbility)
					{
						InstanceAbility->CancelAbility(Spec.Handle, ActorInfo, ActivationInfo);
					}
				}
			}
			else
			{
				Spec.Ability->CancelAbility(Spec.Handle, ActorInfo, ActivationInfo);
				check(!Spec.IsActive() || Spec.Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::NonInstanced);
			}
		}
	}
}

void UAbilitySystemComponent::BlockAbilitiesWithTags(const FGameplayTagContainer Tags)
{
	BlockedAbilityTags.UpdateTagCount(Tags, 1);
}

void UAbilitySystemComponent::UnBlockAbilitiesWithTags(const FGameplayTagContainer Tags)
{
	BlockedAbilityTags.UpdateTagCount(Tags, -1);
}

void UAbilitySystemComponent::BlockAbilityByInputID(int32 InputID)
{
	if (InputID >= 0 && InputID < BlockedAbilityBindings.Num())
	{
		++BlockedAbilityBindings[InputID];
	}
}

void UAbilitySystemComponent::UnBlockAbilityByInputID(int32 InputID)
{
	if (InputID >= 0 && InputID < BlockedAbilityBindings.Num() && BlockedAbilityBindings[InputID] > 0)
	{
		--BlockedAbilityBindings[InputID];
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
int32 DenyClientActivation = 0;
static FAutoConsoleVariableRef CVarDenyClientActivation(
TEXT("AbilitySystem.DenyClientActivations"),
	DenyClientActivation,
	TEXT("Make server deny the next X ability activations from clients. For testing misprediction."),
	ECVF_Default
	);
#endif

void UAbilitySystemComponent::OnRep_ActivateAbilities()
{
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities)
	{
		OnGiveAbility(Spec);
	}

	CheckForClearedAbilities();
}

void UAbilitySystemComponent::GetActivateableGameplayAbilitySpecsByTag(const FGameplayTagContainer& GameplayTagContainer, TArray < struct FGameplayAbilitySpec* >& MatchingGameplayAbilities) const
{
	for (int32 AbilityIndex = 0; AbilityIndex < ActivatableAbilities.Num(); ++AbilityIndex)
	{
		const FGameplayAbilitySpec* AbilitySpecIter = &ActivatableAbilities[AbilityIndex];
		
		if (AbilitySpecIter->Ability->AbilityTags.MatchesAll(GameplayTagContainer, false))
		{
			MatchingGameplayAbilities.Add(const_cast<FGameplayAbilitySpec*>(AbilitySpecIter));
		}
	}
}

UGameplayAbility* UAbilitySystemComponent::TryActivateAbilityByTag(const FGameplayTagContainer& GameplayTagContainer)
{
	TArray<FGameplayAbilitySpec*> AbilitiesToActivate;
	GetActivateableGameplayAbilitySpecsByTag(GameplayTagContainer, AbilitiesToActivate);

	UGameplayAbility* ActivatedAbility(nullptr);

	for (auto GameplayAbilitySpec : AbilitiesToActivate)
	{
		TryActivateAbility(GameplayAbilitySpec->Handle, FPredictionKey(), &ActivatedAbility);
	}

	return ActivatedAbility;
}

/**
 * Attempts to activate the ability.
 *	-This function calls CanActivateAbility
 *	-This function handles instancing
 *	-This function handles networking and prediction
 *	-If all goes well, CallActivateAbility is called next.
 */
bool UAbilitySystemComponent::TryActivateAbility(FGameplayAbilitySpecHandle Handle, FPredictionKey InPredictionKey, UGameplayAbility** OutInstancedAbility, FOnGameplayAbilityEnded* OnGameplayAbilityEndedDelegate)
{
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		ABILITY_LOG(Warning, TEXT("TryActivateAbility called with invalid Handle"));
		return false;
	}

	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();

	// make sure the ActorInfo and then Actor on that FGameplayAbilityActorInfo are valid, if not bail out.
	if (ActorInfo == NULL || !ActorInfo->OwnerActor.IsValid() || !ActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

	// This should only come from button presses/local instigation (AI, etc)
	ENetRole NetMode = ActorInfo->AvatarActor->Role;
	ensure(NetMode != ROLE_SimulatedProxy);

	UGameplayAbility* Ability = Spec->Ability;

	if (!Ability)
	{
		ABILITY_LOG(Warning, TEXT("TryActivateAbility called with invalid Ability"));
		return false;
	}

	// Check if this ability's input binding is currently blocked
	if (Spec->InputID >= 0 && Spec->InputID < BlockedAbilityBindings.Num() && BlockedAbilityBindings[Spec->InputID] > 0)
	{
		return false;
	}

	// Check if any of this ability's tags are currently blocked
	if (Ability->AbilityTags.MatchesAny(BlockedAbilityTags.GetExplicitGameplayTags(), false))
	{
		return false;
	}

	// Always do a non instanced CanActivate check
	if (!Ability->CanActivateAbility(Handle, ActorInfo))
	{
		return false;
	}

	if (Spec->IsActive() && Ability->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
	{
		// Don't have a great solution for ending non instanced abilities on the server. So only warn if this was instanced.
		ABILITY_LOG(Warning, TEXT("TryActivateAbility called when ability was already active. NetMode: %d. Ability: %s"), (int32)NetMode, *Ability->GetName());
	}
	
	Spec->ActiveCount++;

	// Setup a fresh ActivationInfo for this AbilitySpec.
	Spec->ActivationInfo = FGameplayAbilityActivationInfo(ActorInfo->OwnerActor.Get());
	FGameplayAbilityActivationInfo &ActivationInfo = Spec->ActivationInfo;

	UGameplayAbility* InstancedAbility = Ability;

	// If we are the server or this is a client authoritative 
	if (Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::Client || (NetMode == ROLE_Authority))
	{
		// Create instance of this ability if necessary
		if (Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution)
		{
			InstancedAbility = CreateNewInstanceOfAbility(*Spec, Ability);
			ActivationInfo.bCanBeEndedByOtherInstance = Ability->bServerRespectsRemoteAbilityCancelation;
			InstancedAbility->CallActivateAbility(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate);
			if (OutInstancedAbility)
			{
				*OutInstancedAbility = InstancedAbility;
			}
		}
		else
		{
			Ability->CallActivateAbility(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate);
		}
	}
	else if (Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::Server)
	{
		ServerTryActivateAbility(Handle, Spec->InputPressed, FPredictionKey());
	}
	else if (Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::Predictive)
	{
		// This execution is now officially EGameplayAbilityActivationMode:Predicting and has a PredictionKey
		FScopedPredictionWindow ScopedPredictionWindow(this, true);

		ActivationInfo.SetPredicting(ScopedPredictionKey);
		
		// This must be called immediately after GeneratePredictionKey to prevent problems with recursively activating abilities
		ServerTryActivateAbility(Handle, Spec->InputPressed, ScopedPredictionKey);

		// If this PredictionKey is rejected, we will call OnClientActivateAbilityFailed.
		ScopedPredictionKey.NewRejectedDelegate().BindUObject(this, &UAbilitySystemComponent::OnClientActivateAbilityFailed, Handle, ScopedPredictionKey.Current);

		if (Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution)
		{
			// For now, only NonReplicated + InstancedPerExecution abilities can be Predictive.
			// We lack the code to predict spawning an instance of the execution and then merge/combine
			// with the server spawned version when it arrives.

			if (Ability->GetReplicationPolicy() == EGameplayAbilityReplicationPolicy::ReplicateNo)
			{
				InstancedAbility = CreateNewInstanceOfAbility(*Spec, Ability);
				InstancedAbility->CallActivateAbility(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate);
				if (OutInstancedAbility)
				{
					*OutInstancedAbility = InstancedAbility;
				}
			}
			else
			{
				ABILITY_LOG(Error, TEXT("TryActivateAbility called on ability %s that is InstancePerExecution and Replicated. This is an invalid configuration."), *Ability->GetName() );
			}
		}
		else
		{
			Ability->CallActivateAbility(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate);
		}
	}
	
	InstancedAbility->SetCurrentActivationInfo(ActivationInfo);	// Need to push this to the ability if it was instanced.

	return true;
}

void UAbilitySystemComponent::ServerTryActivateAbility_Implementation(FGameplayAbilitySpecHandle Handle, bool InputPressed, FPredictionKey PredictionKey)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (DenyClientActivation > 0)
	{
		DenyClientActivation--;
		ClientActivateAbilityFailed(Handle, PredictionKey.Current);
		return;
	}
#endif

	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		// Can potentially happen in race conditions where client tries to activate ability that is removed server side before it is received.
		return;
	}

	FScopedPredictionWindow ScopedPredictionWindow(this, PredictionKey);

	UGameplayAbility* AbilityToActivate = Spec->Ability;

	ensure(AbilityToActivate);
	ensure(AbilityActorInfo.IsValid());

	// if this was triggered by a predicted ability the server triggered copy should be the canonical one
	if (PredictionKey.Base != 0)
	{
		// first check if the server has already started this ability
		for (int32 ExecutedIdx = 0; ExecutedIdx < ExecutingServerAbilities.Num(); ++ExecutedIdx)
		{
			FExecutingAbilityInfo& ExecutingAbilityInfo = ExecutingServerAbilities[ExecutedIdx];
			if (ExecutingAbilityInfo.PredictionKey.Current == PredictionKey.Base && ExecutingAbilityInfo.Handle == Handle)
			{
				switch (ExecutingAbilityInfo.State)
				{
				case EAbilityExecutionState::Failed:
					ClientActivateAbilityFailed(Handle, PredictionKey.Current);
					break;
				case EAbilityExecutionState::Executing:
				case EAbilityExecutionState::Succeeded:
					ClientActivateAbilitySucceed(Handle, PredictionKey.Current);
					//Client commands to end the ability that come in after this point are considered for this instance
					if (AbilityToActivate->HasAnyFlags(RF_ClassDefaultObject) == false)
					{
						//Only applies to instanced abilities
						AbilityToActivate->CurrentActivationInfo.bCanBeEndedByOtherInstance = true;
					}
					break;
				}

				ExecutingServerAbilities.RemoveAtSwap(ExecutedIdx);
				return;
			}
		}

		FPendingAbilityInfo AbilityInfo;
		AbilityInfo.PredictionKey = PredictionKey;
		AbilityInfo.Handle = Handle;

		PendingClientAbilities.Add(AbilityInfo);
		return;
	}

	UGameplayAbility* InstancedAbility = NULL;
	Spec->InputPressed = InputPressed; // Tasks that check input may need this to be right immediately on ability startup.

	// Attempt to activate the ability (server side) and tell the client if it succeeded or failed.
	if (TryActivateAbility(Handle, PredictionKey, &InstancedAbility))
	{
		ClientActivateAbilitySucceed(Handle, PredictionKey.Current);
		//Client commands to end the ability that come in after this point are considered for this instance
		if (InstancedAbility)
		{
			//Only applies to instanced abilities
			InstancedAbility->CurrentActivationInfo.bCanBeEndedByOtherInstance = true;
		}
	}
	else
	{
		ClientActivateAbilityFailed(Handle, PredictionKey.Current);
	}
}

bool UAbilitySystemComponent::ServerTryActivateAbility_Validate(FGameplayAbilitySpecHandle Handle, bool InputPressed, FPredictionKey PredictionKey)
{
	return true;
}

//This is only called when ending an ability in response to a remote instruction.
void UAbilitySystemComponent::EndAbility(FGameplayAbilitySpecHandle AbilityToEnd)
{
	FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(AbilityToEnd);
	if (AbilitySpec && AbilitySpec->Ability && AbilitySpec->IsActive() && AbilitySpec->ActivationInfo.bCanBeEndedByOtherInstance)
	{
		for (auto Instance : AbilitySpec->NonReplicatedInstances)
		{
			Instance->EndAbility();
		}
	}
}

void UAbilitySystemComponent::ServerEndAbility_Implementation(FGameplayAbilitySpecHandle AbilityToEnd)
{
	EndAbility(AbilityToEnd);
}

bool UAbilitySystemComponent::ServerEndAbility_Validate(FGameplayAbilitySpecHandle AbilityToActivate)
{
	return true;
}

void UAbilitySystemComponent::ClientEndAbility_Implementation(FGameplayAbilitySpecHandle AbilityToEnd)
{
	EndAbility(AbilityToEnd);
}

static_assert(sizeof(int16) == sizeof(FPredictionKey::KeyType), "Sizeof PredictionKey::KeyType does not match RPC parameters in AbilitySystemComponent ClientActivateAbilityFailed_Implementation");

void UAbilitySystemComponent::ClientActivateAbilityFailed_Implementation(FGameplayAbilitySpecHandle Handle, int16 PredictionKey)
{
	// If this was predicted, we must use the PredictionKey system to end the ability
	if (PredictionKey > 0)
	{
		FPredictionKeyDelegates::BroadcastRejectedDelegate(PredictionKey);
	}

	// If this was not predicted... there is not much to do right now. We may need to keep track on the AbilitySpec if we are waiting on a confirm in these cases.
}

void UAbilitySystemComponent::OnClientActivateAbilityFailed(FGameplayAbilitySpecHandle Handle, FPredictionKey::KeyType PredictionKey)
{
	// Find the actual UGameplayAbility		
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);

	ABILITY_LOG(Warning, TEXT("ClientActivateAbilityFailed_Implementation. PredictionKey :%d"), PredictionKey);

	TArray<UGameplayAbility*> Instances = Spec->GetAbilityInstances();
	for (UGameplayAbility* Ability : Instances)
	{
		if (ScopedPredictionKey.Current == PredictionKey)
		{
			ABILITY_LOG(Warning, TEXT("Ending Ability %s"), *Ability->GetName());
			Ability->K2_EndAbility();
		}
	}
}


static_assert(sizeof(int16) == sizeof(FPredictionKey::KeyType), "Sizeof PredictionKey::KeyType does not match RPC parameters in AbilitySystemComponent ClientActivateAbilitySucceed_Implementation");

void UAbilitySystemComponent::ClientActivateAbilitySucceed_Implementation(FGameplayAbilitySpecHandle Handle, int16 PredictionKey)
{
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		// Can potentially happen in race conditions where client tries to activate ability that is removed server side before it is received.
		return;
	}

	UGameplayAbility* AbilityToActivate = Spec->Ability;

	ensure(AbilityToActivate);
	ensure(AbilityActorInfo.IsValid());

	Spec->ActivationInfo.SetActivationConfirmed();

	// Fixme: We need a better way to link up/reconcile predictive replicated abilities. It would be ideal if we could predictively spawn an
	// ability and then replace/link it with the server spawned one once the server has confirmed it.

	if (AbilityToActivate->NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::Predictive)
	{
		if (AbilityToActivate->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::NonInstanced)
		{
			AbilityToActivate->ConfirmActivateSucceed();
		}
		else
		{
			// Find the one we predictively spawned, tell them we are confirmed
			bool found = false;
			for (UGameplayAbility* LocalAbility : Spec->NonReplicatedInstances)				// Fixme: this has to be updated once predictive abilities can replicate
			{
				if (LocalAbility->GetCurrentActivationInfo().GetActivationPredictionKey().Current == PredictionKey)
				{
					LocalAbility->ConfirmActivateSucceed();
					found = true;
					break;
				}
			}

			if (!found)
			{
				ABILITY_LOG(Verbose, TEXT("Ability %s was confirmed by server but no longer exists on client (replication key: %d"), *AbilityToActivate->GetName(), PredictionKey);
			}
		}
	}
	else
	{
		// We haven't already executed this ability at all, so kick it off.
		
		if (AbilityToActivate->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution)
		{
			// Need to instantiate this in order to execute
			UGameplayAbility* InstancedAbility = CreateNewInstanceOfAbility(*Spec, AbilityToActivate);
			InstancedAbility->CallActivateAbility(Handle, AbilityActorInfo.Get(), Spec->ActivationInfo);
		}
		else
		{
			AbilityToActivate->CallActivateAbility(Handle, AbilityActorInfo.Get(), Spec->ActivationInfo);
		}
	}
}

void UAbilitySystemComponent::ClientAbilityNotifyRejected_Implementation(int32 InputID)
{
	TargetingRejectedConfirmationDelegate.Broadcast(InputID);
}

void UAbilitySystemComponent::TriggerAbilityFromGameplayEvent(FGameplayAbilitySpecHandle Handle, FGameplayAbilityActorInfo* ActorInfo, FGameplayTag EventTag, FGameplayEventData* Payload, UAbilitySystemComponent& Component)
{
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!ensure(Spec))
	{
		return;
	}

	UGameplayAbility* Ability = Spec->Ability;
	if (!ensure(Ability))
	{
		return;
	}

	Ability->SetCurrentActorInfo(Handle, ActorInfo);

	if (Ability->ShouldAbilityRespondToEvent(EventTag, Payload))
	{
		int32 ExecutingAbilityIndex = -1;

		// if we're the server and this is coming from a predicted event we should check if the client has already predicted it
		if (ScopedPredictionKey.IsValidKey()
			&& Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::Predictive
			&& ActorInfo->OwnerActor->Role == ROLE_Authority)
		{
			bool bPendingClientAbilityFound = false;
			for (auto PendingAbilityInfo : Component.PendingClientAbilities)
			{
				if (ScopedPredictionKey.Current == PendingAbilityInfo.PredictionKey.Base && Handle == PendingAbilityInfo.Handle) // found a match
				{
					Component.PendingClientAbilities.RemoveSingleSwap(PendingAbilityInfo);
					bPendingClientAbilityFound = true;
					break;
				}
			}

			// we haven't received the client's copy of the triggered ability
			// keep track of this so we can associate the prediction keys when it comes in
			if (bPendingClientAbilityFound == false)
			{
				UAbilitySystemComponent::FExecutingAbilityInfo Info;
				Info.PredictionKey = ScopedPredictionKey;
				Info.Handle = Handle;

				ExecutingAbilityIndex = Component.ExecutingServerAbilities.Add(Info);
			}
		}

		if (TryActivateAbility(Handle, ScopedPredictionKey))
		{
			if (ExecutingAbilityIndex >= 0)
			{
				Component.ExecutingServerAbilities[ExecutingAbilityIndex].State = UAbilitySystemComponent::EAbilityExecutionState::Succeeded;
			}
		}
		else if (ExecutingAbilityIndex >= 0)
		{
			Component.ExecutingServerAbilities[ExecutingAbilityIndex].State = UAbilitySystemComponent::EAbilityExecutionState::Failed;
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------------------------------
//								Input 
// ----------------------------------------------------------------------------------------------------------------------------------------------------

bool UAbilitySystemComponent::GetUserAbilityActivationInhibited() const
{
	return UserAbilityActivationInhibited;
}

void UAbilitySystemComponent::SetUserAbilityActivationInhibited(bool NewInhibit)
{
	if(AbilityActorInfo->IsLocallyControlled())
	{
		if (NewInhibit && UserAbilityActivationInhibited)
		{
			// This could cause problems if two sources try to inhibit ability activation, it is not clear when the ability should be uninhibited
			ABILITY_LOG(Warning, TEXT("Call to SetUserAbilityActivationInhibited(true) when UserAbilityActivationInhibited was already true"));
		}

		UserAbilityActivationInhibited = NewInhibit;
	}
}

void UAbilitySystemComponent::NotifyAbilityCommit(UGameplayAbility* Ability)
{
	AbilityCommitedCallbacks.Broadcast(Ability);
}

void UAbilitySystemComponent::NotifyAbilityActivated(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability)
{
	AbilityActivatedCallbacks.Broadcast(Ability);
}

void UAbilitySystemComponent::HandleGameplayEvent(FGameplayTag EventTag, FGameplayEventData* Payload)
{
	if (GameplayEventTriggeredAbilities.Contains(EventTag))
	{		
		TArray<FGameplayAbilitySpecHandle> TriggeredAbilityHandles = GameplayEventTriggeredAbilities[EventTag];

		for (auto AbilityHandle : TriggeredAbilityHandles)
		{
			TriggerAbilityFromGameplayEvent(AbilityHandle, AbilityActorInfo.Get(), EventTag, Payload, *this);
		}
	}
}

// ----------------------------------------------------------------------------------------------------------------------------------------------------
//								Input 
// ----------------------------------------------------------------------------------------------------------------------------------------------------


void UAbilitySystemComponent::BindToInputComponent(UInputComponent* InputComponent)
{
	static const FName ConfirmBindName(TEXT("AbilityConfirm"));
	static const FName CancelBindName(TEXT("AbilityCancel"));

	// Pressed event
	{
		FInputActionBinding AB(ConfirmBindName, IE_Pressed);
		AB.ActionDelegate.GetDelegateForManualSet().BindUObject(this, &UAbilitySystemComponent::InputConfirm);
		InputComponent->AddActionBinding(AB);
	}

	// 
	{
		FInputActionBinding AB(CancelBindName, IE_Pressed);
		AB.ActionDelegate.GetDelegateForManualSet().BindUObject(this, &UAbilitySystemComponent::InputCancel);
		InputComponent->AddActionBinding(AB);
	}
}

void UAbilitySystemComponent::BindAbilityActivationToInputComponent(UInputComponent* InputComponent, FGameplayAbiliyInputBinds BindInfo)
{
	UEnum* EnumBinds = BindInfo.GetBindEnum();
	BlockedAbilityBindings.SetNumZeroed(EnumBinds->NumEnums());

	for(int32 idx=0; idx < EnumBinds->NumEnums(); ++idx)
	{
		FString FullStr = EnumBinds->GetEnum(idx).ToString();
		FString BindStr;

		FullStr.Split(TEXT("::"), nullptr, &BindStr);

		// Pressed event
		{
			FInputActionBinding AB(FName(*BindStr), IE_Pressed);
			AB.ActionDelegate.GetDelegateForManualSet().BindUObject(this, &UAbilitySystemComponent::AbilityInputPressed, idx);
			InputComponent->AddActionBinding(AB);
		}

		// Released event
		{
			FInputActionBinding AB(FName(*BindStr), IE_Released);
			AB.ActionDelegate.GetDelegateForManualSet().BindUObject(this, &UAbilitySystemComponent::AbilityInputReleased, idx);
			InputComponent->AddActionBinding(AB);
		}
	}

	// Bind Confirm/Cancel. Note: these have to come last!
	{
		FInputActionBinding AB(FName(*BindInfo.ConfirmTargetCommand), IE_Pressed);
		AB.ActionDelegate.GetDelegateForManualSet().BindUObject(this, &UAbilitySystemComponent::InputConfirm);
		InputComponent->AddActionBinding(AB);
	}
	
	{
		FInputActionBinding AB(FName(*BindInfo.CancelTargetCommand), IE_Pressed);
		AB.ActionDelegate.GetDelegateForManualSet().BindUObject(this, &UAbilitySystemComponent::InputCancel);
		InputComponent->AddActionBinding(AB);
	}

}

void UAbilitySystemComponent::AbilityInputPressed(int32 InputID)
{
	// FIXME: Maps or multicast delegate to actually do this
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities)
	{
		if (Spec.InputID == InputID)
		{
			if (Spec.Ability)
			{
				Spec.InputPressed = true;
				if (Spec.IsActive())
				{
					// The ability is active, so just pipe the input event to it
					if (Spec.Ability->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::InstancedPerExecution)
					{
						Spec.Ability->InputPressed(Spec.Handle, AbilityActorInfo.Get(), Spec.ActivationInfo);
					}
					else
					{
						TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
						for (UGameplayAbility* Instance : Instances)
						{
							Instance->InputPressed(Spec.Handle, AbilityActorInfo.Get(), Spec.ActivationInfo);
						}						
					}

					FScopedPredictionWindow ScopedPrediction(this, true);
					//We don't require (AbilityKeyPressCallbacks.IsBound() || AbilityKeyReleaseCallbacks.IsBound())), because we don't necessarily know now if we'll need this data later.
					if (GetOwnerRole() != ROLE_Authority)
					{
						// Tell the server we pressed input.
						ServerSetReplicatedAbilityKeyState(InputID, true, ScopedPredictionKey);
					}
					AbilityKeyPressCallbacks.Broadcast(InputID);
				}
				else
				{
					// Ability is not active, so try to activate it
					TryActivateAbility(Spec.Handle);
				}
			}
		}
	}
}

void UAbilitySystemComponent::AbilityInputReleased(int32 InputID)
{
	// FIXME: Maps or multicast delegate to actually do this
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities)
	{
		if (Spec.InputID == InputID)
		{
			if (Spec.Ability)
			{
				AbilitySpecInputReleased(Spec);
				FPredictionKey NewKey = FPredictionKey::CreateNewPredictionKey(this);
				FScopedPredictionWindow ScopedPrediction(this, true);
				//We don't require (AbilityKeyPressCallbacks.IsBound() || AbilityKeyReleaseCallbacks.IsBound())), because we don't necessarily know now if we'll need this data later.
				if (GetOwnerRole() != ROLE_Authority)
				{
					// Tell the server we released input.
					ServerSetReplicatedAbilityKeyState(InputID, false, ScopedPredictionKey);
				}
				AbilityKeyReleaseCallbacks.Broadcast(InputID);
			}
		}
	}
}

void UAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Spec.InputPressed = true;
	if (Spec.IsActive())
	{
		// The ability is active, so just pipe the input event to it
		if (Spec.Ability->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::InstancedPerExecution)
		{
			Spec.Ability->InputPressed(Spec.Handle, AbilityActorInfo.Get(), Spec.ActivationInfo);
		}
		else
		{
			TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
			for (UGameplayAbility* Instance : Instances)
			{
				Instance->InputPressed(Spec.Handle, AbilityActorInfo.Get(), Spec.ActivationInfo);
			}
		}
	}
}

void UAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Spec.InputPressed = false;
	if (Spec.IsActive())
	{
		// The ability is active, so just pipe the input event to it
		if (Spec.Ability->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::InstancedPerExecution)
		{
			Spec.Ability->InputReleased(Spec.Handle, AbilityActorInfo.Get(), Spec.ActivationInfo);
		}
		else
		{
			TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
			for (UGameplayAbility* Instance : Instances)
			{
				Instance->InputReleased(Spec.Handle, AbilityActorInfo.Get(), Spec.ActivationInfo);
			}
		}
	}
}

void UAbilitySystemComponent::InputConfirm()
{
	FScopedPredictionWindow ScopedPrediction(this, true);

	if (GetOwnerRole() != ROLE_Authority && ConfirmCallbacks.IsBound())
	{
		// Tell the server we confirmed input.
		ServerSetReplicatedConfirm(true, ScopedPredictionKey);
	}
	
	ConfirmCallbacks.Broadcast();
}

void UAbilitySystemComponent::InputCancel()
{
	FScopedPredictionWindow ScopedPrediction(this, true);

	if (GetOwnerRole() != ROLE_Authority && CancelCallbacks.IsBound())
	{
		// Tell the server we confirmed input.
		ServerSetReplicatedConfirm(false, ScopedPredictionKey);
	}

	CancelCallbacks.Broadcast();
}

bool UAbilitySystemComponent::ServerInputPress_Validate(FGameplayAbilitySpecHandle Handle, FPredictionKey ScopedPedictionKey)
{
	return true;
}

void UAbilitySystemComponent::ServerInputPress_Implementation(FGameplayAbilitySpecHandle Handle, FPredictionKey ScopedPedictionKey)
{
	FScopedPredictionWindow ScopedPrediction(this, ScopedPedictionKey);

	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (Spec)
	{
		AbilitySpecInputPressed(*Spec);
	}
}

bool UAbilitySystemComponent::ServerInputRelease_Validate(FGameplayAbilitySpecHandle Handle, FPredictionKey ScopedPedictionKey)
{
	return true;
}

void UAbilitySystemComponent::ServerInputRelease_Implementation(FGameplayAbilitySpecHandle Handle, FPredictionKey ScopedPedictionKey)
{
	FScopedPredictionWindow ScopedPrediction(this, ScopedPedictionKey);

	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (Spec)
	{
		AbilitySpecInputReleased(*Spec);
	}
}

void UAbilitySystemComponent::TargetConfirm()
{
	TArray<AGameplayAbilityTargetActor*> LeftoverTargetActors;
	for (AGameplayAbilityTargetActor* TargetActor : SpawnedTargetActors)
	{
		if (TargetActor)
		{
			if (TargetActor->IsConfirmTargetingAllowed())
			{
				//TODO: There might not be any cases where this bool is false
				if (!TargetActor->bDestroyOnConfirmation)
				{
					LeftoverTargetActors.Add(TargetActor);
				}
				TargetActor->ConfirmTargeting();
			}
			else
			{
				TargetActor->NotifyPlayerControllerOfRejectedConfirmation();
				LeftoverTargetActors.Add(TargetActor);
			}
		}
	}
	SpawnedTargetActors = LeftoverTargetActors;		//These actors declined to confirm targeting, or are allowed to fire multiple times, so keep contact with them.
}

void UAbilitySystemComponent::TargetCancel()
{
	for (AGameplayAbilityTargetActor* TargetActor : SpawnedTargetActors)
	{
		if (TargetActor)
		{
			TargetActor->CancelTargeting();
		}
	}

	SpawnedTargetActors.Empty();
}

// --------------------------------------------------------------------------

void UAbilitySystemComponent::ServerSetReplicatedAbilityKeyState_Implementation(int32 InputID, bool Pressed, FPredictionKey PredictionKey)
{
	FScopedPredictionWindow ScopedPrediction(this, PredictionKey);

	for (FGameplayAbilitySpec& Spec : ActivatableAbilities)
	{
		if (Spec.InputID == InputID)
		{
			if (Spec.Ability)
			{
				Spec.InputPressed = Pressed;

				// Call InputPressed() / InputReleased() on the affected abilities
				if (Pressed)
				{
					AbilitySpecInputPressed(Spec);
				}
				else
				{
					AbilitySpecInputReleased(Spec);
				}
			}
		}
	}

	if (Pressed)
	{
		AbilityKeyPressCallbacks.Broadcast(InputID);
	}
	else
	{
		AbilityKeyReleaseCallbacks.Broadcast(InputID);
	}
} 

bool UAbilitySystemComponent::ServerSetReplicatedAbilityKeyState_Validate(int32 InputID, bool Pressed, FPredictionKey PredictionKey)
{
	return true;
}

// --------------------------------------------------------------------------

void UAbilitySystemComponent::ServerSetReplicatedConfirm_Implementation(bool Confirmed, FPredictionKey PredictionKey)
{
	FScopedPredictionWindow ScopedPrediction(this, PredictionKey);

	if (Confirmed)
	{
		ReplicatedConfirmAbility = true;
		ConfirmCallbacks.Broadcast();
	}
	else
	{
		ReplicatedCancelAbility = true;
		CancelCallbacks.Broadcast();
	}
}

bool UAbilitySystemComponent::ServerSetReplicatedConfirm_Validate(bool Confirmed, FPredictionKey PredictionKey)
{
	return true;
}

// -------

void UAbilitySystemComponent::ServerSetReplicatedTargetData_Implementation(FGameplayAbilityTargetDataHandle Confirmed, FPredictionKey PredictionKey)
{
	FScopedPredictionWindow ScopedPrediction(this, PredictionKey);

	ReplicatedTargetData = Confirmed;
	ReplicatedTargetDataDelegate.Broadcast(ReplicatedTargetData);
}

bool UAbilitySystemComponent::ServerSetReplicatedTargetData_Validate(FGameplayAbilityTargetDataHandle Confirmed, FPredictionKey PredictionKey)
{
	return true;
}

// -------

void UAbilitySystemComponent::ServerSetReplicatedTargetDataCancelled_Implementation()
{
	ReplicatedTargetDataCancelledDelegate.Broadcast();
}

bool UAbilitySystemComponent::ServerSetReplicatedTargetDataCancelled_Validate()
{
	return true;
}

// -------

void UAbilitySystemComponent::ConsumeAbilityConfirmCancel()
{
	ReplicatedConfirmAbility = false;
	ReplicatedCancelAbility = false;
	ConfirmCallbacks.Clear();
	CancelCallbacks.Clear();
}

void UAbilitySystemComponent::ConsumeAbilityTargetData()
{
	ReplicatedTargetData.Clear();
}

void UAbilitySystemComponent::OnRep_SimulatedTasks()
{
	for (UAbilityTask* SimulatedTask : SimulatedTasks)
	{
		// Temp check 
		if (SimulatedTask && SimulatedTask->bTickingTask && TickingTasks.Contains(SimulatedTask) == false)
		{
			SimulatedTask->InitSimulatedTask(this);
			if (TickingTasks.Num() == 0)
			{
				UpdateShouldTick();
			}

			TickingTasks.Add(SimulatedTask);
		}
	}
}

// ---------------------------------------------------------------------------

float UAbilitySystemComponent::PlayMontage(UGameplayAbility* InAnimatingAbility, FGameplayAbilityActivationInfo ActivationInfo, UAnimMontage* NewAnimMontage, float InPlayRate, FName StartSectionName)
{
	UAnimInstance* AnimInstance = AbilityActorInfo->AnimInstance.Get();

	if (AnimInstance && NewAnimMontage)
	{
		float const Duration = AnimInstance->Montage_Play(NewAnimMontage, InPlayRate);
		if (Duration > 0.f)
		{
			if (LocalAnimMontageInfo.AnimatingAbility && LocalAnimMontageInfo.AnimatingAbility != InAnimatingAbility)
			{
				// The ability that was previously animating will have already gotten the 'interrupted' callback.
				// It may be a good idea to make this a global policy and 'cancel' the ability.
				// 
				// For now, we expect it to end itself when this happens.
			}

			LocalAnimMontageInfo.AnimMontage = NewAnimMontage;
			LocalAnimMontageInfo.AnimatingAbility = InAnimatingAbility;
			
			if (InAnimatingAbility)
			{
				InAnimatingAbility->SetCurrentMontage(NewAnimMontage);
			}
			
			// Replicate to non owners
			if (IsOwnerActorAuthoritative())
			{
				// Those are static parameters, they are only set when the montage is played. They are not changed after that.
				RepAnimMontageInfo.AnimMontage = NewAnimMontage;
				RepAnimMontageInfo.ForcePlayBit = !bool(RepAnimMontageInfo.ForcePlayBit);

				// Start at a given Section.
				if (StartSectionName != NAME_None)
				{
					AnimInstance->Montage_JumpToSection(StartSectionName);
				}

				// Update parameters that change during Montage life time.
				AnimMontage_UpdateReplicatedData();
			}
			else
			{
				// If this prediction key is rejected, we need to end the preview
				FPredictionKey PredictionKey = GetPredictionKeyForNewAction();
				if (PredictionKey.IsValidKey())
				{
					PredictionKey.NewRejectedDelegate().BindUObject(this, &UAbilitySystemComponent::OnPredictiveMontageRejected, NewAnimMontage);
				}
			}

			return Duration;
		}
	}

	return -1.f;
}

float UAbilitySystemComponent::PlayMontageSimulated(UAnimMontage* NewAnimMontage, float InPlayRate, FName StartSectionName)
{
	UAnimInstance* AnimInstance = AbilityActorInfo->AnimInstance.Get();

	if (AnimInstance && NewAnimMontage)
	{
		float const Duration = AnimInstance->Montage_Play(NewAnimMontage, InPlayRate);
		if (Duration > 0.f)
		{
			LocalAnimMontageInfo.AnimMontage = NewAnimMontage;
			return Duration;
		}
	}

	return -1.f;
}

void UAbilitySystemComponent::AnimMontage_UpdateReplicatedData()
{
	check(IsOwnerActorAuthoritative());

	UAnimInstance* AnimInstance = AbilityActorInfo->AnimInstance.Get();
	if (AnimInstance && LocalAnimMontageInfo.AnimMontage)
	{
		RepAnimMontageInfo.AnimMontage = LocalAnimMontageInfo.AnimMontage;
		RepAnimMontageInfo.PlayRate = AnimInstance->Montage_GetPlayRate(LocalAnimMontageInfo.AnimMontage);
		RepAnimMontageInfo.Position = AnimInstance->Montage_GetPosition(LocalAnimMontageInfo.AnimMontage);

		// Compressed Flags
		bool bIsStopped = AnimInstance->Montage_GetIsStopped(LocalAnimMontageInfo.AnimMontage);

		if (RepAnimMontageInfo.IsStopped != bIsStopped)
		{
			// When this changes, we should update whether or not we should be ticking
			UpdateShouldTick();
		}
			
		RepAnimMontageInfo.IsStopped = bIsStopped;

		// Replicate NextSectionID to keep it in sync.
		// We actually replicate NextSectionID+1 on a BYTE to put INDEX_NONE in there.
		int32 CurrentSectionID = LocalAnimMontageInfo.AnimMontage->GetSectionIndexFromPosition(RepAnimMontageInfo.Position);
		if (CurrentSectionID != INDEX_NONE)
		{
			int32 NextSectionID = AnimInstance->Montage_GetNextSectionID(LocalAnimMontageInfo.AnimMontage, CurrentSectionID);
			ensure(NextSectionID < (256 - 1));
			RepAnimMontageInfo.NextSectionID = uint8(NextSectionID + 1);
		}
		else
		{
			RepAnimMontageInfo.NextSectionID = 0;
		}
	}
}

void UAbilitySystemComponent::OnPredictiveMontageRejected(UAnimMontage* PredictiveMontage)
{
	static const float MONTAGE_PREDICTION_REJECT_FADETIME = 0.25f;

	UAnimInstance* AnimInstance = AbilityActorInfo->AnimInstance.Get();
	if (AnimInstance && PredictiveMontage)
	{
		// If this montage is still palying: kill it
		if (AnimInstance->Montage_IsPlaying(PredictiveMontage))
		{
			AnimInstance->Montage_Stop(MONTAGE_PREDICTION_REJECT_FADETIME, PredictiveMontage);
		}
	}
}

/**	Replicated Event for AnimMontages */
void UAbilitySystemComponent::OnRep_ReplicatedAnimMontage()
{
	static const float MONTAGE_REP_POS_ERR_THRESH = 0.1f;

	UAnimInstance* AnimInstance = AbilityActorInfo->AnimInstance.Get();
	if (!AbilityActorInfo->IsLocallyControlled() && AnimInstance)
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("net.Montage.Debug"));
		bool DebugMontage = (CVar && CVar->GetValueOnGameThread() == 1);
		if (DebugMontage)
		{
			ABILITY_LOG( Warning, TEXT("\n\nOnRep_ReplicatedAnimMontage, %s"), *GetNameSafe(this));
			ABILITY_LOG( Warning, TEXT("\tAnimMontage: %s\n\tPlayRate: %f\n\tPosition: %f\n\tNextSectionID: %d\n\tIsStopped: %d\n\tForcePlayBit: %d"),
				*GetNameSafe(RepAnimMontageInfo.AnimMontage), 
				RepAnimMontageInfo.PlayRate, 
				RepAnimMontageInfo.Position, 
				RepAnimMontageInfo.NextSectionID, 
				RepAnimMontageInfo.IsStopped, 
				RepAnimMontageInfo.ForcePlayBit);
			ABILITY_LOG( Warning, TEXT("\tLocalAnimMontageInfo.AnimMontage: %s\n\tPosition: %f"),
				*GetNameSafe(LocalAnimMontageInfo.AnimMontage), AnimInstance->Montage_GetPosition(LocalAnimMontageInfo.AnimMontage));
		}

		if( RepAnimMontageInfo.AnimMontage )
		{
			// New Montage to play
			bool ReplicatedPlayBit = bool(RepAnimMontageInfo.ForcePlayBit);
			if ((LocalAnimMontageInfo.AnimMontage != RepAnimMontageInfo.AnimMontage) || (LocalAnimMontageInfo.PlayBit != ReplicatedPlayBit))
			{
				LocalAnimMontageInfo.PlayBit = ReplicatedPlayBit;
				PlayMontageSimulated(RepAnimMontageInfo.AnimMontage, RepAnimMontageInfo.PlayRate);
			}

			// Play Rate has changed
			if (AnimInstance->Montage_GetPlayRate(LocalAnimMontageInfo.AnimMontage) != RepAnimMontageInfo.PlayRate)
			{
				AnimInstance->Montage_SetPlayRate(LocalAnimMontageInfo.AnimMontage, RepAnimMontageInfo.PlayRate);
			}

			int32 RepSectionID = LocalAnimMontageInfo.AnimMontage->GetSectionIndexFromPosition(RepAnimMontageInfo.Position);
			int32 RepNextSectionID = int32(RepAnimMontageInfo.NextSectionID) - 1;
		
			// And NextSectionID for the replicated SectionID.
			if( RepSectionID != INDEX_NONE )
			{
				int32 NextSectionID = AnimInstance->Montage_GetNextSectionID(LocalAnimMontageInfo.AnimMontage, RepSectionID);

				// If NextSectionID is different thant the replicated one, then set it.
				if( NextSectionID != RepNextSectionID )
				{
					AnimInstance->Montage_SetNextSection(LocalAnimMontageInfo.AnimMontage->GetSectionName(RepSectionID), LocalAnimMontageInfo.AnimMontage->GetSectionName(RepNextSectionID));
				}

				// Make sure we haven't received that update too late and the client hasn't already jumped to another section. 
				int32 CurrentSectionID = LocalAnimMontageInfo.AnimMontage->GetSectionIndexFromPosition(AnimInstance->Montage_GetPosition(LocalAnimMontageInfo.AnimMontage));
				if( (CurrentSectionID != RepSectionID) && (CurrentSectionID != RepNextSectionID) )
				{
					// Client is in a wrong section, jump to replicated position.
					AnimInstance->Montage_SetPosition(LocalAnimMontageInfo.AnimMontage, RepAnimMontageInfo.Position);
				}
			}

			// Update Position. If error is too great, jump to replicated position.
			float CurrentPosition = AnimInstance->Montage_GetPosition(LocalAnimMontageInfo.AnimMontage);
			int32 CurrentSectionID = LocalAnimMontageInfo.AnimMontage->GetSectionIndexFromPosition(CurrentPosition);
			// Only check threshold if we are located in the same section. Different sections require a bit more work as we could be jumping around the timeline.
			if( (CurrentSectionID == RepSectionID) && (FMath::Abs(CurrentPosition - RepAnimMontageInfo.Position) > MONTAGE_REP_POS_ERR_THRESH) )
			{
				AnimInstance->Montage_SetPosition(LocalAnimMontageInfo.AnimMontage, RepAnimMontageInfo.Position);
			}

			// Compressed Flags
			bool bIsStopped = AnimInstance->Montage_GetIsStopped(LocalAnimMontageInfo.AnimMontage);
			bool ReplicatedIsStopped = bool(RepAnimMontageInfo.IsStopped);
			if( ReplicatedIsStopped && !bIsStopped )
			{
				CurrentMontageStop();
			}
		}
	}
}

void UAbilitySystemComponent::CurrentMontageStop()
{
	UAnimInstance* AnimInstance = AbilityActorInfo->AnimInstance.Get();
	UAnimMontage* MontageToStop = LocalAnimMontageInfo.AnimMontage;
	bool bShouldStopMontage = AnimInstance && MontageToStop && !AnimInstance->Montage_GetIsStopped(MontageToStop);

	if (bShouldStopMontage)
	{
		AnimInstance->Montage_Stop(MontageToStop->BlendOutTime);

		if (IsOwnerActorAuthoritative())
		{
			AnimMontage_UpdateReplicatedData();
		}
	}
}

void UAbilitySystemComponent::ClearAnimatingAbility(UGameplayAbility* Ability)
{
	if (LocalAnimMontageInfo.AnimatingAbility == Ability)
	{
		Ability->SetCurrentMontage(NULL);
		LocalAnimMontageInfo.AnimatingAbility = NULL;
	}
}

void UAbilitySystemComponent::CurrentMontageJumpToSection(FName SectionName)
{
	UAnimInstance* AnimInstance = AbilityActorInfo->AnimInstance.Get();
	if( (SectionName != NAME_None) && AnimInstance )
	{
		AnimInstance->Montage_JumpToSection(SectionName);
		if (IsOwnerActorAuthoritative())
		{
			AnimMontage_UpdateReplicatedData();
		}
		else
		{
			ServerCurrentMontageJumpToSectionName(LocalAnimMontageInfo.AnimMontage, SectionName);
		}
	}
}

void UAbilitySystemComponent::CurrentMontageSetNextSectionName(FName FromSectionName, FName ToSectionName)
{
	UAnimInstance* AnimInstance = AbilityActorInfo->AnimInstance.Get();
	if( LocalAnimMontageInfo.AnimMontage && AnimInstance )
	{
		// Set Next Section Name. 
		AnimInstance->Montage_SetNextSection(FromSectionName, ToSectionName);

		// Update replicated version for Simulated Proxies if we are on the server.
		if( IsOwnerActorAuthoritative() )
		{
			AnimMontage_UpdateReplicatedData();
		}
		else
		{
			float CurrentPosition = AnimInstance->Montage_GetPosition(LocalAnimMontageInfo.AnimMontage);
			ServerCurrentMontageSetNextSectionName(LocalAnimMontageInfo.AnimMontage, CurrentPosition, FromSectionName, ToSectionName);
		}
	}
}

bool UAbilitySystemComponent::ServerCurrentMontageSetNextSectionName_Validate(UAnimMontage* ClientAnimMontage, float ClientPosition, FName SectionName, FName NextSectionName)
{
	return true;
}

void UAbilitySystemComponent::ServerCurrentMontageSetNextSectionName_Implementation(UAnimMontage* ClientAnimMontage, float ClientPosition, FName SectionName, FName NextSectionName)
{
	UAnimInstance* AnimInstance = AbilityActorInfo->AnimInstance.Get();
	UAnimMontage* CurrentAnimMontage = LocalAnimMontageInfo.AnimMontage;
	if (AnimInstance)
	{
		if (ClientAnimMontage == CurrentAnimMontage)
		{
			// Set NextSectionName
			AnimInstance->Montage_SetNextSection(SectionName, NextSectionName);

			// Correct position if we are in an invalid section
			float CurrentPosition = AnimInstance->Montage_GetPosition(CurrentAnimMontage);
			int32 CurrentSectionID = CurrentAnimMontage->GetSectionIndexFromPosition(CurrentPosition);
			FName CurrentSectionName = CurrentAnimMontage->GetSectionName(CurrentSectionID);

			int32 ClientSectionID = CurrentAnimMontage->GetSectionIndexFromPosition(ClientPosition);
			FName ClientCurrentSectionName = CurrentAnimMontage->GetSectionName(ClientSectionID);
			if ((CurrentSectionName != ClientCurrentSectionName) || (CurrentSectionName != SectionName) || (CurrentSectionName != NextSectionName))
			{
				// We are in an invalid section, jump to client's position.
				AnimInstance->Montage_SetPosition(CurrentAnimMontage, ClientPosition);
			}

			// Update replicated version for Simulated Proxies if we are on the server.
			if (IsOwnerActorAuthoritative())
			{
				AnimMontage_UpdateReplicatedData();
			}
		}
	}
}

bool UAbilitySystemComponent::ServerCurrentMontageJumpToSectionName_Validate(UAnimMontage* ClientAnimMontage, FName SectionName)
{
	return true;
}

void UAbilitySystemComponent::ServerCurrentMontageJumpToSectionName_Implementation(UAnimMontage* ClientAnimMontage, FName SectionName)
{
	UAnimInstance* AnimInstance = AbilityActorInfo->AnimInstance.Get();
	UAnimMontage* CurrentAnimMontage = LocalAnimMontageInfo.AnimMontage;
	if (AnimInstance)
	{
		if (ClientAnimMontage == CurrentAnimMontage)
		{
			// Set NextSectionName
			AnimInstance->Montage_JumpToSection(SectionName);

			// Update replicated version for Simulated Proxies if we are on the server.
			if (IsOwnerActorAuthoritative())
			{
				AnimMontage_UpdateReplicatedData();
			}
		}
	}
}

UAnimMontage* UAbilitySystemComponent::GetCurrentMontage() const
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->AnimInstance.Get() : nullptr;
	if (AnimInstance)
	{
		return AnimInstance->GetCurrentActiveMontage();
	}

	return nullptr;
}

int32 UAbilitySystemComponent::GetCurrentMontageSectionID() const
{
	UAnimInstance* AnimInstance = AbilityActorInfo->AnimInstance.Get();
	UAnimMontage* CurrentAnimMontage = GetCurrentMontage();

	if (CurrentAnimMontage && AnimInstance)
	{
		float MontagePosition = AnimInstance->Montage_GetPosition(CurrentAnimMontage);
		return CurrentAnimMontage->GetSectionIndexFromPosition(MontagePosition);
	}

	return INDEX_NONE;
}

FName UAbilitySystemComponent::GetCurrentMontageSectionName() const
{
	UAnimInstance* AnimInstance = AbilityActorInfo->AnimInstance.Get();
	UAnimMontage* CurrentAnimMontage = GetCurrentMontage();

	if (CurrentAnimMontage && AnimInstance)
	{
		float MontagePosition = AnimInstance->Montage_GetPosition(CurrentAnimMontage);
		int32 CurrentSectionID = CurrentAnimMontage->GetSectionIndexFromPosition(MontagePosition);

		return CurrentAnimMontage->GetSectionName(CurrentSectionID);
	}

	return NAME_None;
}

float UAbilitySystemComponent::GetCurrentMontageSectionLength() const
{
	UAnimInstance* AnimInstance = AbilityActorInfo->AnimInstance.Get();
	UAnimMontage* CurrentAnimMontage = GetCurrentMontage();

	if (CurrentAnimMontage && AnimInstance)
	{
		int32 CurrentSectionID = GetCurrentMontageSectionID();
		if (CurrentSectionID != INDEX_NONE)
		{
			TArray<FCompositeSection>& CompositeSections = CurrentAnimMontage->CompositeSections;

			// If we have another section after us, then take delta between both start times.
			if (CurrentSectionID < (CompositeSections.Num() - 1))
			{
				return (CompositeSections[CurrentSectionID + 1].GetTime() - CompositeSections[CurrentSectionID].GetTime());
			}
			// Otherwise we are the last section, so take delta with Montage total time.
			else
			{
				return (CurrentAnimMontage->SequenceLength - CompositeSections[CurrentSectionID].GetTime());
			}
		}

		// if we have no sections, just return total length of Montage.
		return CurrentAnimMontage->SequenceLength;
	}

	return 0.f;
}

float UAbilitySystemComponent::GetCurrentMontageSectionTimeLeft() const
{
	UAnimInstance* AnimInstance = AbilityActorInfo->AnimInstance.Get();
	UAnimMontage* CurrentAnimMontage = GetCurrentMontage();
	if (CurrentAnimMontage && AnimInstance && AnimInstance->Montage_IsActive(CurrentAnimMontage))
	{
		const float CurrentPosition = AnimInstance->Montage_GetPosition(CurrentAnimMontage);
		return CurrentAnimMontage->GetSectionTimeLeftFromPos(CurrentPosition);
	}

	return -1.f;
}

bool UAbilitySystemComponent::IsAnimatingAbility(UGameplayAbility* InAbility) const
{
	return (LocalAnimMontageInfo.AnimatingAbility == InAbility);
}

UGameplayAbility* UAbilitySystemComponent::GetAnimatingAbility()
{
	return LocalAnimMontageInfo.AnimatingAbility;
}
#undef LOCTEXT_NAMESPACE

