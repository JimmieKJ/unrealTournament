// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "GameplayTagsModule.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilityTask.h"
#include "VisualLogger.h"
#include "GameplayCueManager.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	UGameplayAbility
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

UGameplayAbility::UGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	{
		static FName FuncName = FName(TEXT("K2_ShouldAbilityRespondToEvent"));
		UFunction* ShouldRespondFunction = GetClass()->FindFunctionByName(FuncName);
		bHasBlueprintShouldAbilityRespondToEvent = ShouldRespondFunction && ShouldRespondFunction->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
	}
	{
		static FName FuncName = FName(TEXT("K2_CanActivateAbility"));
		UFunction* CanActivateFunction = GetClass()->FindFunctionByName(FuncName);
		bHasBlueprintCanUse = CanActivateFunction && CanActivateFunction->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
	}
	{
		static FName FuncName = FName(TEXT("K2_ActivateAbility"));
		UFunction* ActivateFunction = GetClass()->FindFunctionByName(FuncName);
		bHasBlueprintActivate = ActivateFunction && ActivateFunction->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
	}
	{
		static FName FuncName = FName(TEXT("K2_ActivateAbilityFromEvent"));
		UFunction* ActivateFunction = GetClass()->FindFunctionByName(FuncName);
		bHasBlueprintActivateFromEvent = ActivateFunction && ActivateFunction->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
	}
	
#if WITH_EDITOR
	/** Autoregister abilities with the blueprint debugger in the editor.*/
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UBlueprint* BP = Cast<UBlueprint>(GetClass()->ClassGeneratedBy);
		if (BP && (BP->GetWorldBeingDebugged() == nullptr || BP->GetWorldBeingDebugged() == GetWorld()))
		{
			BP->SetObjectBeingDebugged(this);
		}
	}
#endif

	bServerRespectsRemoteAbilityCancellation = true;
	bReplicateInputDirectly = false;
}

int32 UGameplayAbility::GetFunctionCallspace(UFunction* Function, void* Parameters, FFrame* Stack)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return FunctionCallspace::Local;
	}
	check(GetOuter() != NULL);
	return GetOuter()->GetFunctionCallspace(Function, Parameters, Stack);
}

bool UGameplayAbility::CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack)
{
	check(!HasAnyFlags(RF_ClassDefaultObject));
	check(GetOuter() != NULL);

	AActor* Owner = CastChecked<AActor>(GetOuter());
	UNetDriver* NetDriver = Owner->GetNetDriver();
	if (NetDriver)
	{
		NetDriver->ProcessRemoteFunction(Owner, Function, Parameters, OutParms, Stack, this);
		return true;
	}

	return false;
}

// TODO: polymorphic payload
void UGameplayAbility::SendGameplayEvent(FGameplayTag EventTag, FGameplayEventData Payload)
{
	UAbilitySystemComponent* AbilitySystemComponent = CurrentActorInfo->AbilitySystemComponent.Get();
	if (ensure(AbilitySystemComponent))
	{
		FScopedPredictionWindow NewScopedWindow(AbilitySystemComponent, true);
		AbilitySystemComponent->HandleGameplayEvent(EventTag, &Payload);
	}
}

void UGameplayAbility::PostNetInit()
{
	/** We were dynamically spawned from replication - we need to init a currentactorinfo by looking at outer.
	 *  This may need to be updated further if we start having abilities live on different outers than player AbilitySystemComponents.
	 */
	
	if (CurrentActorInfo == NULL)
	{
		AActor* OwnerActor = Cast<AActor>(GetOuter());
		if (ensure(OwnerActor))
		{
			UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor);
			if (ensure(AbilitySystemComponent))
			{
				CurrentActorInfo = AbilitySystemComponent->AbilityActorInfo.Get();
			}
		}
	}
}

bool UGameplayAbility::IsActive() const
{
	// Only Instanced-Per-Actor abilities persist between activations
	if (GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor)
	{
		return bIsActive;
	}

	// this should not be called on NonInstanced warn about it, Should call IsActive on the ability spec instead
	if (GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::NonInstanced)
	{
		ABILITY_LOG(Warning, TEXT("UGameplayAbility::IsActive() called on %s NonInstanced ability, call IsActive on the Ability Spec instead"), *GetName());
	}

	// NonInstanced and Instanced-Per-Execution abilities are by definition active unless they are pending kill
	return !IsPendingKill();
}

bool UGameplayAbility::IsSupportedForNetworking() const
{
	/**
	 *	We can only replicate references to:
	 *		-CDOs and DataAssets (e.g., static, non-instanced gameplay abilities)
	 *		-Instanced abilities that are replicating (and will thus be created on clients).
	 *		
	 *	Otherwise it is not supported, and it will be recreated on the client
	 */

	bool Supported = GetReplicationPolicy() != EGameplayAbilityReplicationPolicy::ReplicateNo || GetOuter()->IsA(UPackage::StaticClass());

	return Supported;
}

bool UGameplayAbility::DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	bool bBlocked = false, bMissing = false;

	const FGameplayTag& BlockedTag = UAbilitySystemGlobals::Get().ActivateFailTagsBlockedTag;
	const FGameplayTag& MissingTag = UAbilitySystemGlobals::Get().ActivateFailTagsMissingTag;

	// Check if any of this ability's tags are currently blocked
	if (AbilitySystemComponent.AreAbilityTagsBlocked(AbilityTags))
	{
		bBlocked = true;
	}

	// Check to see the required/blocked tags for this ability
	if (ActivationBlockedTags.Num() || ActivationRequiredTags.Num())
	{
		FGameplayTagContainer AbilitySystemComponentTags;

		AbilitySystemComponent.GetOwnedGameplayTags(AbilitySystemComponentTags);

		if (AbilitySystemComponentTags.MatchesAny(ActivationBlockedTags, false))
		{
			bBlocked = true;
		}

		if (!AbilitySystemComponentTags.MatchesAll(ActivationRequiredTags, true))
		{
			bMissing = true;
		}
	}

	if (SourceTags != nullptr)
	{
		if (SourceBlockedTags.Num() || SourceRequiredTags.Num())
		{
			if (SourceTags->MatchesAny(SourceBlockedTags, false))
			{
				bBlocked = true;
			}

			if (!SourceTags->MatchesAll(SourceRequiredTags, true))
			{
				bMissing = true;
			}
		}
	}

	if (TargetTags != nullptr)
	{
		if (TargetBlockedTags.Num() || TargetRequiredTags.Num())
		{
			if (TargetTags->MatchesAny(TargetBlockedTags, false))
			{
				bBlocked = true;
			}

			if (!TargetTags->MatchesAll(TargetRequiredTags, true))
			{
				bMissing = true;
			}
		}
	}

	if (bBlocked)
	{
		if (OptionalRelevantTags && BlockedTag.IsValid())
		{
			OptionalRelevantTags->AddTag(BlockedTag);
		}
		return false;
	}
	if (bMissing)
	{
		if (OptionalRelevantTags && MissingTag.IsValid())
		{
			OptionalRelevantTags->AddTag(MissingTag);
		}
		return false;
	}
	
	return true;
}

bool UGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	// Don't set the actor info, CanActivate is called on the CDO

	// A valid AvatarActor is required. Simulated proxy check means only authority or autonomous proxies should be executing abilities.
	if (ActorInfo == nullptr || ActorInfo->AvatarActor == nullptr || ActorInfo->AvatarActor->Role == ROLE_SimulatedProxy)
	{
		return false;
	}

	//make into a reference for simplicity
	FGameplayTagContainer DummyContainer;
	FGameplayTagContainer& OutTags = OptionalRelevantTags ? *OptionalRelevantTags : DummyContainer;

	// make sure the ActorInfo and its ability system component are valid, if not bail out.
	if (ActorInfo == nullptr || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return false;
	}

	if (ActorInfo->AbilitySystemComponent->GetUserAbilityActivationInhibited())
	{
		/**
		 *	Input is inhibited (UI is pulled up, another ability may be blocking all other input, etc).
		 *	When we get into triggered abilities, we may need to better differentiate between CanActviate and CanUserActivate or something.
		 *	E.g., we would want LMB/RMB to be inhibited while the user is in the menu UI, but we wouldn't want to prevent a 'buff when I am low health'
		 *	ability to not trigger.
		 *	
		 *	Basically: CanActivateAbility is only used by user activated abilities now. If triggered abilities need to check costs/cooldowns, then we may
		 *	want to split this function up and change the calling API to distinguish between 'can I initiate an ability activation' and 'can this ability be activated'.
		 */ 
		return false;
	}
	
	if (!UAbilitySystemGlobals::Get().ShouldIgnoreCooldowns() && !CheckCooldown(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	if (!UAbilitySystemGlobals::Get().ShouldIgnoreCosts() && !CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	if (!DoesAbilitySatisfyTagRequirements(*ActorInfo->AbilitySystemComponent.Get(), SourceTags, TargetTags, OptionalRelevantTags))
	{	// If the ability's tags are blocked, or if it has a "Blocking" tag or is missing a "Required" tag, then it can't activate.
		return false;
	}

	FGameplayAbilitySpec* Spec = ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		ABILITY_LOG(Warning, TEXT("CanActivateAbility called with invalid Handle"));
		return false;
	}

	// Check if this ability's input binding is currently blocked
	if (ActorInfo->AbilitySystemComponent->IsAbilityInputBlocked(Spec->InputID))
	{
		return false;
	}

	if (bHasBlueprintCanUse)
	{
		if (K2_CanActivateAbility(*ActorInfo, OutTags) == false)
		{
			ABILITY_LOG(Log, TEXT("CanActivateAbility %s failed, blueprint refused"), *GetName());
			return false;
		}
	}

	return true;
}

bool UGameplayAbility::ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* Payload) const
{
	if (bHasBlueprintShouldAbilityRespondToEvent)
	{
		if (K2_ShouldAbilityRespondToEvent(*ActorInfo, *Payload) == false)
		{
			ABILITY_LOG(Log, TEXT("ShouldAbilityRespondToEvent %s failed, blueprint refused"), *GetName());
			return false;
		}
	}

	return true;
}

bool UGameplayAbility::CommitAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Last chance to fail (maybe we no longer have resources to commit since we after we started this ability activation)
	if (!CommitCheck(Handle, ActorInfo, ActivationInfo))
	{
		return false;
	}

	CommitExecute(Handle, ActorInfo, ActivationInfo);

	// Fixme: Should we always call this or only if it is implemented? A noop may not hurt but could be bad for perf (storing a HasBlueprintCommit per instance isn't good either)
	K2_CommitExecute();

	// Broadcast this commitment
	ActorInfo->AbilitySystemComponent->NotifyAbilityCommit(this);

	return true;
}

bool UGameplayAbility::CommitAbilityCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (UAbilitySystemGlobals::Get().ShouldIgnoreCooldowns())
	{
		return true;
	}

	// Last chance to fail (maybe we no longer have resources to commit since we after we started this ability activation)
	if (!CheckCooldown(Handle, ActorInfo))
	{
		return false;
	}

	ApplyCooldown(Handle, ActorInfo, ActivationInfo);
	return true;
}

bool UGameplayAbility::CommitAbilityCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (UAbilitySystemGlobals::Get().ShouldIgnoreCosts())
	{
		return true;
	}

	// Last chance to fail (maybe we no longer have resources to commit since we after we started this ability activation)
	if (!CheckCost(Handle, ActorInfo))
	{
		return false;
	}

	ApplyCost(Handle, ActorInfo, ActivationInfo);
	return true;
}

bool UGameplayAbility::CommitCheck(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	/**
	 *	Checks if we can (still) commit this ability. There are some subtleties here.
	 *		-An ability can start activating, play an animation, wait for a user confirmation/target data, and then actually commit
	 *		-Commit = spend resources/cooldowns. Its possible the source has changed state since he started activation, so a commit may fail.
	 *		-We don't want to just call CanActivateAbility() since right now that also checks things like input inhibition.
	 *			-E.g., its possible the act of starting your ability makes it no longer activatable (CanaCtivateAbility() may be false if called here).
	 */

	const bool bValidHandle = Handle.IsValid();
	const bool bValidActorInfoPieces = (ActorInfo && (ActorInfo->AbilitySystemComponent != nullptr));
	const bool bValidSpecFound = bValidActorInfoPieces && (ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle) != nullptr);

	// Ensure that the ability spec is even valid before trying to process the commit
	if (!bValidHandle || !bValidActorInfoPieces || !bValidSpecFound)
	{
		ensureMsgf(false, TEXT("UGameplayAbility::CommitCheck provided an invalid handle or actor info or couldn't find ability spec: %s Handle Valid: %d ActorInfo Valid: %d Spec Not Found: %d"), *GetName(), bValidHandle, bValidActorInfoPieces, bValidSpecFound);
		return false;
	}

	if (!UAbilitySystemGlobals::Get().ShouldIgnoreCooldowns() && !CheckCooldown(Handle, ActorInfo))
	{
		return false;
	}

	if (!UAbilitySystemGlobals::Get().ShouldIgnoreCosts() && !CheckCost(Handle, ActorInfo))
	{
		return false;
	}

	return true;
}

void UGameplayAbility::CommitExecute(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	ApplyCooldown(Handle, ActorInfo, ActivationInfo);

	ApplyCost(Handle, ActorInfo, ActivationInfo);
}

bool UGameplayAbility::CanBeCanceled() const
{
	if (GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
	{
		return bIsCancelable;
	}

	// Non instanced are always cancelable
	return true;
}

void UGameplayAbility::SetCanBeCanceled(bool bCanBeCanceled)
{
	if (GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced && bCanBeCanceled != bIsCancelable)
	{
		bIsCancelable = bCanBeCanceled;

		UAbilitySystemComponent* Comp = CurrentActorInfo->AbilitySystemComponent.Get();
		if (Comp)
		{
			Comp->HandleChangeAbilityCanBeCanceled(AbilityTags, this, bCanBeCanceled);
		}
	}
}

bool UGameplayAbility::IsBlockingOtherAbilities() const
{
	if (GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
	{
		return bIsBlockingOtherAbilities;
	}

	// Non instanced are always marked as blocking other abilities
	return true;
}

void UGameplayAbility::SetShouldBlockOtherAbilities(bool bShouldBlockAbilities)
{
	if (GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced && bShouldBlockAbilities != bIsBlockingOtherAbilities)
	{
		bIsBlockingOtherAbilities = bShouldBlockAbilities;

		UAbilitySystemComponent* Comp = CurrentActorInfo->AbilitySystemComponent.Get();
		if (Comp)
		{
			Comp->ApplyAbilityBlockAndCancelTags(AbilityTags, this, bIsBlockingOtherAbilities, BlockAbilitiesWithTag, false, CancelAbilitiesWithTag);
		}
	}
}

void UGameplayAbility::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	if (CanBeCanceled())
	{
		// Replicate the the server/client if needed
		if (bReplicateCancelAbility)
		{
			ActorInfo->AbilitySystemComponent->ReplicateEndOrCancelAbility(Handle, ActivationInfo, this, true);
		}

		// Gives the Ability BP a chance to perform custom logic/cleanup when any active ability states are active
		if (OnGameplayAbilityCancelled.IsBound())
		{
			OnGameplayAbilityCancelled.Broadcast();
		}

		// End the ability but don't replicate it, we replicate the CancelAbility call directly
		EndAbility(Handle, ActorInfo, ActivationInfo, false);
	}
}

bool UGameplayAbility::IsEndAbilityValid(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	check(ActorInfo);

	// Protect against EndAbility being called multiple times
	// Ending an AbilityState may cause this to be invoked again
	if (bIsActive == false && GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
	{
		return false;
	}

	// check to see if this is an NonInstanced or if the ability is active.
	const FGameplayAbilitySpec* Spec = ActorInfo ? ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle) : nullptr;
	const bool bIsSpecActive = (Spec != nullptr) ? Spec->IsActive() : IsActive();

	if (!bIsSpecActive)
	{
		return false;
	}

	return true;
}

void UGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility)
{
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		// Give blueprint a chance to react
		K2_OnEndAbility();

		// Stop any timers or latent actions for the ability
		UWorld* MyWorld = ActorInfo ? ActorInfo->AbilitySystemComponent->GetOwner()->GetWorld() : nullptr;
		if (MyWorld)
		{
			MyWorld->GetLatentActionManager().RemoveActionsForObject(this);
			MyWorld->GetTimerManager().ClearAllTimersForObject(this);
		}

		// Execute our delegate and unbind it, as we are no longer active and listeners can re-register when we become active again.
		OnGameplayAbilityEnded.ExecuteIfBound(this);
		OnGameplayAbilityEnded.Unbind();

		if (GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
		{
			bIsActive = false;
		}

		// Tell all our tasks that we are finished and they should cleanup
		for (int32 TaskIdx = ActiveTasks.Num() - 1; TaskIdx >= 0 && ActiveTasks.Num() > 0; --TaskIdx)
		{
			TWeakObjectPtr<UAbilityTask> Task = ActiveTasks[TaskIdx];
			if (Task.IsValid())
			{
				Task.Get()->AbilityEnded();
			}
		}
		ActiveTasks.Reset();	// Empty the array but dont resize memory, since this object is probably going to be destroyed very soon anyways.

		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			if (bReplicateEndAbility)
			{
				ActorInfo->AbilitySystemComponent->ReplicateEndOrCancelAbility(Handle, ActivationInfo, this, false);
			}

			// Remove tags we added to owner
			ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTags(ActivationOwnedTags);

			// Remove tracked GameplayCues that we added
			for (FGameplayTag& GameplayCueTag : TrackedGameplayCues)
			{
				ActorInfo->AbilitySystemComponent->RemoveGameplayCue(GameplayCueTag);
			}
			TrackedGameplayCues.Empty();

			if (CanBeCanceled())
			{
				// If we're still cancelable, cancel it now
				ActorInfo->AbilitySystemComponent->HandleChangeAbilityCanBeCanceled(AbilityTags, this, false);
			}
			
			if (IsBlockingOtherAbilities())
			{
				// If we're still blocking other abilities, cancel now
				ActorInfo->AbilitySystemComponent->ApplyAbilityBlockAndCancelTags(AbilityTags, this, false, BlockAbilitiesWithTag, false, CancelAbilitiesWithTag);
			}

			// Tell owning AbilitySystemComponent that we ended so it can do stuff (including MarkPendingKill us)
			ActorInfo->AbilitySystemComponent->NotifyAbilityEnded(Handle, this);
		}
	}
}

void UGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	SetCurrentInfo(Handle, ActorInfo, ActivationInfo);

	if (bHasBlueprintActivate)
	{
		// A Blueprinted ActivateAbility function must call CommitAbility somewhere in its execution chain.
		K2_ActivateAbility();
	}
	else if (bHasBlueprintActivateFromEvent)
	{
		if (TriggerEventData)
		{
			// A Blueprinted ActivateAbility function must call CommitAbility somewhere in its execution chain.
			K2_ActivateAbilityFromEvent(*TriggerEventData);
		}
		else
		{
			UE_LOG(LogAbilitySystem, Warning, TEXT("Ability %s expects event data but none is being supplied. Use Activate Ability instead of Activate Ability From Event."), *GetName());
			EndAbility(Handle, ActorInfo, ActivationInfo, false);
		}
	}
	else
	{
		// Native child classes may want to override ActivateAbility and do something like this:

		// Do stuff...

		if (CommitAbility(Handle, ActorInfo, ActivationInfo))		// ..then commit the ability...
		{			
			//	Then do more stuff...
		}
	}
}

void UGameplayAbility::PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded* OnGameplayAbilityEndedDelegate)
{
	UAbilitySystemComponent* Comp = ActorInfo->AbilitySystemComponent.Get();

	if (GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
	{
		bIsActive = true;
		bIsBlockingOtherAbilities = true;
		bIsCancelable = true;
	}

	Comp->HandleChangeAbilityCanBeCanceled(AbilityTags, this, true);
	Comp->ApplyAbilityBlockAndCancelTags(AbilityTags, this, true, BlockAbilitiesWithTag, true, CancelAbilitiesWithTag);
	Comp->AddLooseGameplayTags(ActivationOwnedTags);

	if (OnGameplayAbilityEndedDelegate)
	{
		OnGameplayAbilityEnded = *OnGameplayAbilityEndedDelegate;
	}

	Comp->NotifyAbilityActivated(Handle, this);
}

void UGameplayAbility::CallActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData)
{
	PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate);
	ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UGameplayAbility::ConfirmActivateSucceed()
{
	// On instanced abilities, update CurrentActivationInfo and call any registered delegates.
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		PostNetInit();
		check(CurrentActorInfo);
		CurrentActivationInfo.SetActivationConfirmed();

		OnConfirmDelegate.Broadcast(this);
		OnConfirmDelegate.Clear();
	}
}

UGameplayEffect* UGameplayAbility::GetCooldownGameplayEffect() const
{
	if ( CooldownGameplayEffectClass )
	{
		return CooldownGameplayEffectClass->GetDefaultObject<UGameplayEffect>();
	}
	else
	{
		return CooldownGameplayEffect;
	}
}

UGameplayEffect* UGameplayAbility::GetCostGameplayEffect() const
{
	if ( CostGameplayEffectClass )
	{
		return CostGameplayEffectClass->GetDefaultObject<UGameplayEffect>();
	}
	else
	{
		return CostGameplayEffect;
	}
}

bool UGameplayAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	const FGameplayTagContainer* CooldownTags = GetCooldownTags();
	if (CooldownTags)
	{
		check(ActorInfo->AbilitySystemComponent.IsValid());
		if (CooldownTags->Num() > 0 && ActorInfo->AbilitySystemComponent->HasAnyMatchingGameplayTags(*CooldownTags))
		{
			const FGameplayTag& CooldownTag = UAbilitySystemGlobals::Get().ActivateFailCooldownTag;

			if (OptionalRelevantTags && CooldownTag.IsValid())
			{
				OptionalRelevantTags->AddTag(CooldownTag);
			}

			return false;
		}
	}
	return true;
}

void UGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	if (CooldownGE)
	{
		ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, CooldownGE, GetAbilityLevel(Handle, ActorInfo));
	}
}

bool UGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	UGameplayEffect* CostGE = GetCostGameplayEffect();
	if (CostGE)
	{
		check(ActorInfo->AbilitySystemComponent.IsValid());
		if (!ActorInfo->AbilitySystemComponent->CanApplyAttributeModifiers(CostGE, GetAbilityLevel(Handle, ActorInfo), GetEffectContext(Handle, ActorInfo)))
		{
			const FGameplayTag& CostTag = UAbilitySystemGlobals::Get().ActivateFailCostTag;

			if (OptionalRelevantTags && CostTag.IsValid())
			{
				OptionalRelevantTags->AddTag(CostTag);
			}
			return false;
		}
	}
	return true;
}

void UGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* CostGE = GetCostGameplayEffect();
	if (CostGE)
	{
		ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, CostGE, GetAbilityLevel(Handle, ActorInfo));
	}
}

float UGameplayAbility::GetCooldownTimeRemaining(const FGameplayAbilityActorInfo* ActorInfo) const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayAbilityGetCooldownTimeRemaining);

	check(ActorInfo->AbilitySystemComponent.IsValid());
	const FGameplayTagContainer* CooldownTags = GetCooldownTags();
	if (CooldownTags && CooldownTags->Num() > 0)
	{
		TArray< float > Durations = ActorInfo->AbilitySystemComponent->GetActiveEffectsTimeRemaining(FActiveGameplayEffectQuery(CooldownTags));
		if (Durations.Num() > 0)
		{
			Durations.Sort();
			return Durations[Durations.Num()-1];
		}
	}

	return 0.f;
}

void UGameplayAbility::GetCooldownTimeRemainingAndDuration(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, float& TimeRemaining, float& CooldownDuration) const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayAbilityGetCooldownTimeRemainingAndDuration);

	check(ActorInfo->AbilitySystemComponent.IsValid());

	TimeRemaining = 0.f;
	CooldownDuration = 0.f;
	
	const FGameplayTagContainer* CooldownTags = GetCooldownTags();
	if (CooldownTags && CooldownTags->Num() > 0)
	{
		TArray< float > DurationRemaining = ActorInfo->AbilitySystemComponent->GetActiveEffectsTimeRemaining(FActiveGameplayEffectQuery(CooldownTags));
		if (DurationRemaining.Num() > 0)
		{
			TArray< float > Durations = ActorInfo->AbilitySystemComponent->GetActiveEffectsDuration(FActiveGameplayEffectQuery(CooldownTags));
			check(Durations.Num() == DurationRemaining.Num());
			int32 BestIdx = 0;
			float LongestTime = DurationRemaining[0];
			for (int32 Idx = 1; Idx < DurationRemaining.Num(); ++Idx)
			{
				if (DurationRemaining[Idx] > LongestTime)
				{
					LongestTime = DurationRemaining[Idx];
					BestIdx = Idx;
				}
			}

			TimeRemaining = DurationRemaining[BestIdx];
			CooldownDuration = Durations[BestIdx];
		}
	}
}

const FGameplayTagContainer* UGameplayAbility::GetCooldownTags() const
{
	UGameplayEffect* CDGE = GetCooldownGameplayEffect();
	return CDGE ? &CDGE->InheritableOwnedTagsContainer.CombinedTags : nullptr;
}

FGameplayAbilityActorInfo UGameplayAbility::GetActorInfo() const
{
	if (!ensure(CurrentActorInfo))
	{
		return FGameplayAbilityActorInfo();
	}
	return *CurrentActorInfo;
}

AActor* UGameplayAbility::GetOwningActorFromActorInfo() const
{
	if (!ensure(CurrentActorInfo))
	{
		return nullptr;
	}
	return CurrentActorInfo->OwnerActor.Get();
}

AActor* UGameplayAbility::GetAvatarActorFromActorInfo() const
{
	if (!ensure(CurrentActorInfo))
	{
		return nullptr;
	}
	return CurrentActorInfo->AvatarActor.Get();
}

USkeletalMeshComponent* UGameplayAbility::GetOwningComponentFromActorInfo() const
{
	if (!ensure(CurrentActorInfo))
	{
		return nullptr;
	}
	if (CurrentActorInfo->AnimInstance.IsValid())
	{
		return CurrentActorInfo->AnimInstance.Get()->GetOwningComponent();
	}
	return NULL;
}

FGameplayEffectSpecHandle UGameplayAbility::MakeOutgoingGameplayEffectSpec(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level) const
{
	check(CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid());
	return MakeOutgoingGameplayEffectSpec(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, GameplayEffectClass, Level);
}

FGameplayEffectSpecHandle UGameplayAbility::GetOutgoingGameplayEffectSpec(const UGameplayEffect* GameplayEffect, float Level) const
{
	if ( GameplayEffect )
	{
		return MakeOutgoingGameplayEffectSpec(GameplayEffect->GetClass(), Level);
	}
	
	return FGameplayEffectSpecHandle(nullptr);
}

FGameplayEffectSpecHandle UGameplayAbility::MakeOutgoingGameplayEffectSpec(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level) const
{
	check(ActorInfo);

	FGameplayEffectSpecHandle NewHandle = ActorInfo->AbilitySystemComponent->MakeOutgoingSpec(GameplayEffectClass, Level, GetEffectContext(Handle, ActorInfo));
	if (NewHandle.IsValid())
	{
		FGameplayAbilitySpec* AbilitySpec =  ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
		ApplyAbilityTagsToGameplayEffectSpec(*NewHandle.Data.Get(), AbilitySpec);
	}
	return NewHandle;
}

void UGameplayAbility::ApplyAbilityTagsToGameplayEffectSpec(FGameplayEffectSpec& Spec, FGameplayAbilitySpec* AbilitySpec) const
{
	Spec.CapturedSourceTags.GetSpecTags().AppendTags(AbilityTags);

	// Allow the source object of the ability to propagate tags along as well
	if (AbilitySpec)
	{
		const IGameplayTagAssetInterface* SourceObjAsTagInterface = Cast<IGameplayTagAssetInterface>(AbilitySpec->SourceObject);
		if (SourceObjAsTagInterface)
		{
			FGameplayTagContainer SourceObjTags;
			SourceObjAsTagInterface->GetOwnedGameplayTags(SourceObjTags);

			Spec.CapturedSourceTags.GetSpecTags().AppendTags(SourceObjTags);
		}
	}
}

/** Fixme: Naming is confusing here */

bool UGameplayAbility::K2_CommitAbility()
{
	check(CurrentActorInfo);
	return CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
}

bool UGameplayAbility::K2_CommitAbilityCooldown(bool BroadcastCommitEvent)
{
	check(CurrentActorInfo);
	if (BroadcastCommitEvent)
	{
		CurrentActorInfo->AbilitySystemComponent->NotifyAbilityCommit(this);
	}
	return CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
}

bool UGameplayAbility::K2_CommitAbilityCost(bool BroadcastCommitEvent)
{
	check(CurrentActorInfo);
	if (BroadcastCommitEvent)
	{
		CurrentActorInfo->AbilitySystemComponent->NotifyAbilityCommit(this);
	}
	return CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
}

bool UGameplayAbility::K2_CheckAbilityCooldown()
{
	check(CurrentActorInfo);
	return UAbilitySystemGlobals::Get().ShouldIgnoreCooldowns() || CheckCooldown(CurrentSpecHandle, CurrentActorInfo);
}

bool UGameplayAbility::K2_CheckAbilityCost()
{
	check(CurrentActorInfo);
	return UAbilitySystemGlobals::Get().ShouldIgnoreCosts() || CheckCost(CurrentSpecHandle, CurrentActorInfo);
}

void UGameplayAbility::K2_EndAbility()
{
	check(CurrentActorInfo);

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

// --------------------------------------------------------------------

void UGameplayAbility::MontageJumpToSection(FName SectionName)
{
	check(CurrentActorInfo);

	if (CurrentActorInfo->AbilitySystemComponent->IsAnimatingAbility(this))
	{
		CurrentActorInfo->AbilitySystemComponent->CurrentMontageJumpToSection(SectionName);
	}
}

void UGameplayAbility::MontageSetNextSectionName(FName FromSectionName, FName ToSectionName)
{
	check(CurrentActorInfo);

	if (CurrentActorInfo->AbilitySystemComponent->IsAnimatingAbility(this))
	{
		CurrentActorInfo->AbilitySystemComponent->CurrentMontageSetNextSectionName(FromSectionName, ToSectionName);
	}
}

void UGameplayAbility::MontageStop(float OverrideBlendOutTime)
{
	check(CurrentActorInfo);

	UAbilitySystemComponent* AbilitySystemComponent = CurrentActorInfo->AbilitySystemComponent.Get();
	if (AbilitySystemComponent != NULL)
	{
		// We should only stop the current montage if we are the animating ability
		if (AbilitySystemComponent->IsAnimatingAbility(this))
		{
			AbilitySystemComponent->CurrentMontageStop();
		}
	}
}

void UGameplayAbility::SetCurrentMontage(class UAnimMontage* InCurrentMontage)
{
	ensure(IsInstantiated());
	CurrentMontage = InCurrentMontage;
}

UAnimMontage* UGameplayAbility::GetCurrentMontage() const
{
	return CurrentMontage;
}

// --------------------------------------------------------------------

FGameplayAbilityTargetingLocationInfo UGameplayAbility::MakeTargetLocationInfoFromOwnerActor()
{
	FGameplayAbilityTargetingLocationInfo ReturnLocation;
	ReturnLocation.LocationType = EGameplayAbilityTargetingLocationType::ActorTransform;
	ReturnLocation.SourceActor = GetActorInfo().AvatarActor.Get();
	return ReturnLocation;
}

FGameplayAbilityTargetingLocationInfo UGameplayAbility::MakeTargetLocationInfoFromOwnerSkeletalMeshComponent(FName SocketName) const
{
	FGameplayAbilityTargetingLocationInfo ReturnLocation;
	ReturnLocation.LocationType = EGameplayAbilityTargetingLocationType::SocketTransform;
	ReturnLocation.SourceComponent = GetActorInfo().AnimInstance.IsValid() ? GetActorInfo().AnimInstance.Get()->GetOwningComponent() : NULL;
	ReturnLocation.SourceSocketName = SocketName;
	return ReturnLocation;
}

//----------------------------------------------------------------------

void UGameplayAbility::TaskStarted(UAbilityTask* NewTask)
{
	ABILITY_VLOG(CastChecked<AActor>(GetOuter()), Log, TEXT("Task Started %s"), *NewTask->GetName());

	ActiveTasks.Add(NewTask);
}

void UGameplayAbility::ConfirmTaskByInstanceName(FName InstanceName, bool bEndTask)
{
	TArray<TWeakObjectPtr<UAbilityTask>> NamedTasks = ActiveTasks.FilterByPredicate<FAbilityInstanceNamePredicate>(FAbilityInstanceNamePredicate(InstanceName));
	for (int32 i = NamedTasks.Num() - 1; i >= 0; --i)
	{
		if (NamedTasks[i].IsValid())
		{
			UAbilityTask* CurrentTask = NamedTasks[i].Get();
			CurrentTask->ExternalConfirm(bEndTask);
		}
	}
}

void UGameplayAbility::EndOrCancelTasksByInstanceName()
{
	for (int32 j = 0; j < EndTaskInstanceNames.Num(); ++j)
	{
		FName InstanceName = EndTaskInstanceNames[j];
		TArray<TWeakObjectPtr<UAbilityTask>> NamedTasks = ActiveTasks.FilterByPredicate<FAbilityInstanceNamePredicate>(FAbilityInstanceNamePredicate(InstanceName));
		for (int32 i = NamedTasks.Num() - 1; i >= 0; --i)
		{
			if (NamedTasks[i].IsValid())
			{
				UAbilityTask* CurrentTask = NamedTasks[i].Get();
				CurrentTask->EndTask();
			}
		}
	}
	EndTaskInstanceNames.Empty();

	for (int32 j = 0; j < CancelTaskInstanceNames.Num(); ++j)
	{
		FName InstanceName = CancelTaskInstanceNames[j];
		TArray<TWeakObjectPtr<UAbilityTask>> NamedTasks = ActiveTasks.FilterByPredicate<FAbilityInstanceNamePredicate>(FAbilityInstanceNamePredicate(InstanceName));
		for (int32 i = NamedTasks.Num() - 1; i >= 0; --i)
		{
			if (NamedTasks[i].IsValid())
			{
				UAbilityTask* CurrentTask = NamedTasks[i].Get();
				CurrentTask->ExternalCancel();
			}
		}
	}
	CancelTaskInstanceNames.Empty();
}

void UGameplayAbility::EndTaskByInstanceName(FName InstanceName)
{
	//Avoid race condition by delaying for one frame
	EndTaskInstanceNames.AddUnique(InstanceName);
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UGameplayAbility::EndOrCancelTasksByInstanceName);
}

void UGameplayAbility::CancelTaskByInstanceName(FName InstanceName)
{
	//Avoid race condition by delaying for one frame
	CancelTaskInstanceNames.AddUnique(InstanceName);
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UGameplayAbility::EndOrCancelTasksByInstanceName);
}

void UGameplayAbility::EndAbilityState(FName OptionalStateNameToEnd)
{
	check(CurrentActorInfo);

	if (OnGameplayAbilityStateEnded.IsBound())
	{
		OnGameplayAbilityStateEnded.Broadcast(OptionalStateNameToEnd);
	}
}

void UGameplayAbility::TaskEnded(UAbilityTask* Task)
{
	ABILITY_VLOG(CastChecked<AActor>(GetOuter()), Log, TEXT("Task Ended %s"), *Task->GetName());

	ActiveTasks.Remove(Task);
}

/**
 *	Helper methods for adding GaAmeplayCues without having to go through GameplayEffects.
 *	For now, none of these will happen predictively. We can eventually build this out more to 
 *	work with the PredictionKey system.
 */

void UGameplayAbility::K2_ExecuteGameplayCue(FGameplayTag GameplayCueTag, FGameplayEffectContextHandle Context)
{
	check(CurrentActorInfo);
	CurrentActorInfo->AbilitySystemComponent->ExecuteGameplayCue(GameplayCueTag, Context);
}

void UGameplayAbility::K2_AddGameplayCue(FGameplayTag GameplayCueTag, FGameplayEffectContextHandle Context, bool bRemoveOnAbilityEnd)
{
	check(CurrentActorInfo);
	CurrentActorInfo->AbilitySystemComponent->AddGameplayCue(GameplayCueTag, Context);

	if (bRemoveOnAbilityEnd)
	{
		TrackedGameplayCues.Add(GameplayCueTag);
	}
}

void UGameplayAbility::K2_RemoveGameplayCue(FGameplayTag GameplayCueTag)
{
	check(CurrentActorInfo);
	CurrentActorInfo->AbilitySystemComponent->RemoveGameplayCue(GameplayCueTag);

	TrackedGameplayCues.Remove(GameplayCueTag);
}

FGameplayEffectContextHandle UGameplayAbility::GetContextFromOwner(FGameplayAbilityTargetDataHandle OptionalTargetData) const
{
	check(CurrentActorInfo);
	FGameplayEffectContextHandle Context = GetEffectContext(CurrentSpecHandle, CurrentActorInfo);
	
	for (auto Data : OptionalTargetData.Data)
	{
		if (Data.IsValid())
		{
			Data->AddTargetDataToContext(Context, true);
		}
	}

	return Context;
}

int32 UGameplayAbility::GetAbilityLevel() const
{
	check(IsInstantiated()); // You should not call this on non instanced abilities.
	check(CurrentActorInfo);
	return GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo);
}

/** Returns current ability level for non instanced abilities. You m ust call this version in these contexts! */
int32 UGameplayAbility::GetAbilityLevel(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	FGameplayAbilitySpec* Spec = ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
	check(Spec);

	return Spec->Level;
}

FGameplayAbilitySpec* UGameplayAbility::GetCurrentAbilitySpec() const
{
	check(IsInstantiated()); // You should not call this on non instanced abilities.
	check(CurrentActorInfo);
	return CurrentActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(CurrentSpecHandle);
}

UObject* UGameplayAbility::GetSourceObject(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (ActorInfo != NULL)
	{
		UAbilitySystemComponent* AbilitySystemComponent = ActorInfo->AbilitySystemComponent.Get();
		if (AbilitySystemComponent != NULL)
		{
			FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
			if (AbilitySpec)
			{
				return AbilitySpec->SourceObject;
			}
		}
	}
	return nullptr;
}

UObject* UGameplayAbility::GetCurrentSourceObject() const
{
	FGameplayAbilitySpec* AbilitySpec = GetCurrentAbilitySpec();
	if (AbilitySpec)
	{
		return AbilitySpec->SourceObject;
	}
	return nullptr;
}

FGameplayEffectContextHandle UGameplayAbility::GetEffectContext(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo) const
{
	check(ActorInfo);
	FGameplayEffectContextHandle Context = FGameplayEffectContextHandle(UAbilitySystemGlobals::Get().AllocGameplayEffectContext());
	// By default use the owner and avatar as the instigator and causer
	Context.AddInstigator(ActorInfo->OwnerActor.Get(), ActorInfo->AvatarActor.Get());
	return Context;
}

bool UGameplayAbility::IsTriggered() const
{
	// Assume that if there is triggered data, then we are triggered. 
	// If we need to support abilities that can be both, this will need to be expanded.
	return AbilityTriggers.Num() > 0;
}

bool UGameplayAbility::IsPredictingClient() const
{
	if (GetCurrentActorInfo()->OwnerActor.IsValid())
	{
		bool bIsLocallyControlled = GetCurrentActorInfo()->IsLocallyControlled();
		bool bIsAuthority = GetCurrentActorInfo()->IsNetAuthority();

		// LocalPredicted and ServerInitiated are both valid because in both those modes the ability also runs on the client
		if (!bIsAuthority && bIsLocallyControlled && (GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted || GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerInitiated))
		{
			return true;
		}
	}

	return false;
}

bool UGameplayAbility::IsForRemoteClient() const
{
	if (GetCurrentActorInfo()->OwnerActor.IsValid())
	{
		bool bIsLocallyControlled = GetCurrentActorInfo()->IsLocallyControlled();
		bool bIsAuthority = GetCurrentActorInfo()->IsNetAuthority();

		if (bIsAuthority && !bIsLocallyControlled)
		{
			return true;
		}
	}

	return false;
}

bool UGameplayAbility::IsLocallyControlled() const
{
	if (GetCurrentActorInfo()->OwnerActor.IsValid())
	{
		return GetCurrentActorInfo()->IsLocallyControlled();
	}

	return false;
}

bool UGameplayAbility::HasAuthority(const FGameplayAbilityActivationInfo* ActivationInfo) const
{
	return (ActivationInfo->ActivationMode == EGameplayAbilityActivationMode::Authority);
}

bool UGameplayAbility::HasAuthorityOrPredictionKey(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo* ActivationInfo) const
{
	return ActorInfo->AbilitySystemComponent->HasAuthorityOrPredictionKey(ActivationInfo);
}

void UGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	SetCurrentActorInfo(Spec.Handle, ActorInfo);

	// If we already have an avatar set, call the OnAvatarSet event as well
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		OnAvatarSet(ActorInfo, Spec);
	}
}

void UGameplayAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	// Projects may want to initiate passives or do other "BeginPlay" type of logic here.
}

// -------------------------------------------------------

FActiveGameplayEffectHandle UGameplayAbility::BP_ApplyGameplayEffectToOwner(TSubclassOf<UGameplayEffect> GameplayEffectClass, int32 GameplayEffectLevel)
{
	check(CurrentActorInfo);
	check(CurrentSpecHandle.IsValid());

	if ( GameplayEffectClass )
	{
		const UGameplayEffect* GameplayEffect = GameplayEffectClass->GetDefaultObject<UGameplayEffect>();
		return ApplyGameplayEffectToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, GameplayEffect, GameplayEffectLevel);
	}

	ABILITY_LOG(Error, TEXT("BP_ApplyGameplayEffectToOwner called on ability %s with no GameplayEffectClass."), *GetName());
	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle UGameplayAbility::K2_ApplyGameplayEffectToOwner(const UGameplayEffect* GameplayEffect, int32 GameplayEffectLevel)
{
	if ( GameplayEffect )
	{
		return BP_ApplyGameplayEffectToOwner(GameplayEffect->GetClass(), GameplayEffectLevel);
	}

	ABILITY_LOG(Error, TEXT("K2_ApplyGameplayEffectToOwner called on ability %s with no GameplayEffect."), *GetName());
	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle UGameplayAbility::ApplyGameplayEffectToOwner(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const UGameplayEffect* GameplayEffect, float GameplayEffectLevel) const
{
	if (GameplayEffect && (HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo)))
	{
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, GameplayEffect->GetClass(), GameplayEffectLevel);
		if (SpecHandle.IsValid())
		{
			return ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		}
	}

	// We cannot apply GameplayEffects in this context. Return an empty handle.
	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle UGameplayAbility::K2_ApplyGameplayEffectSpecToOwner(const FGameplayEffectSpecHandle EffectSpecHandle)
{
	return ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, EffectSpecHandle);
}

FActiveGameplayEffectHandle UGameplayAbility::ApplyGameplayEffectSpecToOwner(const FGameplayAbilitySpecHandle AbilityHandle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEffectSpecHandle SpecHandle) const
{
	// This batches all created cues together
	FScopedGameplayCueSendContext GameplayCueSendContext;

	if (SpecHandle.IsValid() && (HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo)))
	{
		return ActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get(), ActorInfo->AbilitySystemComponent->GetPredictionKeyForNewAction());

	}
	return FActiveGameplayEffectHandle();
}

// -------------------------------

TArray<FActiveGameplayEffectHandle> UGameplayAbility::BP_ApplyGameplayEffectToTarget(FGameplayAbilityTargetDataHandle Target, TSubclassOf<UGameplayEffect> GameplayEffectClass, int32 GameplayEffectLevel)
{
	return ApplyGameplayEffectToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Target, GameplayEffectClass, GameplayEffectLevel);
}

TArray<FActiveGameplayEffectHandle> UGameplayAbility::K2_ApplyGameplayEffectToTarget(FGameplayAbilityTargetDataHandle Target, const UGameplayEffect* GameplayEffect, int32 GameplayEffectLevel)
{
	return BP_ApplyGameplayEffectToTarget(Target, GameplayEffect->GetClass(), GameplayEffectLevel);
}

TArray<FActiveGameplayEffectHandle> UGameplayAbility::ApplyGameplayEffectToTarget(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FGameplayAbilityTargetDataHandle Target, TSubclassOf<UGameplayEffect> GameplayEffectClass, float GameplayEffectLevel) const
{
	TArray<FActiveGameplayEffectHandle> EffectHandles;

	// This batches all created cues together
	FScopedGameplayCueSendContext GameplayCueSendContext;

	if (GameplayEffectClass == nullptr)
	{
		ABILITY_LOG(Error, TEXT("ApplyGameplayEffectToTarget called on ability %s with no GameplayEffect."), *GetName());
	}
	else if (HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, GameplayEffectClass, GameplayEffectLevel);
		EffectHandles.Append(ApplyGameplayEffectSpecToTarget(Handle, ActorInfo, ActivationInfo, SpecHandle, Target));
	}

	return EffectHandles;
}

TArray<FActiveGameplayEffectHandle> UGameplayAbility::K2_ApplyGameplayEffectSpecToTarget(const FGameplayEffectSpecHandle SpecHandle, FGameplayAbilityTargetDataHandle TargetData)
{
	return ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, TargetData);
}

TArray<FActiveGameplayEffectHandle> UGameplayAbility::ApplyGameplayEffectSpecToTarget(const FGameplayAbilitySpecHandle AbilityHandle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEffectSpecHandle SpecHandle, FGameplayAbilityTargetDataHandle TargetData) const
{
	TArray<FActiveGameplayEffectHandle> EffectHandles;
	
	if (SpecHandle.IsValid() && HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		for (auto Data : TargetData.Data)
		{
			EffectHandles.Append(Data->ApplyGameplayEffectSpec(*SpecHandle.Data.Get(), ActorInfo->AbilitySystemComponent->GetPredictionKeyForNewAction()));
		}
	}
	return EffectHandles;
}

void UGameplayAbility::BP_RemoveGameplayEffectFromOwnerWithAssetTags(FGameplayTagContainer WithTags, int32 StacksToRemove)
{
	if (HasAuthority(&CurrentActivationInfo) == false)
	{
		return;
	}

	FActiveGameplayEffectQuery Query;
	Query.EffectTagContainer = &WithTags;
	CurrentActorInfo->AbilitySystemComponent->RemoveActiveEffects(Query, StacksToRemove);
}

void UGameplayAbility::BP_RemoveGameplayEffectFromOwnerWithGrantedTags(FGameplayTagContainer WithGrantedTags, int32 StacksToRemove)
{
	if (HasAuthority(&CurrentActivationInfo) == false)
	{
		return;
	}

	FActiveGameplayEffectQuery Query;
	Query.OwningTagContainer = &WithGrantedTags;
	CurrentActorInfo->AbilitySystemComponent->RemoveActiveEffects(Query, StacksToRemove);
}

void UGameplayAbility::ConvertDeprecatedGameplayEffectReferencesToBlueprintReferences(UGameplayEffect* OldGE, TSubclassOf<UGameplayEffect> NewGEClass)
{
	bool bChangedSomething = false;
	if ( CooldownGameplayEffect && CooldownGameplayEffect == OldGE )
	{
		if ( !CooldownGameplayEffectClass )
		{
			CooldownGameplayEffectClass = NewGEClass;
		}

		CooldownGameplayEffect = nullptr;

		bChangedSomething = true;
	}

	if ( CostGameplayEffect && CostGameplayEffect == OldGE )
	{
		if ( !CostGameplayEffectClass )
		{
			CostGameplayEffectClass = NewGEClass;
		}

		CostGameplayEffect = nullptr;

		bChangedSomething = true;
	}

	if ( bChangedSomething )
	{
		MarkPackageDirty();
	}
}
