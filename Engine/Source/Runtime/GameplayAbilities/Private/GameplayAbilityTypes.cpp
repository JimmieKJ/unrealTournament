// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsModule.h"
#include "GameplayEffectTypes.h"
#include "GameplayAbility.h"

//----------------------------------------------------------------------

void FGameplayAbilityActorInfo::InitFromActor(AActor *InOwnerActor, AActor *InAvatarActor, UAbilitySystemComponent* InAbilitySystemComponent)
{
	check(InOwnerActor);
	check(InAbilitySystemComponent);

	OwnerActor = InOwnerActor;
	AvatarActor = InAvatarActor;
	AbilitySystemComponent = InAbilitySystemComponent;

	APlayerController* OldPC = PlayerController.Get();

	// Look for a player controller or pawn in the owner chain.
	AActor *TestActor = InOwnerActor;
	while (TestActor)
	{
		if (APlayerController * CastPC = Cast<APlayerController>(TestActor))
		{
			PlayerController = CastPC;
			break;
		}

		if (APawn * Pawn = Cast<APawn>(TestActor))
		{
			PlayerController = Cast<APlayerController>(Pawn->GetController());
			break;
		}

		TestActor = TestActor->GetOwner();
	}

	// Notify ASC if PlayerController was found for first time
	if (OldPC == nullptr && PlayerController.IsValid())
	{
		InAbilitySystemComponent->OnPlayerControllerSet();
	}

	if (AvatarActor.Get())
	{
		// Grab Components that we care about
		SkeletalMeshComponent = AvatarActor->FindComponentByClass<USkeletalMeshComponent>();
		MovementComponent = AvatarActor->FindComponentByClass<UMovementComponent>();
	}
	else
	{
		SkeletalMeshComponent = nullptr;
		MovementComponent = nullptr;
	}
}

void FGameplayAbilityActorInfo::SetAvatarActor(AActor *InAvatarActor)
{
	InitFromActor(OwnerActor.Get(), InAvatarActor, AbilitySystemComponent.Get());
}

void FGameplayAbilityActorInfo::ClearActorInfo()
{
	OwnerActor = nullptr;
	AvatarActor = nullptr;
	PlayerController = nullptr;
	SkeletalMeshComponent = nullptr;
	MovementComponent = nullptr;
}

bool FGameplayAbilityActorInfo::IsLocallyControlled() const
{
	if (PlayerController.IsValid())
	{
		return PlayerController->IsLocalController();
	}
	else if (IsNetAuthority())
	{
		// Non-players are always locally controlled on the server
		return true;
	}

	return false;
}

bool FGameplayAbilityActorInfo::IsLocallyControlledPlayer() const
{
	if (PlayerController.IsValid())
	{
		return PlayerController->IsLocalController();
	}

	return false;
}

bool FGameplayAbilityActorInfo::IsNetAuthority() const
{
	// Make sure this works on pending kill actors
	if (OwnerActor.IsValid(true))
	{
		return (OwnerActor.Get(true)->Role == ROLE_Authority);
	}

	// If we encounter issues with this being called before or after the owning actor is destroyed,
	// we may need to cache off the authority (or look for it on some global/world state).

	ensure(false);
	return false;
}

void FGameplayAbilityActivationInfo::SetPredicting(FPredictionKey PredictionKey)
{
	ActivationMode = EGameplayAbilityActivationMode::Predicting;
	PredictionKeyWhenActivated = PredictionKey;

	// Abilities can be cancelled by server at any time. There is no reason to have to wait until confirmation.
	// prediction keys keep previous activations of abilities from ending future activations.
	bCanBeEndedByOtherInstance = true;
}

void FGameplayAbilityActivationInfo::ServerSetActivationPredictionKey(FPredictionKey PredictionKey)
{
	PredictionKeyWhenActivated = PredictionKey;
}

void FGameplayAbilityActivationInfo::SetActivationConfirmed()
{
	ActivationMode = EGameplayAbilityActivationMode::Confirmed;
	//Remote (server) commands to end the ability that come in after this point are considered for this instance
	bCanBeEndedByOtherInstance = true;
}

void FGameplayAbilityActivationInfo::SetActivationRejected()
{
	ActivationMode = EGameplayAbilityActivationMode::Rejected;
}

bool FGameplayAbilitySpec::IsActive() const
{
	// If ability hasn't replicated yet we're not active
	return Ability != nullptr && ActiveCount > 0;
}

UGameplayAbility* FGameplayAbilitySpec::GetPrimaryInstance() const
{
	if (Ability && Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor)
	{
		if (NonReplicatedInstances.Num() > 0)
		{
			return NonReplicatedInstances[0];
		}
		if (ReplicatedInstances.Num() > 0)
		{
			return ReplicatedInstances[0];
		}
	}
	return nullptr;
}

void FGameplayAbilitySpec::PreReplicatedRemove(const struct FGameplayAbilitySpecContainer& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->OnRemoveAbility(*this);
	}
}

void FGameplayAbilitySpec::PostReplicatedAdd(const struct FGameplayAbilitySpecContainer& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->OnGiveAbility(*this);
	}
}

void FGameplayAbilitySpecContainer::RegisterWithOwner(UAbilitySystemComponent* InOwner)
{
	Owner = InOwner;
}

// ----------------------------------------------------

FGameplayAbilitySpec::FGameplayAbilitySpec(FGameplayAbilitySpecDef& InDef, int32 InGameplayEffectLevel, FActiveGameplayEffectHandle InGameplayEffectHandle)
	: Ability(InDef.Ability ? InDef.Ability->GetDefaultObject<UGameplayAbility>() : nullptr)
	, InputID(InDef.InputID)
	, SourceObject(InDef.SourceObject)
	, ActiveCount(0)
	, InputPressed(false)
	, RemoveAfterActivation(false)
	, PendingRemove(false)
{
	Handle.GenerateNewHandle();
	InDef.AssignedHandle = Handle;
	GameplayEffectHandle = InGameplayEffectHandle;

	FString ContextString = FString::Printf(TEXT("FGameplayAbilitySpec::FGameplayAbilitySpec for %s from %s"), 
		(InDef.Ability ? *InDef.Ability->GetName() : TEXT("INVALID ABILITY")), 
		(InDef.SourceObject ? *InDef.SourceObject->GetName() : TEXT("INVALID ABILITY")));
	Level = InDef.LevelScalableFloat.GetValueAtLevel(InGameplayEffectLevel, &ContextString);
}

// ----------------------------------------------------

FScopedAbilityListLock::FScopedAbilityListLock(UAbilitySystemComponent& InAbilitySystemComponent)
	: AbilitySystemComponent(InAbilitySystemComponent)
{
	AbilitySystemComponent.IncrementAbilityListLock();
}

FScopedAbilityListLock::~FScopedAbilityListLock()
{
	AbilitySystemComponent.DecrementAbilityListLock();
}

// ----------------------------------------------------

FScopedTargetListLock::FScopedTargetListLock(UAbilitySystemComponent& InAbilitySystemComponent, const UGameplayAbility& InAbility)
	: GameplayAbility(InAbility)
	, AbilityLock(InAbilitySystemComponent)
{
	GameplayAbility.IncrementListLock();
}

FScopedTargetListLock::~FScopedTargetListLock()
{
	GameplayAbility.DecrementListLock();
}
