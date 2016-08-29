// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "GameplayEffectAggregator.h"
#include "AbilitySystemComponent.h"

bool FAggregatorMod::Qualifies(const FAggregatorEvaluateParameters& Parameters) const
{
	bool bSourceMet = (!SourceTagReqs || SourceTagReqs->IsEmpty()) || (Parameters.SourceTags && SourceTagReqs->RequirementsMet(*Parameters.SourceTags));
	bool bTargetMet = (!TargetTagReqs || TargetTagReqs->IsEmpty()) || (Parameters.TargetTags && TargetTagReqs->RequirementsMet(*Parameters.TargetTags));

	bool bSourceFilterMet = (Parameters.AppliedSourceTagFilter.Num() == 0);
	bool bTargetFilterMet = (Parameters.AppliedTargetTagFilter.Num() == 0);

	if (Parameters.IncludePredictiveMods == false && IsPredicted)
	{
		return false;
	}
	
	const UAbilitySystemComponent* HandleComponent = ActiveHandle.GetOwningAbilitySystemComponent();
	if (HandleComponent)
	{
		if (!bSourceFilterMet)
		{
			const FGameplayTagContainer* SourceTags = HandleComponent->GetGameplayEffectSourceTagsFromHandle(ActiveHandle);
			bSourceFilterMet = (SourceTags && SourceTags->MatchesAll(Parameters.AppliedSourceTagFilter, false));
		}

		if (!bTargetFilterMet)
		{
			const FGameplayTagContainer* TargetTags = HandleComponent->GetGameplayEffectTargetTagsFromHandle(ActiveHandle);
			bTargetFilterMet = (TargetTags && TargetTags->MatchesAll(Parameters.AppliedTargetTagFilter, false));
		}
	}

	return bSourceMet && bTargetMet && bSourceFilterMet && bTargetFilterMet;
}

FAggregator::~FAggregator()
{
	int32 NumRemoved = FScopedAggregatorOnDirtyBatch::DirtyAggregators.Remove(this);
	ensure(NumRemoved == 0);
}

float FAggregator::Evaluate(const FAggregatorEvaluateParameters& Parameters) const
{
	return EvaluateWithBase(BaseValue, Parameters);
}

float FAggregator::EvaluateWithBase(float InlineBaseValue, const FAggregatorEvaluateParameters& Parameters) const
{
	for (const FAggregatorMod& Mod : Mods[EGameplayModOp::Override])
	{
		if (Mod.Qualifies(Parameters))
		{
			return Mod.EvaluatedMagnitude;
		}
	}

	float Additive = SumMods(Mods[EGameplayModOp::Additive], GameplayEffectUtilities::GetModifierBiasByModifierOp(EGameplayModOp::Additive), Parameters);
	float Multiplicitive = SumMods(Mods[EGameplayModOp::Multiplicitive], GameplayEffectUtilities::GetModifierBiasByModifierOp(EGameplayModOp::Multiplicitive), Parameters);
	float Division = SumMods(Mods[EGameplayModOp::Division], GameplayEffectUtilities::GetModifierBiasByModifierOp(EGameplayModOp::Division), Parameters);

	if (FMath::IsNearlyZero(Division))
	{
		ABILITY_LOG(Warning, TEXT("Division summation was 0.0f in FAggregator."));
		Division = 1.f;
	}

	return ((InlineBaseValue + Additive) * Multiplicitive) / Division;
}

float FAggregator::ReverseEvaluate(float FinalValue, const FAggregatorEvaluateParameters& Parameters) const
{
	for (const FAggregatorMod& Mod : Mods[EGameplayModOp::Override])
	{
		if (Mod.Qualifies(Parameters))
		{
			// This is the case we can't really handle due to lack of information.
			return FinalValue;
		}
	}

	float Additive = SumMods(Mods[EGameplayModOp::Additive], GameplayEffectUtilities::GetModifierBiasByModifierOp(EGameplayModOp::Additive), Parameters);
	float Multiplicitive = SumMods(Mods[EGameplayModOp::Multiplicitive], GameplayEffectUtilities::GetModifierBiasByModifierOp(EGameplayModOp::Multiplicitive), Parameters);
	float Division = SumMods(Mods[EGameplayModOp::Division], GameplayEffectUtilities::GetModifierBiasByModifierOp(EGameplayModOp::Division), Parameters);

	if (FMath::IsNearlyZero(Division))
	{
		ABILITY_LOG(Warning, TEXT("Division summation was 0.0f in FAggregator."));
		Division = 1.f;
	}

	if (Multiplicitive <= SMALL_NUMBER)
	{
		return FinalValue;
	}

	float CalculatedBaseValue = (FinalValue * Division / Multiplicitive) - Additive;
	return CalculatedBaseValue;
}

float FAggregator::EvaluateBonus(const FAggregatorEvaluateParameters& Parameters) const
{
	return (Evaluate(Parameters) - GetBaseValue());
}

float FAggregator::EvaluateContribution(const FAggregatorEvaluateParameters& Parameters, FActiveGameplayEffectHandle ActiveHandle) const
{
	FAggregator Temp;
	Temp.BaseValue = GetBaseValue();

	if (ActiveHandle.IsValid())
	{
		for (int32 Idx = 0; Idx < ARRAY_COUNT(Mods); ++Idx)
		{
			Temp.Mods[Idx] = Mods[Idx];

			Temp.RemoveModsWithActiveHandle(Temp.Mods[Idx], ActiveHandle);
		}

		return Evaluate(Parameters) - Temp.Evaluate(Parameters);
	}

	return 0.f;
}

float FAggregator::GetBaseValue() const
{
	return BaseValue;
}

void FAggregator::SetBaseValue(float NewBaseValue, bool BroadcastDirtyEvent)
{
	BaseValue = NewBaseValue;
	if (BroadcastDirtyEvent)
	{
		BroadcastOnDirty();
	}
}

float FAggregator::StaticExecModOnBaseValue(float BaseValue, TEnumAsByte<EGameplayModOp::Type> ModifierOp, float EvaluatedMagnitude)
{
	switch (ModifierOp)
	{
		case EGameplayModOp::Override:
		{
			BaseValue = EvaluatedMagnitude;
			break;
		}
		case EGameplayModOp::Additive:
		{
			BaseValue += EvaluatedMagnitude;
			break;
		}
		case EGameplayModOp::Multiplicitive:
		{
			BaseValue *= EvaluatedMagnitude;
			break;
		}
		case EGameplayModOp::Division:
		{
			if (FMath::IsNearlyZero(EvaluatedMagnitude) == false)
			{
				BaseValue /= EvaluatedMagnitude;
			}
			break;
		}
	}

	return BaseValue;
}

void FAggregator::ExecModOnBaseValue(TEnumAsByte<EGameplayModOp::Type> ModifierOp, float EvaluatedMagnitude)
{
	BaseValue = StaticExecModOnBaseValue(BaseValue, ModifierOp, EvaluatedMagnitude);
	BroadcastOnDirty();
}

float FAggregator::SumMods(const TArray<FAggregatorMod> &InMods, float Bias, const FAggregatorEvaluateParameters& Parameters) const
{
	float Sum = Bias;

	for (const FAggregatorMod& Mod : InMods)
	{
		if (Mod.Qualifies(Parameters))
		{
			Sum += (Mod.EvaluatedMagnitude - Bias);
		}
	}

	return Sum;
}

void FAggregator::AddAggregatorMod(float EvaluatedMagnitude, TEnumAsByte<EGameplayModOp::Type> ModifierOp, const FGameplayTagRequirements* SourceTagReqs, const FGameplayTagRequirements* TargetTagReqs, bool IsPredicted, FActiveGameplayEffectHandle ActiveHandle)
{
	TArray<FAggregatorMod> &ModList = Mods[ModifierOp];

	int32 NewIdx = ModList.AddUninitialized();
	FAggregatorMod& NewMod = ModList[NewIdx];

	NewMod.SourceTagReqs = SourceTagReqs;
	NewMod.TargetTagReqs = TargetTagReqs;
	NewMod.EvaluatedMagnitude = EvaluatedMagnitude;
	NewMod.ActiveHandle = ActiveHandle;
	NewMod.IsPredicted = IsPredicted;

	if (IsPredicted)
	{
		NumPredictiveMods++;
	}

	BroadcastOnDirty();
}

void FAggregator::RemoveAggregatorMod(FActiveGameplayEffectHandle ActiveHandle)
{
	InternalRemoveAggregatorMod(ActiveHandle);

	// mark it as dirty so that all the stats get updated
	BroadcastOnDirty();
}

void FAggregator::UpdateAggregatorMod(FActiveGameplayEffectHandle ActiveHandle, const FGameplayAttribute& Attribute, const FGameplayEffectSpec& Spec, bool bWasLocalyGenerated, FActiveGameplayEffectHandle InHandle)
{
	// remove the mods but don't mark it as dirty until we re-add the aggregators, we are doing this so the UAttributeSets stats only know about the delta change.
	InternalRemoveAggregatorMod(ActiveHandle);

	// Now re-add ALL of our mods
	for (int32 ModIdx = 0; ModIdx < Spec.Modifiers.Num(); ++ModIdx)
	{
		const FGameplayModifierInfo& ModDef = Spec.Def->Modifiers[ModIdx];

		if (ModDef.Attribute == Attribute)
		{
			AddAggregatorMod(Spec.GetModifierMagnitude(ModIdx, true), ModDef.ModifierOp, &ModDef.SourceTags, &ModDef.TargetTags, bWasLocalyGenerated, InHandle);
		}
	}

	// mark it as dirty so that all the stats get updated
	BroadcastOnDirty();
}

// moved the remove into a common function since now we will need to remove w/o marking it as dirty so we can support updating the aggregators 
void FAggregator::InternalRemoveAggregatorMod(FActiveGameplayEffectHandle ActiveHandle)
{
	if (ActiveHandle.IsValid())
	{
		RemoveModsWithActiveHandle(Mods[EGameplayModOp::Additive], ActiveHandle);
		RemoveModsWithActiveHandle(Mods[EGameplayModOp::Multiplicitive], ActiveHandle);
		RemoveModsWithActiveHandle(Mods[EGameplayModOp::Division], ActiveHandle);
		RemoveModsWithActiveHandle(Mods[EGameplayModOp::Override], ActiveHandle);
	}
}

void FAggregator::AddModsFrom(const FAggregator& SourceAggregator)
{
	for(int32 idx = 0; idx < ARRAY_COUNT(Mods); ++idx)
	{
		Mods[idx].Append(SourceAggregator.Mods[idx]);
	}
}

void FAggregator::AddDependent(FActiveGameplayEffectHandle Handle)
{
	Dependents.Add(Handle);
}

void FAggregator::RemoveDependent(FActiveGameplayEffectHandle Handle)
{
	Dependents.Remove(Handle);
}

bool FAggregator::HasPredictedMods() const
{
	return NumPredictiveMods > 0;
}

void FAggregator::RemoveModsWithActiveHandle(TArray<FAggregatorMod>& InMods, FActiveGameplayEffectHandle ActiveHandle)
{
	check(ActiveHandle.IsValid());

	for(int32 idx=InMods.Num()-1; idx >= 0; --idx)
	{
		if (InMods[idx].ActiveHandle == ActiveHandle)
		{
			if (InMods[idx].IsPredicted)
			{
				NumPredictiveMods--;
			}

			InMods.RemoveAtSwap(idx, 1, false);
		}
	}
}

void FAggregator::TakeSnapshotOf(const FAggregator& AggToSnapshot)
{
	BaseValue = AggToSnapshot.BaseValue;
	for (int32 idx=0; idx < ARRAY_COUNT(Mods); ++idx)
	{
		Mods[idx] = AggToSnapshot.Mods[idx];
	}
}

void FAggregator::BroadcastOnDirty()
{
	// If we are batching on Dirty calls (and we actually have dependents registered with us) then early out.
	if (FScopedAggregatorOnDirtyBatch::GlobalBatchCount > 0 && (Dependents.Num() > 0 || OnDirty.IsBound()))
	{
		FScopedAggregatorOnDirtyBatch::DirtyAggregators.Add(this);
		return;
	}

	if (IsBroadcastingDirty)
	{
		// Apologies for the vague warning but its very hard from this spot to call out what data has caused this. If this frequently happens we should improve this.
		ABILITY_LOG(Warning, TEXT("FAggregator detected cyclic attribute dependencies. We are skipping a recursive dirty call. Its possible the resulting attribute values are not what you expect!"));

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Additional, slow, debugging that will print all aggregator/attributes that are currently dirty
		for (TObjectIterator<UAbilitySystemComponent> It; It; ++It)
		{
			It->DebugCyclicAggregatorBroadcasts(this);
		}
#endif
		return;
	}

	TGuardValue<bool>	Guard(IsBroadcastingDirty, true);
	
	OnDirty.Broadcast(this);


	static TArray<FActiveGameplayEffectHandle>	ValidDependents;
	ValidDependents.Reset();

	for (FActiveGameplayEffectHandle Handle : Dependents)
	{
		UAbilitySystemComponent* ASC = Handle.GetOwningAbilitySystemComponent();
		if (ASC)
		{
			ASC->OnMagnitudeDependencyChange(Handle, this);
			ValidDependents.Add(Handle);
		}
	}
	Dependents = ValidDependents;

}

void FAggregatorRef::TakeSnapshotOf(const FAggregatorRef& RefToSnapshot)
{
	if (RefToSnapshot.Data.IsValid())
	{
		FAggregator* SrcData = RefToSnapshot.Data.Get();

		Data = TSharedPtr<FAggregator>(new FAggregator());
		Data->TakeSnapshotOf(*SrcData);
	}
	else
	{
		Data.Reset();
	}
}

int32 FScopedAggregatorOnDirtyBatch::GlobalBatchCount = 0;
TSet<FAggregator*> FScopedAggregatorOnDirtyBatch::DirtyAggregators;
bool FScopedAggregatorOnDirtyBatch::GlobalFromNetworkUpdate = false;
int32 FScopedAggregatorOnDirtyBatch::NetUpdateID = 1;

FScopedAggregatorOnDirtyBatch::FScopedAggregatorOnDirtyBatch()
{
	BeginLock();
}

FScopedAggregatorOnDirtyBatch::~FScopedAggregatorOnDirtyBatch()
{
	EndLock();
}

void FScopedAggregatorOnDirtyBatch::BeginLock()
{
	GlobalBatchCount++;
}
void FScopedAggregatorOnDirtyBatch::EndLock()
{
	GlobalBatchCount--;
	if (GlobalBatchCount == 0)
	{
		TSet<FAggregator*> LocalSet(MoveTemp(DirtyAggregators));
		for (FAggregator* Agg : LocalSet)
		{
			Agg->BroadcastOnDirty();
		}
		LocalSet.Empty();
	}
}


void FScopedAggregatorOnDirtyBatch::BeginNetReceiveLock()
{
	BeginLock();
}
void FScopedAggregatorOnDirtyBatch::EndNetReceiveLock()
{
	// The network lock must end the first time it is called.
	// Subsequent calls to EndNetReceiveLock() should not trigger a full EndLock, only the first one.
	if (GlobalBatchCount > 0)
	{
		GlobalBatchCount = 1;
		NetUpdateID++;
		GlobalFromNetworkUpdate = true;
		EndLock();
		GlobalFromNetworkUpdate = false;
	}
}
