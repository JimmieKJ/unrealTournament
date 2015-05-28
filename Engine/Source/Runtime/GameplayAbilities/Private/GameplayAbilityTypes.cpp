// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

	if (AvatarActor.Get())
	{
		// Grab Components that we care about
		USkeletalMeshComponent * SkelMeshComponent = AvatarActor->FindComponentByClass<USkeletalMeshComponent>();
		if (SkelMeshComponent)
		{
			this->AnimInstance = SkelMeshComponent->GetAnimInstance();
		}

		MovementComponent = AvatarActor->FindComponentByClass<UMovementComponent>();
	}
	else
	{
		MovementComponent = NULL;
		AnimInstance = NULL;
	}
}

void FGameplayAbilityActorInfo::SetAvatarActor(AActor *InAvatarActor)
{
	InitFromActor(OwnerActor.Get(), InAvatarActor, AbilitySystemComponent.Get());
}

void FGameplayAbilityActorInfo::ClearActorInfo()
{
	OwnerActor = NULL;
	AvatarActor = NULL;
	PlayerController = NULL;
	AnimInstance = NULL;
	MovementComponent = NULL;
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

FGameplayAbilitySpec:: FGameplayAbilitySpec(FGameplayAbilitySpecDef& InDef, FActiveGameplayEffectHandle InGameplayEffectHandle)
	: Ability(InDef.Ability ? InDef.Ability->GetDefaultObject<UGameplayAbility>() : nullptr)
	, Level(InDef.Level)
	, InputID(InDef.InputID)
	, SourceObject(InDef.SourceObject)
	, InputPressed(false)
	, ActiveCount(0)
	, RemoveAfterActivation(false)
	, PendingRemove(false)
{
	Handle.GenerateNewHandle();
	InDef.AssignedHandle = Handle;
	GameplayEffectHandle = InGameplayEffectHandle;
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