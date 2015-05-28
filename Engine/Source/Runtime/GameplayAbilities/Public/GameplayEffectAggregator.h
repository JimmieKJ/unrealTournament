// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffectTypes.h"

struct GAMEPLAYABILITIES_API FAggregatorEvaluateParameters
{
	FAggregatorEvaluateParameters() : SourceTags(nullptr), TargetTags(nullptr), IncludePredictiveMods(false) { }

	const FGameplayTagContainer*	SourceTags;
	const FGameplayTagContainer*	TargetTags;

	/** If any tags are specified in the filter, a mod's owning active gameplay effect's source tags must match ALL of them in order for the mod to count during evaluation */
	FGameplayTagContainer	AppliedSourceTagFilter;

	/** If any tags are specified in the filter, a mod's owning active gameplay effect's target tags must match ALL of them in order for the mod to count during evaluation */
	FGameplayTagContainer	AppliedTargetTagFilter;

	bool IncludePredictiveMods;
};

struct GAMEPLAYABILITIES_API FAggregatorMod
{
	const FGameplayTagRequirements*	SourceTagReqs;
	const FGameplayTagRequirements*	TargetTagReqs;

	float EvaluatedMagnitude;		// Magnitude this mod was last evaluated at
	float StackCount;

	FActiveGameplayEffectHandle ActiveHandle;	// Handle of the active GameplayEffect we are tied to (if any)
	bool IsPredicted;

	bool Qualifies(const FAggregatorEvaluateParameters& Parameters) const;
};


struct GAMEPLAYABILITIES_API FAggregator : public TSharedFromThis<FAggregator>
{
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAggregatorDirty, FAggregator*);

	FAggregator(float InBaseValue=0.f) : NetUpdateID(0), BaseValue(InBaseValue), IsBroadcastingDirty(false), NumPredictiveMods(0) { }

	/** Simple accessor to base value */
	float GetBaseValue() const;
	void SetBaseValue(float NewBaseValue, bool BroadcastDirtyEvent = true);
	
	void ExecModOnBaseValue(TEnumAsByte<EGameplayModOp::Type> ModifierOp, float EvaluatedMagnitude);
	static float StaticExecModOnBaseValue(float BaseValue, TEnumAsByte<EGameplayModOp::Type> ModifierOp, float EvaluatedMagnitude);

	void AddAggregatorMod(float EvaluatedData, TEnumAsByte<EGameplayModOp::Type> ModifierOp, const FGameplayTagRequirements*	SourceTagReqs, const FGameplayTagRequirements* TargetTagReqs, bool IsPredicted, FActiveGameplayEffectHandle ActiveHandle = FActiveGameplayEffectHandle() );
	void RemoveAggregatorMod(FActiveGameplayEffectHandle ActiveHandle);

	/** Evaluates the Aggregator with the internal base value and given parameters */
	float Evaluate(const FAggregatorEvaluateParameters& Parameters) const;

	/** Works backwards to calculate the base value. Used on clients for doing predictive modifiers */
	float ReverseEvaluate(float FinalValue, const FAggregatorEvaluateParameters& Parameters) const;

	/** Evaluates the Aggregator with an arbitrary base value */
	float EvaluateWithBase(float InlineBaseValue, const FAggregatorEvaluateParameters& Parameters) const;

	/** Evaluates the Aggregator to compute its "bonus" (final - base) value */
	float EvaluateBonus(const FAggregatorEvaluateParameters& Parameters) const;

	void TakeSnapshotOf(const FAggregator& AggToSnapshot);

	FOnAggregatorDirty OnDirty;

	void AddModsFrom(const FAggregator& SourceAggregator);

	void AddDependent(FActiveGameplayEffectHandle Handle);
	void RemoveDependent(FActiveGameplayEffectHandle Handle);

	bool HasPredictedMods() const;
	
	/** NetworkID that we had our last update from. Will only be set on clients and is only used to ensure we dont recompute server base value twice. */
	int32 NetUpdateID;

private:

	void BroadcastOnDirty();
	float SumMods(const TArray<FAggregatorMod> &Mods, float Bias, const FAggregatorEvaluateParameters& Parameters) const;
	void RemoveModsWithActiveHandle(TArray<FAggregatorMod>& Mods, FActiveGameplayEffectHandle ActiveHandle);

	float	BaseValue;
	TArray<FAggregatorMod>	Mods[EGameplayModOp::Max];

	/** ActiveGE handles that we need to notify if we change. NOT copied over during snapshots. */
	TArray<FActiveGameplayEffectHandle>	Dependents;
	bool	IsBroadcastingDirty;
	int32	NumPredictiveMods;

	friend struct FAggregator;
	friend struct FScopedAggregatorOnDirtyBatch;	// Only outside class that gets to call BroadcastOnDirty()
	friend class UAbilitySystemComponent;	// Only needed for DisplayDebug()
};

struct GAMEPLAYABILITIES_API FAggregatorRef
{
	FAggregatorRef() { }
	FAggregatorRef(FAggregator* InData) : Data(InData) { }

	FAggregator* Get() const { return Data.Get(); }

	TSharedPtr<FAggregator>	Data;

	void TakeSnapshotOf(const FAggregatorRef& RefToSnapshot);
};

/**
 *	Allows us to batch all aggregator OnDirty calls within a scope. That is, ALL OnDirty() callbacks are
 *	delayed until FScopedAggregatorOnDirtyBatch goes out of scope.
 *	
 *	The only catch is that we store raw FAggregator*. This should only be used in scopes where aggreagtors
 *	are not deleted. There is currently no place that does. If we find to, we could add additional safety checks.
 */
struct GAMEPLAYABILITIES_API FScopedAggregatorOnDirtyBatch
{
	FScopedAggregatorOnDirtyBatch();
	~FScopedAggregatorOnDirtyBatch();

	static void BeginLock();
	static void EndLock();

	static void BeginNetReceiveLock();
	static void EndNetReceiveLock();

	static int32	GlobalBatchCount;
	static TSet<FAggregator*>	DirtyAggregators;

	static bool		GlobalFromNetworkUpdate;
	static int32	NetUpdateID;
};