// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "Abilities/GameplayAbility_Montage.h"
#include "GameplayEffect.h"
#include "AbilitySystemComponent.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	UGameplayAbility_Montage
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

UGameplayAbility_Montage::UGameplayAbility_Montage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayRate = 1.f;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

void UGameplayAbility_Montage::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		return;
	}

	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();

	if (MontageToPlay != nullptr && AnimInstance != nullptr && AnimInstance->GetActiveMontageInstance() == nullptr)
	{
		TArray<FActiveGameplayEffectHandle>	AppliedEffects;

		// Apply GameplayEffects
		TArray<const UGameplayEffect*> Effects;
		GetGameplayEffectsWhileAnimating(Effects);
		for (const UGameplayEffect* Effect : Effects)
		{
			FActiveGameplayEffectHandle EffectHandle = ActorInfo->AbilitySystemComponent->ApplyGameplayEffectToSelf(Effect, 1.f, MakeEffectContext(Handle, ActorInfo));
			if (EffectHandle.IsValid())
			{
				AppliedEffects.Add(EffectHandle);
			}
		}

		float const Duration = AnimInstance->Montage_Play(MontageToPlay, PlayRate);

		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &UGameplayAbility_Montage::OnMontageEnded, ActorInfo->AbilitySystemComponent, AppliedEffects);
		AnimInstance->Montage_SetEndDelegate(EndDelegate);

		if (SectionName != NAME_None)
		{
			AnimInstance->Montage_JumpToSection(SectionName);
		}
	}
}

void UGameplayAbility_Montage::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent, TArray<FActiveGameplayEffectHandle> AppliedEffects)
{
	// Remove any GameplayEffects that we applied
	if (AbilitySystemComponent.IsValid())
	{
		for (FActiveGameplayEffectHandle Handle : AppliedEffects)
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);
		}
	}
}

void UGameplayAbility_Montage::GetGameplayEffectsWhileAnimating(TArray<const UGameplayEffect*>& OutEffects) const
{
	OutEffects.Append(GameplayEffectsWhileAnimating);

	for ( TSubclassOf<UGameplayEffect> EffectClass : GameplayEffectClassesWhileAnimating )
	{
		if ( EffectClass )
		{
			OutEffects.Add(EffectClass->GetDefaultObject<UGameplayEffect>());
		}
	}
}

void UGameplayAbility_Montage::ConvertDeprecatedGameplayEffectReferencesToBlueprintReferences(UGameplayEffect* OldGE, TSubclassOf<UGameplayEffect> NewGEClass)
{
	Super::ConvertDeprecatedGameplayEffectReferencesToBlueprintReferences(OldGE, NewGEClass);

	bool bChangedSomething = false;

	for ( int32 GEIdx = GameplayEffectsWhileAnimating.Num() - 1; GEIdx >= 0; --GEIdx )
	{
		const UGameplayEffect* Effect = GameplayEffectsWhileAnimating[GEIdx];
		if ( Effect && Effect == OldGE )
		{
			GameplayEffectClassesWhileAnimating.Add(NewGEClass);
			GameplayEffectsWhileAnimating.RemoveAt(GEIdx);

			bChangedSomething = true;
		}
	}

	if (bChangedSomething)
	{
		MarkPackageDirty();
	}
}