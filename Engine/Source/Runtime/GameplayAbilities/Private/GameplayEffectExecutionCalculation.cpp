// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "GameplayEffectExecutionCalculation.h"
#include "AbilitySystemComponent.h"

FGameplayEffectCustomExecutionParameters::FGameplayEffectCustomExecutionParameters()
	: OwningSpec(nullptr)
	, TargetAbilitySystemComponent(nullptr)
{
}

FGameplayEffectCustomExecutionParameters::FGameplayEffectCustomExecutionParameters(FGameplayEffectSpec& InOwningSpec, const TArray<FGameplayEffectExecutionScopedModifierInfo>& InScopedMods, UAbilitySystemComponent* InTargetAbilityComponent)
	: OwningSpec(&InOwningSpec)
	, TargetAbilitySystemComponent(InTargetAbilityComponent)
{
	check(InOwningSpec.Def);

	FActiveGameplayEffectHandle ModifierHandle = FActiveGameplayEffectHandle::GenerateNewHandle(InTargetAbilityComponent);

	for (const FGameplayEffectExecutionScopedModifierInfo& CurScopedMod : InScopedMods)
	{
		FAggregator* ScopedAggregator = ScopedModifierAggregators.Find(CurScopedMod.CapturedAttribute);
		if (!ScopedAggregator)
		{
			const FGameplayEffectAttributeCaptureSpec* CaptureSpec = InOwningSpec.CapturedRelevantAttributes.FindCaptureSpecByDefinition(CurScopedMod.CapturedAttribute, true);

			FAggregator SnapshotAgg;
			if (CaptureSpec && CaptureSpec->AttemptGetAttributeAggregatorSnapshot(SnapshotAgg))
			{
				ScopedAggregator = &(ScopedModifierAggregators.Add(CurScopedMod.CapturedAttribute, SnapshotAgg));
			}
		}
		
		float ModEvalValue = 0.f;
		if (ScopedAggregator && CurScopedMod.ModifierMagnitude.AttemptCalculateMagnitude(InOwningSpec, ModEvalValue))
		{
			ScopedAggregator->AddMod(ModEvalValue, CurScopedMod.ModifierOp, &CurScopedMod.SourceTags, &CurScopedMod.TargetTags, false, ModifierHandle);
		}
		else
		{
			ABILITY_LOG(Warning, TEXT("Attempted to apply a scoped modifier from %s's %s magnitude calculation that could not be properly calculated. Some attributes necessary for the calculation were missing."), *InOwningSpec.Def->GetName(), *CurScopedMod.CapturedAttribute.ToSimpleString());
		}
	}
}

const FGameplayEffectSpec& FGameplayEffectCustomExecutionParameters::GetOwningSpec() const
{
	check(OwningSpec);
	return *OwningSpec;
}

FGameplayEffectSpec& FGameplayEffectCustomExecutionParameters::GetOwningSpec()
{
	check(OwningSpec);
	return *OwningSpec;
}


UAbilitySystemComponent* FGameplayEffectCustomExecutionParameters::GetTargetAbilitySystemComponent() const
{
	return TargetAbilitySystemComponent.Get();
}

UAbilitySystemComponent* FGameplayEffectCustomExecutionParameters::GetSourcebilitySystemComponent() const
{
	check(OwningSpec);
	return OwningSpec->GetContext().GetInstigatorAbilitySystemComponent();
}

bool FGameplayEffectCustomExecutionParameters::AttemptCalculateCapturedAttributeMagnitude(const FGameplayEffectAttributeCaptureDefinition& InCaptureDef, const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutMagnitude) const
{
	check(OwningSpec);

	const FAggregator* CalcAgg = ScopedModifierAggregators.Find(InCaptureDef);
	if (CalcAgg)
	{
		OutMagnitude = CalcAgg->Evaluate(InEvalParams);
		return true;
	}
	else
	{
		const FGameplayEffectAttributeCaptureSpec* CaptureSpec = OwningSpec->CapturedRelevantAttributes.FindCaptureSpecByDefinition(InCaptureDef, true);
		if (CaptureSpec)
		{
			return CaptureSpec->AttemptCalculateAttributeMagnitude(InEvalParams, OutMagnitude);
		}
	}

	return false;
}

bool FGameplayEffectCustomExecutionParameters::AttemptCalculateCapturedAttributeMagnitudeWithBase(const FGameplayEffectAttributeCaptureDefinition& InCaptureDef, const FAggregatorEvaluateParameters& InEvalParams, float InBaseValue, OUT float& OutMagnitude) const
{
	check(OwningSpec);

	const FAggregator* CalcAgg = ScopedModifierAggregators.Find(InCaptureDef);
	if (CalcAgg)
	{
		OutMagnitude = CalcAgg->EvaluateWithBase(InBaseValue, InEvalParams);
		return true;
	}
	else
	{
		const FGameplayEffectAttributeCaptureSpec* CaptureSpec = OwningSpec->CapturedRelevantAttributes.FindCaptureSpecByDefinition(InCaptureDef, true);
		if (CaptureSpec)
		{
			return CaptureSpec->AttemptCalculateAttributeMagnitudeWithBase(InEvalParams, InBaseValue, OutMagnitude);
		}
	}

	return false;
}

bool FGameplayEffectCustomExecutionParameters::AttemptCalculateCapturedAttributeBaseValue(const FGameplayEffectAttributeCaptureDefinition& InCaptureDef, OUT float& OutBaseValue) const
{
	check(OwningSpec);

	const FAggregator* CalcAgg = ScopedModifierAggregators.Find(InCaptureDef);
	if (CalcAgg)
	{
		OutBaseValue = CalcAgg->GetBaseValue();
		return true;
	}
	else
	{
		const FGameplayEffectAttributeCaptureSpec* CaptureSpec = OwningSpec->CapturedRelevantAttributes.FindCaptureSpecByDefinition(InCaptureDef, true);
		if (CaptureSpec)
		{
			return CaptureSpec->AttemptCalculateAttributeBaseValue(OutBaseValue);
		}
	}

	return false;
}

bool FGameplayEffectCustomExecutionParameters::AttemptCalculateCapturedAttributeBonusMagnitude(const FGameplayEffectAttributeCaptureDefinition& InCaptureDef, const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutBonusMagnitude) const
{
	check(OwningSpec);

	const FAggregator* CalcAgg = ScopedModifierAggregators.Find(InCaptureDef);
	if (CalcAgg)
	{
		OutBonusMagnitude = CalcAgg->EvaluateBonus(InEvalParams);
		return true;
	}
	else
	{
		const FGameplayEffectAttributeCaptureSpec* CaptureSpec = OwningSpec->CapturedRelevantAttributes.FindCaptureSpecByDefinition(InCaptureDef, true);
		if (CaptureSpec)
		{
			return CaptureSpec->AttemptCalculateAttributeBonusMagnitude(InEvalParams, OutBonusMagnitude);
		}
	}

	return false;
}

bool FGameplayEffectCustomExecutionParameters::AttemptGetCapturedAttributeAggregatorSnapshot(const FGameplayEffectAttributeCaptureDefinition& InCaptureDef, OUT FAggregator& OutSnapshottedAggregator) const
{
	check(OwningSpec);

	const FAggregator* CalcAgg = ScopedModifierAggregators.Find(InCaptureDef);
	if (CalcAgg)
	{
		OutSnapshottedAggregator.TakeSnapshotOf(*CalcAgg);
		return true;
	}
	else
	{
		const FGameplayEffectAttributeCaptureSpec* CaptureSpec = OwningSpec->CapturedRelevantAttributes.FindCaptureSpecByDefinition(InCaptureDef, true);
		if (CaptureSpec)
		{
			return CaptureSpec->AttemptGetAttributeAggregatorSnapshot(OutSnapshottedAggregator);
		}
	}

	return false;
}

UGameplayEffectExecutionCalculation::UGameplayEffectExecutionCalculation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITORONLY_DATA
void UGameplayEffectExecutionCalculation::GetValidScopedModifierAttributeCaptureDefinitions(OUT TArray<FGameplayEffectAttributeCaptureDefinition>& OutScopableModifiers) const
{
	OutScopableModifiers.Empty();

	const TArray<FGameplayEffectAttributeCaptureDefinition>& DefaultCaptureDefs = GetAttributeCaptureDefinitions();
	for (const FGameplayEffectAttributeCaptureDefinition& CurDef : DefaultCaptureDefs)
	{
		if (!InvalidScopedModifierAttributes.Contains(CurDef))
		{
			OutScopableModifiers.Add(CurDef);
		}
	}
}
#endif // #if WITH_EDITORONLY_DATA

void UGameplayEffectExecutionCalculation::Execute_Implementation(FGameplayEffectCustomExecutionParameters& ExecutionParams, OUT TArray<FGameplayModifierEvaluatedData>& OutAdditionalModifiers) const
{
}
