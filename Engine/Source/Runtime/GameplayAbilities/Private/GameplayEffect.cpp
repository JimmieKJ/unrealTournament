// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagsModule.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayModMagnitudeCalculation.h"
#include "GameplayEffectExecutionCalculation.h"
#include "GameplayCueManager.h"
#if ENABLE_VISUAL_LOG
#include "VisualLoggerTypes.h"
//#include "VisualLogger/VisualLogger.h"
#endif // ENABLE_VISUAL_LOG

const float UGameplayEffect::INFINITE_DURATION = -1.f;
const float UGameplayEffect::INSTANT_APPLICATION = 0.f;
const float UGameplayEffect::NO_PERIOD = 0.f;
const float UGameplayEffect::INVALID_LEVEL = -1.f;

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	UGameplayEffect
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------


UGameplayEffect::UGameplayEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	bExecutePeriodicEffectOnApplication = true;
	ChanceToApplyToTarget.SetValue(1.f);
	StackingType = EGameplayEffectStackingType::None;
	StackLimitCount = 0;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	StackPeriodResetPolicy = EGameplayEffectStackingPeriodPolicy::ResetOnSuccessfulApplication;
	bRequireModifierSuccessToTriggerCues = true;

#if WITH_EDITORONLY_DATA
	ShowAllProperties = true;
	Template = nullptr;
#endif

}

void UGameplayEffect::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	TagContainer.AppendTags(InheritableOwnedTagsContainer.CombinedTags);
}

void UGameplayEffect::GetTargetEffects(TArray<const UGameplayEffect*>& OutEffects) const
{
	OutEffects.Append(TargetEffects);

	for ( TSubclassOf<UGameplayEffect> EffectClass : TargetEffectClasses )
	{
		if ( EffectClass )
		{
			OutEffects.Add(EffectClass->GetDefaultObject<UGameplayEffect>());
		}
	}
}

void UGameplayEffect::PostLoad()
{
	Super::PostLoad();

	// Temporary post-load fix-up to preserve magnitude data
	for (FGameplayModifierInfo& CurModInfo : Modifiers)
	{
		// If the old magnitude actually had some value in it, copy it over and then clear out the old data
		if (CurModInfo.Magnitude.Value != 0.f || CurModInfo.Magnitude.Curve.IsValid())
		{
			CurModInfo.ModifierMagnitude.ScalableFloatMagnitude = CurModInfo.Magnitude;
			CurModInfo.Magnitude = FScalableFloat();
		}
	}

	// We need to update when we first load to override values coming in from the superclass
	// We also copy the tags from the old tag containers into the inheritable tag containers

	InheritableGameplayEffectTags.Added.AppendTags(GameplayEffectTags);
	GameplayEffectTags.RemoveAllTags();

	InheritableOwnedTagsContainer.Added.AppendTags(OwnedTagsContainer);
	OwnedTagsContainer.RemoveAllTags();

	RemoveGameplayEffectsWithTags.Added.AppendTags(ClearTagsContainer);
	ClearTagsContainer.RemoveAllTags();

	UpdateInheritedTagProperties();
}

void UGameplayEffect::PostInitProperties()
{
	Super::PostInitProperties();

	InheritableGameplayEffectTags.PostInitProperties();
	InheritableOwnedTagsContainer.PostInitProperties();
	RemoveGameplayEffectsWithTags.PostInitProperties();
}

#if WITH_EDITOR

void UGameplayEffect::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const UProperty* PropertyThatChanged = PropertyChangedEvent.MemberProperty;
	if (PropertyThatChanged)
	{
		UGameplayEffect* Parent = Cast<UGameplayEffect>(GetClass()->GetSuperClass()->GetDefaultObject());
		FName PropName = PropertyThatChanged->GetFName();
		if (PropName == GET_MEMBER_NAME_CHECKED(UGameplayEffect, InheritableGameplayEffectTags))
		{
			InheritableGameplayEffectTags.UpdateInheritedTagProperties(Parent ? &Parent->InheritableGameplayEffectTags : NULL);
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(UGameplayEffect, InheritableOwnedTagsContainer))
		{
			InheritableOwnedTagsContainer.UpdateInheritedTagProperties(Parent ? &Parent->InheritableOwnedTagsContainer : NULL);
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(UGameplayEffect, RemoveGameplayEffectsWithTags))
		{
			RemoveGameplayEffectsWithTags.UpdateInheritedTagProperties(Parent ? &Parent->RemoveGameplayEffectsWithTags : NULL);
		}
	}
}

#endif // #if WITH_EDITOR

void UGameplayEffect::UpdateInheritedTagProperties()
{
	UGameplayEffect* Parent = Cast<UGameplayEffect>(GetClass()->GetSuperClass()->GetDefaultObject());

	InheritableGameplayEffectTags.UpdateInheritedTagProperties(Parent ? &Parent->InheritableGameplayEffectTags : NULL);
	InheritableOwnedTagsContainer.UpdateInheritedTagProperties(Parent ? &Parent->InheritableOwnedTagsContainer : NULL);
	RemoveGameplayEffectsWithTags.UpdateInheritedTagProperties(Parent ? &Parent->RemoveGameplayEffectsWithTags : NULL);
}

void UGameplayEffect::ValidateGameplayEffect()
{
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FAttributeBasedFloat
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

float FAttributeBasedFloat::CalculateMagnitude(const FGameplayEffectSpec& InRelevantSpec) const
{
	const FGameplayEffectAttributeCaptureSpec* CaptureSpec = InRelevantSpec.CapturedRelevantAttributes.FindCaptureSpecByDefinition(BackingAttribute, true);
	checkf(CaptureSpec, TEXT("Attempted to calculate an attribute-based float from spec: %s that did not have the required captured attribute: %s"), 
		*InRelevantSpec.ToSimpleString(), *BackingAttribute.ToSimpleString());

	float AttribValue = 0.f;

	// Base value can be calculated w/o evaluation parameters
	if (AttributeCalculationType == EAttributeBasedFloatCalculationType::AttributeBaseValue)
	{
		CaptureSpec->AttemptCalculateAttributeBaseValue(AttribValue);
	}
	// Set up eval params to handle magnitude or bonus magnitude calculations
	else
	{
		FAggregatorEvaluateParameters EvaluationParameters;
		EvaluationParameters.SourceTags = InRelevantSpec.CapturedSourceTags.GetAggregatedTags();
		EvaluationParameters.TargetTags = InRelevantSpec.CapturedTargetTags.GetAggregatedTags();
		EvaluationParameters.AppliedSourceTagFilter = SourceTagFilter;
		EvaluationParameters.AppliedTargetTagFilter = TargetTagFilter;

		if (AttributeCalculationType == EAttributeBasedFloatCalculationType::AttributeMagnitude)
		{
			CaptureSpec->AttemptCalculateAttributeMagnitude(EvaluationParameters, AttribValue);
		}
		else
		{
			CaptureSpec->AttemptCalculateAttributeBonusMagnitude(EvaluationParameters, AttribValue);
		}
	}

	const float SpecLvl = InRelevantSpec.GetLevel();
	return ((Coefficient.GetValueAtLevel(SpecLvl) * (AttribValue + PreMultiplyAdditiveValue.GetValueAtLevel(SpecLvl))) + PostMultiplyAdditiveValue.GetValueAtLevel(SpecLvl));
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FCustomCalculationBasedFloat
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

float FCustomCalculationBasedFloat::CalculateMagnitude(const FGameplayEffectSpec& InRelevantSpec) const
{
	const UGameplayModMagnitudeCalculation* CalcCDO = CalculationClassMagnitude->GetDefaultObject<UGameplayModMagnitudeCalculation>();
	check(CalcCDO);

	float CustomBaseValue = CalcCDO->CalculateBaseMagnitude(InRelevantSpec);

	const float SpecLvl = InRelevantSpec.GetLevel();
	return ((Coefficient.GetValueAtLevel(SpecLvl) * (CustomBaseValue + PreMultiplyAdditiveValue.GetValueAtLevel(SpecLvl))) + PostMultiplyAdditiveValue.GetValueAtLevel(SpecLvl));
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FGameplayEffectMagnitude
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

bool FGameplayEffectModifierMagnitude::CanCalculateMagnitude(const FGameplayEffectSpec& InRelevantSpec) const
{
	// Only can calculate magnitude properly if all required capture definitions are fulfilled by the spec
	TArray<FGameplayEffectAttributeCaptureDefinition> ReqCaptureDefs;
	GetAttributeCaptureDefinitions(ReqCaptureDefs);

	return InRelevantSpec.HasValidCapturedAttributes(ReqCaptureDefs);
}

bool FGameplayEffectModifierMagnitude::AttemptCalculateMagnitude(const FGameplayEffectSpec& InRelevantSpec, OUT float& OutCalculatedMagnitude) const
{
	const bool bCanCalc = CanCalculateMagnitude(InRelevantSpec);
	if (bCanCalc)
	{
		switch (MagnitudeCalculationType)
		{
			case EGameplayEffectMagnitudeCalculation::ScalableFloat:
			{
				OutCalculatedMagnitude = ScalableFloatMagnitude.GetValueAtLevel(InRelevantSpec.GetLevel());
			}
			break;

			case EGameplayEffectMagnitudeCalculation::AttributeBased:
			{
				OutCalculatedMagnitude = AttributeBasedMagnitude.CalculateMagnitude(InRelevantSpec);
			}
			break;

			case EGameplayEffectMagnitudeCalculation::CustomCalculationClass:
			{
				OutCalculatedMagnitude = CustomMagnitude.CalculateMagnitude(InRelevantSpec);
			}
			break;

			case EGameplayEffectMagnitudeCalculation::SetByCaller:
				OutCalculatedMagnitude = InRelevantSpec.GetSetByCallerMagnitude(SetByCallerMagnitude.DataName);
				break;
			default:
				ABILITY_LOG(Error, TEXT("Unknown MagnitudeCalculationType %d in AttemptCalculateMagnitude"), (int32)MagnitudeCalculationType);
				OutCalculatedMagnitude = 0.f;
				break;
		}
	}
	else
	{
		OutCalculatedMagnitude = 0.f;
	}

	return bCanCalc;
}

bool FGameplayEffectModifierMagnitude::AttemptRecalculateMagnitudeFromDependentChange(const FGameplayEffectSpec& InRelevantSpec, OUT float& OutCalculatedMagnitude, const FAggregator* ChangedAggregator) const
{
	TArray<FGameplayEffectAttributeCaptureDefinition> ReqCaptureDefs;
	GetAttributeCaptureDefinitions(ReqCaptureDefs);

	// We could have many potential captures. If a single one matches our criteria, then we call AttemptCalculateMagnitude once and return.
	for (const FGameplayEffectAttributeCaptureDefinition& CaptureDef : ReqCaptureDefs)
	{
		if (CaptureDef.bSnapshot == false)
		{
			const FGameplayEffectAttributeCaptureSpec* CapturedSpec = InRelevantSpec.CapturedRelevantAttributes.FindCaptureSpecByDefinition(CaptureDef, true);
			if (CapturedSpec && CapturedSpec->ShouldRefreshLinkedAggregator(ChangedAggregator))
			{
				return AttemptCalculateMagnitude(InRelevantSpec, OutCalculatedMagnitude);
			}
		}
	}

	return false;
}

void FGameplayEffectModifierMagnitude::GetAttributeCaptureDefinitions(OUT TArray<FGameplayEffectAttributeCaptureDefinition>& OutCaptureDefs) const
{
	OutCaptureDefs.Empty();

	switch (MagnitudeCalculationType)
	{
		case EGameplayEffectMagnitudeCalculation::AttributeBased:
		{
			OutCaptureDefs.Add(AttributeBasedMagnitude.BackingAttribute);
		}
		break;

		case EGameplayEffectMagnitudeCalculation::CustomCalculationClass:
		{
			if (CustomMagnitude.CalculationClassMagnitude)
			{
				const UGameplayModMagnitudeCalculation* CalcCDO = CustomMagnitude.CalculationClassMagnitude->GetDefaultObject<UGameplayModMagnitudeCalculation>();
				check(CalcCDO);

				OutCaptureDefs.Append(CalcCDO->GetAttributeCaptureDefinitions());
			}
		}
		break;
	}
}

bool FGameplayEffectModifierMagnitude::GetStaticMagnitudeIfPossible(float InLevel, float& OutMagnitude) const
{
	if (MagnitudeCalculationType == EGameplayEffectMagnitudeCalculation::ScalableFloat)
	{
		OutMagnitude = ScalableFloatMagnitude.GetValueAtLevel(InLevel);
		return true;
	}

	return false;
}

bool FGameplayEffectModifierMagnitude::GetSetByCallerDataNameIfPossible(FName& OutDataName) const
{
	if (MagnitudeCalculationType == EGameplayEffectMagnitudeCalculation::SetByCaller)
	{
		OutDataName = SetByCallerMagnitude.DataName;
		return true;
	}

	return false;
}

#if WITH_EDITOR
FText FGameplayEffectModifierMagnitude::GetValueForEditorDisplay() const
{
	switch (MagnitudeCalculationType)
	{
		case EGameplayEffectMagnitudeCalculation::ScalableFloat:
			return FText::Format(NSLOCTEXT("GameplayEffect", "ScalableFloatModifierMagnitude", "{0} s"), FText::AsNumber(ScalableFloatMagnitude.Value));
			
		case EGameplayEffectMagnitudeCalculation::AttributeBased:
			return NSLOCTEXT("GameplayEffect", "AttributeBasedModifierMagnitude", "Attribute Based");

		case EGameplayEffectMagnitudeCalculation::CustomCalculationClass:
			return NSLOCTEXT("GameplayEffect", "CustomCalculationClassModifierMagnitude", "Custom Calculation");

		case EGameplayEffectMagnitudeCalculation::SetByCaller:
			return NSLOCTEXT("GameplayEffect", "SetByCallerModifierMagnitude", "Set by Caller");
	}

	return NSLOCTEXT("GameplayEffect", "UnknownModifierMagnitude", "Unknown");
}
#endif // WITH_EDITOR


// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FGameplayEffectExecutionDefinition
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

void FGameplayEffectExecutionDefinition::GetAttributeCaptureDefinitions(OUT TArray<FGameplayEffectAttributeCaptureDefinition>& OutCaptureDefs) const
{
	OutCaptureDefs.Empty();

	if (CalculationClass)
	{
		const UGameplayEffectExecutionCalculation* CalculationCDO = Cast<UGameplayEffectExecutionCalculation>(CalculationClass->ClassDefaultObject);
		check(CalculationCDO);

		OutCaptureDefs.Append(CalculationCDO->GetAttributeCaptureDefinitions());
	}

	// Scoped modifiers might have custom magnitude calculations, requiring additional captured attributes
	for (const FGameplayEffectExecutionScopedModifierInfo& CurScopedMod : CalculationModifiers)
	{
		TArray<FGameplayEffectAttributeCaptureDefinition> ScopedModMagDefs;
		CurScopedMod.ModifierMagnitude.GetAttributeCaptureDefinitions(ScopedModMagDefs);

		OutCaptureDefs.Append(ScopedModMagDefs);
	}
}


// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FGameplayEffectSpec
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

FGameplayEffectSpec::FGameplayEffectSpec()
	: Def(nullptr)
	, Duration(UGameplayEffect::INSTANT_APPLICATION)
	, Period(UGameplayEffect::NO_PERIOD)
	, ChanceToApplyToTarget(1.f)
	, StackCount(1)
	, bCompletedSourceAttributeCapture(false)
	, bCompletedTargetAttributeCapture(false)
	, bDurationLocked(false)
	, Level(UGameplayEffect::INVALID_LEVEL)
{

}

FGameplayEffectSpec::FGameplayEffectSpec(const UGameplayEffect* InDef, const FGameplayEffectContextHandle& InEffectContext, float InLevel)
	: Def(InDef)
	, StackCount(1)
	, bCompletedSourceAttributeCapture(false)
	, bCompletedTargetAttributeCapture(false)
	, bDurationLocked(false)
{
	check(Def);	
	SetLevel(InLevel);
	SetContext(InEffectContext);

	// Init our ModifierSpecs
	Modifiers.SetNum(Def->Modifiers.Num());

	// Prep the spec with all of the attribute captures it will need to perform
	SetupAttributeCaptureDefinitions();
	
	// Add the GameplayEffect asset tags to the source Spec tags
	CapturedSourceTags.GetSpecTags().AppendTags(InDef->InheritableGameplayEffectTags.CombinedTags);

	// Make TargetEffectSpecs too
	TArray<const UGameplayEffect*> TargetEffectDefs;
	InDef->GetTargetEffects(TargetEffectDefs);

	for (const UGameplayEffect* TargetDef : TargetEffectDefs)
	{
		TargetEffectSpecs.Add(FGameplayEffectSpecHandle(new FGameplayEffectSpec(TargetDef, EffectContext, InLevel)));
	}

	// Make Granted AbilitySpecs (caller may modify these specs after creating spec, which is why we dont just reference them from the def)
	GrantedAbilitySpecs = InDef->GrantedAbilities;

	// Everything is setup now, capture data from our source
	CaptureDataFromSource();
}

FGameplayEffectSpec::FGameplayEffectSpec(const FGameplayEffectSpec& Other)
{
	*this = Other;
}

FGameplayEffectSpec::FGameplayEffectSpec(FGameplayEffectSpec&& Other)
	: Def(Other.Def)
	, ModifiedAttributes(MoveTemp(Other.ModifiedAttributes))
	, CapturedRelevantAttributes(MoveTemp(Other.CapturedRelevantAttributes))
	, TargetEffectSpecs(MoveTemp(Other.TargetEffectSpecs))
	, Duration(Other.Duration)
	, Period(Other.Period)
	, ChanceToApplyToTarget(Other.ChanceToApplyToTarget)
	, CapturedSourceTags(MoveTemp(Other.CapturedSourceTags))
	, CapturedTargetTags(MoveTemp(Other.CapturedTargetTags))
	, DynamicGrantedTags(MoveTemp(Other.DynamicGrantedTags))
	, Modifiers(MoveTemp(Other.Modifiers))
	, StackCount(Other.StackCount)
	, bCompletedSourceAttributeCapture(Other.bCompletedSourceAttributeCapture)
	, bCompletedTargetAttributeCapture(Other.bCompletedTargetAttributeCapture)
	, bDurationLocked(Other.bDurationLocked)
	, GrantedAbilitySpecs(MoveTemp(Other.GrantedAbilitySpecs))
	, SetByCallerMagnitudes(MoveTemp(Other.SetByCallerMagnitudes))
	, EffectContext(Other.EffectContext)
	, Level(Other.Level)
{

}

FGameplayEffectSpec& FGameplayEffectSpec::operator=(FGameplayEffectSpec&& Other)
{
	Def = Other.Def;
	ModifiedAttributes = MoveTemp(Other.ModifiedAttributes);
	CapturedRelevantAttributes = MoveTemp(Other.CapturedRelevantAttributes);
	TargetEffectSpecs = MoveTemp(Other.TargetEffectSpecs);
	Duration = Other.Duration;
	Period = Other.Period;
	ChanceToApplyToTarget = Other.ChanceToApplyToTarget;
	CapturedSourceTags = MoveTemp(Other.CapturedSourceTags);
	CapturedTargetTags = MoveTemp(Other.CapturedTargetTags);
	DynamicGrantedTags = MoveTemp(Other.DynamicGrantedTags);
	Modifiers = MoveTemp(Other.Modifiers);
	StackCount = Other.StackCount;
	bCompletedSourceAttributeCapture = Other.bCompletedSourceAttributeCapture;
	bCompletedTargetAttributeCapture = Other.bCompletedTargetAttributeCapture;
	bDurationLocked = Other.bDurationLocked;
	GrantedAbilitySpecs = MoveTemp(Other.GrantedAbilitySpecs);
	SetByCallerMagnitudes = MoveTemp(Other.SetByCallerMagnitudes);
	EffectContext = Other.EffectContext;
	Level = Other.Level;
	return *this;
}

FGameplayEffectSpec& FGameplayEffectSpec::operator=(const FGameplayEffectSpec& Other)
{
	Def = Other.Def;
	ModifiedAttributes = Other.ModifiedAttributes;
	CapturedRelevantAttributes = Other.CapturedRelevantAttributes;
	TargetEffectSpecs = Other.TargetEffectSpecs;
	Duration = Other.Duration;
	Period = Other.Period;
	ChanceToApplyToTarget = Other.ChanceToApplyToTarget;
	CapturedSourceTags = Other.CapturedSourceTags;
	CapturedTargetTags = Other.CapturedTargetTags;
	DynamicGrantedTags = Other.DynamicGrantedTags;
	Modifiers = Other.Modifiers;
	StackCount = Other.StackCount;
	bCompletedSourceAttributeCapture = Other.bCompletedSourceAttributeCapture;
	bCompletedTargetAttributeCapture = Other.bCompletedTargetAttributeCapture;
	bDurationLocked = Other.bDurationLocked;
	GrantedAbilitySpecs =  Other.GrantedAbilitySpecs;
	SetByCallerMagnitudes = Other.SetByCallerMagnitudes;
	EffectContext = Other.EffectContext;
	Level = Other.Level;
	return *this;
}

FGameplayEffectSpecForRPC::FGameplayEffectSpecForRPC()
	: Def(nullptr)
	, Level(UGameplayEffect::INVALID_LEVEL)
{
}

FGameplayEffectSpecForRPC::FGameplayEffectSpecForRPC(const FGameplayEffectSpec& InSpec)
	: Def(InSpec.Def)
	, ModifiedAttributes(MoveTemp(InSpec.ModifiedAttributes))
	, EffectContext(InSpec.GetEffectContext())
	, AggregatedSourceTags(*InSpec.CapturedSourceTags.GetAggregatedTags())
	, AggregatedTargetTags(*InSpec.CapturedTargetTags.GetAggregatedTags())
	, Level(InSpec.GetLevel())
{
}

void FGameplayEffectSpec::SetupAttributeCaptureDefinitions()
{
	// Add duration if required
	if (Def->DurationPolicy == EGameplayEffectDurationType::HasDuration)
	{
		CapturedRelevantAttributes.AddCaptureDefinition(UAbilitySystemComponent::GetOutgoingDurationCapture());
		CapturedRelevantAttributes.AddCaptureDefinition(UAbilitySystemComponent::GetIncomingDurationCapture());
	}

	// Gather capture definitions from duration
	{
		TArray<FGameplayEffectAttributeCaptureDefinition> DurationCaptureDefs;
		Def->DurationMagnitude.GetAttributeCaptureDefinitions(DurationCaptureDefs);
		for (const FGameplayEffectAttributeCaptureDefinition& CurDurationCaptureDef : DurationCaptureDefs)
		{
			CapturedRelevantAttributes.AddCaptureDefinition(CurDurationCaptureDef);
		}
	}

	// Gather all capture definitions from modifiers
	for (int32 ModIdx = 0; ModIdx < Modifiers.Num(); ++ModIdx)
	{
		const FGameplayModifierInfo& ModDef = Def->Modifiers[ModIdx];
		const FModifierSpec& ModSpec = Modifiers[ModIdx];

		TArray<FGameplayEffectAttributeCaptureDefinition> CalculationCaptureDefs;
		ModDef.ModifierMagnitude.GetAttributeCaptureDefinitions(CalculationCaptureDefs);
		
		for (const FGameplayEffectAttributeCaptureDefinition& CurCaptureDef : CalculationCaptureDefs)
		{
			CapturedRelevantAttributes.AddCaptureDefinition(CurCaptureDef);
		}
	}

	// Gather all capture definitions from executions
	for (const FGameplayEffectExecutionDefinition& Exec : Def->Executions)
	{
		TArray<FGameplayEffectAttributeCaptureDefinition> ExecutionCaptureDefs;
		Exec.GetAttributeCaptureDefinitions(ExecutionCaptureDefs);

		for (const FGameplayEffectAttributeCaptureDefinition& CurExecCaptureDef : ExecutionCaptureDefs)
		{
			CapturedRelevantAttributes.AddCaptureDefinition(CurExecCaptureDef);
		}
	}
}

void FGameplayEffectSpec::CaptureAttributeDataFromTarget(UAbilitySystemComponent* TargetAbilitySystemComponent)
{
	CapturedRelevantAttributes.CaptureAttributes(TargetAbilitySystemComponent, EGameplayEffectAttributeCaptureSource::Target);
	bCompletedTargetAttributeCapture = true;
}

void FGameplayEffectSpec::CaptureDataFromSource()
{
	// Capture source actor tags
	CapturedSourceTags.GetActorTags().RemoveAllTags();
	EffectContext.GetOwnedGameplayTags(CapturedSourceTags.GetActorTags(), CapturedSourceTags.GetSpecTags());

	// Capture source Attributes
	// Is this the right place to do it? Do we ever need to create spec and capture attributes at a later time? If so, this will need to move.
	CapturedRelevantAttributes.CaptureAttributes(EffectContext.GetInstigatorAbilitySystemComponent(), EGameplayEffectAttributeCaptureSource::Source);

	// Now that we have source attributes captured, re-evaluate the duration since it could be based on the captured attributes.
	float DefCalcDuration = 0.f;
	if (AttemptCalculateDurationFromDef(DefCalcDuration))
	{
		SetDuration(DefCalcDuration, false);
	}

	bCompletedSourceAttributeCapture = true;
}

bool FGameplayEffectSpec::AttemptCalculateDurationFromDef(OUT float& OutDefDuration) const
{
	check(Def);

	bool bCalculatedDuration = true;

	const EGameplayEffectDurationType DurType = Def->DurationPolicy;
	if (DurType == EGameplayEffectDurationType::Infinite)
	{
		OutDefDuration = UGameplayEffect::INFINITE_DURATION;
	}
	else if (DurType == EGameplayEffectDurationType::Instant)
	{
		OutDefDuration = UGameplayEffect::INSTANT_APPLICATION;
	}
	else
	{
		bCalculatedDuration = Def->DurationMagnitude.AttemptCalculateMagnitude(*this, OutDefDuration);
	}

	return bCalculatedDuration;
}

void FGameplayEffectSpec::SetLevel(float InLevel)
{
	Level = InLevel;
	if (Def)
	{
		float DefCalcDuration = 0.f;
		if (AttemptCalculateDurationFromDef(DefCalcDuration))
		{
			SetDuration(DefCalcDuration, false);
		}
		
		Period = Def->Period.GetValueAtLevel(InLevel);
		ChanceToApplyToTarget = Def->ChanceToApplyToTarget.GetValueAtLevel(InLevel);
	}
}

float FGameplayEffectSpec::GetLevel() const
{
	return Level;
}

float FGameplayEffectSpec::GetDuration() const
{
	return Duration;
}

void FGameplayEffectSpec::SetDuration(float NewDuration, bool bLockDuration)
{
	if (!bDurationLocked)
	{
		Duration = NewDuration;
		bDurationLocked = bLockDuration;
		if (Duration > 0.f)
		{
			// We may have potential problems one day if a game is applying duration based gameplay effects from instantaneous effects
			// (E.g., every time fire damage is applied, a DOT is also applied). We may need to for Duration to always be captured.
			CapturedRelevantAttributes.AddCaptureDefinition(UAbilitySystemComponent::GetOutgoingDurationCapture());
		}
	}
}

float FGameplayEffectSpec::GetPeriod() const
{
	return Period;
}

float FGameplayEffectSpec::GetChanceToApplyToTarget() const
{
	return ChanceToApplyToTarget;
}

float FGameplayEffectSpec::GetModifierMagnitude(int32 ModifierIdx, bool bFactorInStackCount) const
{
	check(Modifiers.IsValidIndex(ModifierIdx) && Def && Def->Modifiers.IsValidIndex(ModifierIdx));

	const float SingleEvaluatedMagnitude = Modifiers[ModifierIdx].GetEvaluatedMagnitude();

	float ModMagnitude = SingleEvaluatedMagnitude;
	if (bFactorInStackCount)
	{
		ModMagnitude = GameplayEffectUtilities::ComputeStackedModifierMagnitude(SingleEvaluatedMagnitude, StackCount, Def->Modifiers[ModifierIdx].ModifierOp);
	}

	return ModMagnitude;
}

void FGameplayEffectSpec::CalculateModifierMagnitudes()
{
	for(int32 ModIdx = 0; ModIdx < Modifiers.Num(); ++ModIdx)
	{
		const FGameplayModifierInfo& ModDef = Def->Modifiers[ModIdx];
		FModifierSpec& ModSpec = Modifiers[ModIdx];

		if (ModDef.ModifierMagnitude.AttemptCalculateMagnitude(*this, ModSpec.EvaluatedMagnitude) == false)
		{
			ModSpec.EvaluatedMagnitude = 0.f;
			ABILITY_LOG(Warning, TEXT("Modifier on spec: %s was asked to CalculateMagnitude and failed, falling back to 0."), *ToSimpleString());
		}
	}
}

bool FGameplayEffectSpec::HasValidCapturedAttributes(const TArray<FGameplayEffectAttributeCaptureDefinition>& InCaptureDefsToCheck) const
{
	return CapturedRelevantAttributes.HasValidCapturedAttributes(InCaptureDefsToCheck);
}

const FGameplayEffectModifiedAttribute* FGameplayEffectSpec::GetModifiedAttribute(const FGameplayAttribute& Attribute) const
{
	for (const FGameplayEffectModifiedAttribute& ModifiedAttribute : ModifiedAttributes)
	{
		if (ModifiedAttribute.Attribute == Attribute)
		{
			return &ModifiedAttribute;
		}
	}
	return NULL;
}

FGameplayEffectModifiedAttribute* FGameplayEffectSpec::GetModifiedAttribute(const FGameplayAttribute& Attribute)
{
	for (FGameplayEffectModifiedAttribute& ModifiedAttribute : ModifiedAttributes)
	{
		if (ModifiedAttribute.Attribute == Attribute)
		{
			return &ModifiedAttribute;
		}
	}
	return NULL;
}

const FGameplayEffectModifiedAttribute* FGameplayEffectSpecForRPC::GetModifiedAttribute(const FGameplayAttribute& Attribute) const
{
	for (const FGameplayEffectModifiedAttribute& ModifiedAttribute : ModifiedAttributes)
	{
		if (ModifiedAttribute.Attribute == Attribute)
		{
			return &ModifiedAttribute;
		}
	}
	return NULL;
}

FGameplayEffectModifiedAttribute* FGameplayEffectSpec::AddModifiedAttribute(const FGameplayAttribute& Attribute)
{
	FGameplayEffectModifiedAttribute NewAttribute;
	NewAttribute.Attribute = Attribute;
	return &ModifiedAttributes[ModifiedAttributes.Add(NewAttribute)];
}

void FGameplayEffectSpec::PruneModifiedAttributes()
{
	TSet<FGameplayAttribute> ImportantAttributes;

	for (FGameplayEffectCue CueInfo : Def->GameplayCues)
	{
		if (CueInfo.MagnitudeAttribute.IsValid())
		{
			ImportantAttributes.Add(CueInfo.MagnitudeAttribute);
		}
	}

	// Remove all unimportant attributes
	for (int32 i = ModifiedAttributes.Num() - 1; i >= 0; i--)
	{
		if (!ImportantAttributes.Contains(ModifiedAttributes[i].Attribute))
		{
			ModifiedAttributes.RemoveAtSwap(i);
		}
	}
}

void FGameplayEffectSpec::SetContext(FGameplayEffectContextHandle NewEffectContext)
{
	bool bWasAlreadyInit = EffectContext.IsValid();
	EffectContext = NewEffectContext;	
	if (bWasAlreadyInit)
	{
		CaptureDataFromSource();
	}
}

void FGameplayEffectSpec::GetAllGrantedTags(OUT FGameplayTagContainer& Container) const
{
	Container.AppendTags(DynamicGrantedTags);
	Container.AppendTags(Def->InheritableOwnedTagsContainer.CombinedTags);
}

void FGameplayEffectSpec::SetSetByCallerMagnitude(FName DataName, float Magnitude)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const float* CurrentValue = SetByCallerMagnitudes.Find(DataName);
	if (CurrentValue)
	{
		ABILITY_LOG(Error, TEXT("FGameplayEffectSpec::SetMagnitude called on Data %s for Def %s when this magnitude was already set. Current Value: %.2f"), *DataName.ToString(), *Def->GetName(), *CurrentValue);
	}
#endif

	SetByCallerMagnitudes.Add(DataName) = Magnitude;
}

float FGameplayEffectSpec::GetSetByCallerMagnitude(FName DataName) const
{
	float Magnitude = 0.f;
	const float* Ptr = SetByCallerMagnitudes.Find(DataName);
	if (Ptr)
	{
		Magnitude = *Ptr;
	}
	else
	{
		ABILITY_LOG(Error, TEXT("FGameplayEffectSpec::GetMagnitude called for Data %s on Def %s when magnitude had not yet been set by caller."), *DataName.ToString(), *Def->GetName());
	}

	return Magnitude;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FGameplayEffectAttributeCaptureSpec
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

FGameplayEffectAttributeCaptureSpec::FGameplayEffectAttributeCaptureSpec()
{
}

FGameplayEffectAttributeCaptureSpec::FGameplayEffectAttributeCaptureSpec(const FGameplayEffectAttributeCaptureDefinition& InDefinition)
	: BackingDefinition(InDefinition)
{
}

bool FGameplayEffectAttributeCaptureSpec::HasValidCapture() const
{
	return (AttributeAggregator.Get() != nullptr);
}

bool FGameplayEffectAttributeCaptureSpec::AttemptCalculateAttributeMagnitude(const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutMagnitude) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutMagnitude = Agg->Evaluate(InEvalParams);
		return true;
	}

	return false;
}

bool FGameplayEffectAttributeCaptureSpec::AttemptCalculateAttributeMagnitudeWithBase(const FAggregatorEvaluateParameters& InEvalParams, float InBaseValue, OUT float& OutMagnitude) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutMagnitude = Agg->EvaluateWithBase(InBaseValue, InEvalParams);
		return true;
	}

	return false;
}

bool FGameplayEffectAttributeCaptureSpec::AttemptCalculateAttributeBaseValue(OUT float& OutBaseValue) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutBaseValue = Agg->GetBaseValue();
		return true;
	}

	return false;
}

bool FGameplayEffectAttributeCaptureSpec::AttemptCalculateAttributeBonusMagnitude(const FAggregatorEvaluateParameters& InEvalParams, OUT float& OutBonusMagnitude) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutBonusMagnitude = Agg->EvaluateBonus(InEvalParams);
		return true;
	}

	return false;
}

bool FGameplayEffectAttributeCaptureSpec::AttemptGetAttributeAggregatorSnapshot(OUT FAggregator& OutAggregatorSnapshot) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutAggregatorSnapshot.TakeSnapshotOf(*Agg);
		return true;
	}

	return false;
}

bool FGameplayEffectAttributeCaptureSpec::AttemptAddAggregatorModsToAggregator(OUT FAggregator& OutAggregatorToAddTo) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		OutAggregatorToAddTo.AddModsFrom(*Agg);
		return true;
	}

	return false;
}

void FGameplayEffectAttributeCaptureSpec::RegisterLinkedAggregatorCallback(FActiveGameplayEffectHandle Handle) const
{
	if (BackingDefinition.bSnapshot == false)
	{
		// Its possible the linked Aggregator is already gone.
		FAggregator* Agg = AttributeAggregator.Get();
		if (Agg)
		{
			Agg->AddDependent(Handle);
		}
	}
}

void FGameplayEffectAttributeCaptureSpec::UnregisterLinkedAggregatorCallback(FActiveGameplayEffectHandle Handle) const
{
	FAggregator* Agg = AttributeAggregator.Get();
	if (Agg)
	{
		Agg->RemoveDependent(Handle);
	}
}

bool FGameplayEffectAttributeCaptureSpec::ShouldRefreshLinkedAggregator(const FAggregator* ChangedAggregator) const
{
	return (BackingDefinition.bSnapshot == false && (ChangedAggregator == nullptr || AttributeAggregator.Get() == ChangedAggregator));
}

const FGameplayEffectAttributeCaptureDefinition& FGameplayEffectAttributeCaptureSpec::GetBackingDefinition() const
{
	return BackingDefinition;
}

FGameplayEffectAttributeCaptureSpecContainer::FGameplayEffectAttributeCaptureSpecContainer()
	: bHasNonSnapshottedAttributes(false)
{

}

FGameplayEffectAttributeCaptureSpecContainer::FGameplayEffectAttributeCaptureSpecContainer(FGameplayEffectAttributeCaptureSpecContainer&& Other)
	: SourceAttributes(MoveTemp(Other.SourceAttributes))
	, TargetAttributes(MoveTemp(Other.TargetAttributes))
	, bHasNonSnapshottedAttributes(Other.bHasNonSnapshottedAttributes)
{
}

FGameplayEffectAttributeCaptureSpecContainer::FGameplayEffectAttributeCaptureSpecContainer(const FGameplayEffectAttributeCaptureSpecContainer& Other)
	: SourceAttributes(Other.SourceAttributes)
	, TargetAttributes(Other.TargetAttributes)
	, bHasNonSnapshottedAttributes(Other.bHasNonSnapshottedAttributes)
{
}


FGameplayEffectAttributeCaptureSpecContainer& FGameplayEffectAttributeCaptureSpecContainer::operator=(FGameplayEffectAttributeCaptureSpecContainer&& Other)
{
	SourceAttributes = MoveTemp(Other.SourceAttributes);
	TargetAttributes = MoveTemp(Other.TargetAttributes);
	bHasNonSnapshottedAttributes = Other.bHasNonSnapshottedAttributes;
	return *this;
}

FGameplayEffectAttributeCaptureSpecContainer& FGameplayEffectAttributeCaptureSpecContainer::operator=(const FGameplayEffectAttributeCaptureSpecContainer& Other)
{
	SourceAttributes = Other.SourceAttributes;
	TargetAttributes = Other.TargetAttributes;
	bHasNonSnapshottedAttributes = Other.bHasNonSnapshottedAttributes;
	return *this;
}

void FGameplayEffectAttributeCaptureSpecContainer::AddCaptureDefinition(const FGameplayEffectAttributeCaptureDefinition& InCaptureDefinition)
{
	const bool bSourceAttribute = (InCaptureDefinition.AttributeSource == EGameplayEffectAttributeCaptureSource::Source);
	TArray<FGameplayEffectAttributeCaptureSpec>& AttributeArray = (bSourceAttribute ? SourceAttributes : TargetAttributes);

	// Only add additional captures if this exact capture definition isn't already being handled
	if (!AttributeArray.ContainsByPredicate(
			[&](const FGameplayEffectAttributeCaptureSpec& Element)
			{
				return Element.GetBackingDefinition() == InCaptureDefinition;
			}))
	{
		AttributeArray.Add(FGameplayEffectAttributeCaptureSpec(InCaptureDefinition));

		if (!InCaptureDefinition.bSnapshot)
		{
			bHasNonSnapshottedAttributes = true;
		}
	}
}

void FGameplayEffectAttributeCaptureSpecContainer::CaptureAttributes(UAbilitySystemComponent* InAbilitySystemComponent, EGameplayEffectAttributeCaptureSource InCaptureSource)
{
	if (InAbilitySystemComponent)
	{
		const bool bSourceComponent = (InCaptureSource == EGameplayEffectAttributeCaptureSource::Source);
		TArray<FGameplayEffectAttributeCaptureSpec>& AttributeArray = (bSourceComponent ? SourceAttributes : TargetAttributes);

		// Capture every spec's requirements from the specified component
		for (FGameplayEffectAttributeCaptureSpec& CurCaptureSpec : AttributeArray)
		{
			InAbilitySystemComponent->CaptureAttributeForGameplayEffect(CurCaptureSpec);
		}
	}
}

const FGameplayEffectAttributeCaptureSpec* FGameplayEffectAttributeCaptureSpecContainer::FindCaptureSpecByDefinition(const FGameplayEffectAttributeCaptureDefinition& InDefinition, bool bOnlyIncludeValidCapture) const
{
	const bool bSourceAttribute = (InDefinition.AttributeSource == EGameplayEffectAttributeCaptureSource::Source);
	const TArray<FGameplayEffectAttributeCaptureSpec>& AttributeArray = (bSourceAttribute ? SourceAttributes : TargetAttributes);

	const FGameplayEffectAttributeCaptureSpec* MatchingSpec = AttributeArray.FindByPredicate(
		[&](const FGameplayEffectAttributeCaptureSpec& Element)
	{
		return Element.GetBackingDefinition() == InDefinition;
	});

	// Null out the found results if the caller only wants valid captures and we don't have one yet
	if (MatchingSpec && bOnlyIncludeValidCapture && !MatchingSpec->HasValidCapture())
	{
		MatchingSpec = nullptr;
	}

	return MatchingSpec;
}

bool FGameplayEffectAttributeCaptureSpecContainer::HasValidCapturedAttributes(const TArray<FGameplayEffectAttributeCaptureDefinition>& InCaptureDefsToCheck) const
{
	bool bHasValid = true;

	for (const FGameplayEffectAttributeCaptureDefinition& CurDef : InCaptureDefsToCheck)
	{
		const FGameplayEffectAttributeCaptureSpec* CaptureSpec = FindCaptureSpecByDefinition(CurDef, true);
		if (!CaptureSpec)
		{
			bHasValid = false;
			break;
		}
	}

	return bHasValid;
}

bool FGameplayEffectAttributeCaptureSpecContainer::HasNonSnapshottedAttributes() const
{
	return bHasNonSnapshottedAttributes;
}

void FGameplayEffectAttributeCaptureSpecContainer::RegisterLinkedAggregatorCallbacks(FActiveGameplayEffectHandle Handle) const
{
	for (const FGameplayEffectAttributeCaptureSpec& CaptureSpec : SourceAttributes)
	{
		CaptureSpec.RegisterLinkedAggregatorCallback(Handle);
	}

	for (const FGameplayEffectAttributeCaptureSpec& CaptureSpec : TargetAttributes)
	{
		CaptureSpec.RegisterLinkedAggregatorCallback(Handle);
	}
}

void FGameplayEffectAttributeCaptureSpecContainer::UnregisterLinkedAggregatorCallbacks(FActiveGameplayEffectHandle Handle) const
{
	for (const FGameplayEffectAttributeCaptureSpec& CaptureSpec : SourceAttributes)
	{
		CaptureSpec.UnregisterLinkedAggregatorCallback(Handle);
	}

	for (const FGameplayEffectAttributeCaptureSpec& CaptureSpec : TargetAttributes)
	{
		CaptureSpec.UnregisterLinkedAggregatorCallback(Handle);
	}
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FActiveGameplayEffect
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

FActiveGameplayEffect::FActiveGameplayEffect()
	: StartGameStateTime(0)
	, CachedStartGameStateTime(0)
	, StartWorldTime(0.f)
	, bIsInhibited(true)
	, bPendingRepOnActiveGC(false)
	, bPendingRepWhileActiveGC(false)
	, IsPendingRemove(false)
	, PendingNext(nullptr)
{
}

FActiveGameplayEffect::FActiveGameplayEffect(const FActiveGameplayEffect& Other)
{
	*this = Other;
}

FActiveGameplayEffect::FActiveGameplayEffect(FActiveGameplayEffectHandle InHandle, const FGameplayEffectSpec &InSpec, float CurrentWorldTime, int32 InStartGameStateTime, FPredictionKey InPredictionKey)
	: Handle(InHandle)
	, Spec(InSpec)
	, PredictionKey(InPredictionKey)
	, StartGameStateTime(InStartGameStateTime)
	, CachedStartGameStateTime(InStartGameStateTime)
	, StartWorldTime(CurrentWorldTime)
	, bIsInhibited(true)
	, bPendingRepOnActiveGC(false)
	, bPendingRepWhileActiveGC(false)
	, IsPendingRemove(false)
	, PendingNext(nullptr)
{
}

FActiveGameplayEffect::FActiveGameplayEffect(FActiveGameplayEffect&& Other)
	:Handle(Other.Handle)
	,Spec(MoveTemp(Other.Spec))
	,PredictionKey(Other.PredictionKey)
	,StartGameStateTime(Other.StartGameStateTime)
	,CachedStartGameStateTime(Other.CachedStartGameStateTime)
	,StartWorldTime(Other.StartWorldTime)
	,bIsInhibited(Other.bIsInhibited)
	,bPendingRepOnActiveGC(Other.bPendingRepOnActiveGC)
	,bPendingRepWhileActiveGC(Other.bPendingRepWhileActiveGC)
	,IsPendingRemove(Other.IsPendingRemove)
	,OnRemovedDelegate(Other.OnRemovedDelegate)
	,PeriodHandle(Other.PeriodHandle)
	,DurationHandle(Other.DurationHandle)
{
	// Note: purposefully not copying PendingNext pointer.
}

FActiveGameplayEffect& FActiveGameplayEffect::operator=(FActiveGameplayEffect&& Other)
{
	Handle = Other.Handle;
	Spec = MoveTemp(Other.Spec);
	PredictionKey = Other.PredictionKey;
	StartGameStateTime = Other.StartGameStateTime;
	CachedStartGameStateTime = Other.CachedStartGameStateTime;
	StartWorldTime = Other.StartWorldTime;
	bIsInhibited = Other.bIsInhibited;
	bPendingRepOnActiveGC = Other.bPendingRepOnActiveGC;
	bPendingRepWhileActiveGC = Other.bPendingRepWhileActiveGC;
	IsPendingRemove = Other.IsPendingRemove;
	OnRemovedDelegate = Other.OnRemovedDelegate;
	PeriodHandle = Other.PeriodHandle;
	DurationHandle = Other.DurationHandle;
	// Note: purposefully not copying PendingNext pointer.
	return *this;
}

FActiveGameplayEffect& FActiveGameplayEffect::operator=(const FActiveGameplayEffect& Other)
{
	Handle = Other.Handle;
	Spec = Other.Spec;
	PredictionKey = Other.PredictionKey;
	StartGameStateTime = Other.StartGameStateTime;
	CachedStartGameStateTime = Other.CachedStartGameStateTime;
	StartWorldTime = Other.StartWorldTime;
	bIsInhibited = Other.bIsInhibited;
	bPendingRepOnActiveGC = Other.bPendingRepOnActiveGC;
	bPendingRepWhileActiveGC = Other.bPendingRepWhileActiveGC;
	IsPendingRemove = Other.IsPendingRemove;
	OnRemovedDelegate = Other.OnRemovedDelegate;
	PeriodHandle = Other.PeriodHandle;
	DurationHandle = Other.DurationHandle;
	PendingNext = Other.PendingNext;
	return *this;
}

/** This is the core function that turns the ActiveGE 'on' or 'off */
void FActiveGameplayEffect::CheckOngoingTagRequirements(const FGameplayTagContainer& OwnerTags, FActiveGameplayEffectsContainer& OwningContainer, bool bInvokeGameplayCueEvents)
{
	bool bShouldBeInhibited = !Spec.Def->OngoingTagRequirements.RequirementsMet(OwnerTags);

	if (bIsInhibited != bShouldBeInhibited)
	{
		// All OnDirty callbacks must be inhibited until we update this entire GameplayEffect.
		FScopedAggregatorOnDirtyBatch	AggregatorOnDirtyBatcher;

		if (bShouldBeInhibited)
		{
			// Remove our ActiveGameplayEffects modifiers with our Attribute Aggregators
			OwningContainer.RemoveActiveGameplayEffectGrantedTagsAndModifiers(*this, bInvokeGameplayCueEvents);
		}
		else
		{
			OwningContainer.AddActiveGameplayEffectGrantedTagsAndModifiers(*this, bInvokeGameplayCueEvents);
		}

		bIsInhibited = bShouldBeInhibited;
	}
}

void FActiveGameplayEffect::PreReplicatedRemove(const struct FActiveGameplayEffectsContainer &InArray)
{
	if (Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("Received PreReplicatedRemove with no UGameplayEffect def."));
		return;
	}

	ABILITY_LOG(Verbose, TEXT("PreReplicatedRemove: %s %s Marked as Pending Remove: %s"), *Handle.ToString(), *Spec.Def->GetName(), IsPendingRemove ? TEXT("TRUE") : TEXT("FALSE"));

	const_cast<FActiveGameplayEffectsContainer&>(InArray).InternalOnActiveGameplayEffectRemoved(*this);	// Const cast is ok. It is there to prevent mutation of the GameplayEffects array, which this wont do.
	
	// Only invoke the GC event if the GE was not inhibited when it was removed
	if (bIsInhibited == false)
	{
		InArray.Owner->InvokeGameplayCueEvent(Spec, EGameplayCueEvent::Removed);
	}
}

void FActiveGameplayEffect::PostReplicatedAdd(const struct FActiveGameplayEffectsContainer &InArray)
{
	if (Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("Received ReplicatedGameplayEffect with no UGameplayEffect def."));
		return;
	}

	bool ShouldInvokeGameplayCueEvents = true;
	if (PredictionKey.IsLocalClientKey())
	{
		// PredictionKey will only be valid on the client that predicted it. So if this has a valid PredictionKey, we can assume we already predicted it and shouldn't invoke GameplayCues.
		// We may need to do more bookkeeping here in the future. Possibly give the predicted gameplayeffect a chance to pass something off to the new replicated gameplay effect.
		
		if (InArray.HasPredictedEffectWithPredictedKey(PredictionKey))
		{
			ShouldInvokeGameplayCueEvents = false;
		}
	}

	// Handles are not replicated, so create a new one.
	Handle = FActiveGameplayEffectHandle::GenerateNewHandle(InArray.Owner);

	const_cast<FActiveGameplayEffectsContainer&>(InArray).InternalOnActiveGameplayEffectAdded(*this);	// Const cast is ok. It is there to prevent mutation of the GameplayEffects array, which this wont do.

	static const int32 MAX_DELTA_TIME = 3;

	// Was this actually just activated, or are we just finding out about it due to relevancy/join in progress?
	float WorldTimeSeconds = InArray.GetWorldTime();
	int32 GameStateTime = InArray.GetGameStateTime();

	int32 DeltaGameStateTime = GameStateTime - StartGameStateTime;	// How long we think the effect has been playing (only 1 second precision!)

	// Set our local start time accordingly
	StartWorldTime = WorldTimeSeconds - static_cast<float>(DeltaGameStateTime);
	CachedStartGameStateTime = StartGameStateTime;

	if (ShouldInvokeGameplayCueEvents)
	{
		// These events will get invoked if, after the parent array has been completely updated, this GE is still not inhibited
		bPendingRepOnActiveGC = (GameStateTime > 0 && FMath::Abs(DeltaGameStateTime) < MAX_DELTA_TIME);
		bPendingRepWhileActiveGC = true;
	}
}

void FActiveGameplayEffect::PostReplicatedChange(const struct FActiveGameplayEffectsContainer &InArray)
{
	if (Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("Received ReplicatedGameplayEffect with no UGameplayEffect def."));
	}

	// Handle potential duration refresh
	// @todo: Due to precision of gamestate timer, this could be incorrect by just under an entire second; Need more precise replicated timer
	if (CachedStartGameStateTime != StartGameStateTime)
	{
		StartWorldTime = InArray.GetWorldTime() - static_cast<float>(InArray.GetGameStateTime() - StartGameStateTime);
		CachedStartGameStateTime = StartGameStateTime;
	}

	// Const cast is ok. It is there to prevent mutation of the GameplayEffects array, which this wont do.
	const_cast<FActiveGameplayEffectsContainer&>(InArray).UpdateAllAggregatorModMagnitudes(*this);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FActiveGameplayEffectsContainer
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

FActiveGameplayEffectsContainer::FActiveGameplayEffectsContainer()
	: Owner(nullptr)
	, ScopedLockCount(0)
	, PendingRemoves(0)
	, PendingGameplayEffectHead(nullptr)
{
	PendingGameplayEffectNext = &PendingGameplayEffectHead;
}

FActiveGameplayEffectsContainer::~FActiveGameplayEffectsContainer()
{
	FActiveGameplayEffect* PendingGameplayEffect = PendingGameplayEffectHead;
	if (PendingGameplayEffectHead)
	{
		FActiveGameplayEffect* Next = PendingGameplayEffectHead->PendingNext;
		delete PendingGameplayEffectHead;
		PendingGameplayEffectHead =	Next;
	}
}

void FActiveGameplayEffectsContainer::RegisterWithOwner(UAbilitySystemComponent* InOwner)
{
	if (Owner != InOwner)
	{
		Owner = InOwner;

		// Binding raw is ok here, since the owner is literally the UObject that owns us. If we are destroyed, its because that uobject is destroyed,
		// and if that is destroyed, the delegate wont be able to fire.
		Owner->RegisterGenericGameplayTagEvent().AddRaw(this, &FActiveGameplayEffectsContainer::OnOwnerTagChange);
	}
}

/** This is the main function that executes a GameplayEffect on Attributes and ActiveGameplayEffects */
void FActiveGameplayEffectsContainer::ExecuteActiveEffectsFrom(FGameplayEffectSpec &Spec, FPredictionKey PredictionKey)
{
	FGameplayEffectSpec& SpecToUse = Spec;

	// Capture our own tags.
	// TODO: We should only capture them if we need to. We may have snapshotted target tags (?) (in the case of dots with exotic setups?)

	SpecToUse.CapturedTargetTags.GetActorTags().RemoveAllTags();
	Owner->GetOwnedGameplayTags(SpecToUse.CapturedTargetTags.GetActorTags());

	SpecToUse.CalculateModifierMagnitudes();

	// ------------------------------------------------------
	//	Modifiers
	//		These will modify the base value of attributes
	// ------------------------------------------------------
	
	bool ModifierSuccessfullyExecuted = false;

	for (int32 ModIdx = 0; ModIdx < SpecToUse.Modifiers.Num(); ++ModIdx)
	{
		const FGameplayModifierInfo& ModDef = SpecToUse.Def->Modifiers[ModIdx];
		
		FGameplayModifierEvaluatedData EvalData(ModDef.Attribute, ModDef.ModifierOp, SpecToUse.GetModifierMagnitude(ModIdx, true));
		ModifierSuccessfullyExecuted |= InternalExecuteMod(SpecToUse, EvalData);
	}

	// ------------------------------------------------------
	//	Executions
	//		This will run custom code to 'do stuff'
	// ------------------------------------------------------
	
	TArray< FGameplayEffectSpecHandle > ConditionalEffectSpecs;
	bool GameplayCuesWereManuallyHandled = false;

	for (const FGameplayEffectExecutionDefinition& CurExecDef : SpecToUse.Def->Executions)
	{
		if (CurExecDef.CalculationClass)
		{
			const UGameplayEffectExecutionCalculation* ExecCDO = CurExecDef.CalculationClass->GetDefaultObject<UGameplayEffectExecutionCalculation>();
			check(ExecCDO);

			// Run the custom execution
			FGameplayEffectCustomExecutionParameters ExecutionParams(SpecToUse, CurExecDef.CalculationModifiers, Owner, CurExecDef.PassedInTags);
			FGameplayEffectCustomExecutionOutput ExecutionOutput;
			ExecCDO->Execute(ExecutionParams, ExecutionOutput);

			const bool bRunConditionalEffects = ExecutionOutput.ShouldTriggerConditionalGameplayEffects();
			if (bRunConditionalEffects)
			{
				// If successful, apply conditional specs
				for (const TSubclassOf<UGameplayEffect>& TargetDefClass : CurExecDef.ConditionalGameplayEffectClasses)
				{
					if (*TargetDefClass != nullptr)
					{
						const UGameplayEffect* TargetDef = TargetDefClass->GetDefaultObject<UGameplayEffect>();

						ConditionalEffectSpecs.Add(FGameplayEffectSpecHandle(new FGameplayEffectSpec(TargetDef, Spec.GetEffectContext(), Spec.GetLevel())));
					}
				}
			}

			// Execute any mods the custom execution yielded
			TArray<FGameplayModifierEvaluatedData> OutModifiers;
			ExecutionOutput.GetOutputModifiers(OutModifiers);

			const bool bApplyStackCountToEmittedMods = !ExecutionOutput.IsStackCountHandledManually();
			const int32 SpecStackCount = SpecToUse.StackCount;

			for (FGameplayModifierEvaluatedData& CurExecMod : OutModifiers)
			{
				// If the execution didn't manually handle the stack count, automatically apply it here
				if (bApplyStackCountToEmittedMods && SpecStackCount > 1)
				{
					CurExecMod.Magnitude = GameplayEffectUtilities::ComputeStackedModifierMagnitude(CurExecMod.Magnitude, SpecStackCount, CurExecMod.ModifierOp);
				}
				ModifierSuccessfullyExecuted |= InternalExecuteMod(SpecToUse, CurExecMod);
			}

			// If execution handled GameplayCues, we dont have to.
			if (ExecutionOutput.AreGameplayCuesHandledManually())
			{
				GameplayCuesWereManuallyHandled = true;
			}
		}
	}

	// ------------------------------------------------------
	//	Invoke GameplayCue events
	// ------------------------------------------------------
	
	// Prune the modified attributes before we replicate
	SpecToUse.PruneModifiedAttributes();

	// If there are no modifiers or we don't require modifier success to trigger, we apply the GameplayCue. 
	bool InvokeGameplayCueExecute = (SpecToUse.Modifiers.Num() == 0) || !Spec.Def->bRequireModifierSuccessToTriggerCues;

	// If there are modifiers, we only want to invoke the GameplayCue if one of them went through (could be blocked by immunity or % chance roll)
	if (SpecToUse.Modifiers.Num() > 0 && ModifierSuccessfullyExecuted)
	{
		InvokeGameplayCueExecute = true;
	}

	// Don't trigger gameplay cues if one of the executions says it manually handled them
	if (GameplayCuesWereManuallyHandled)
	{
		InvokeGameplayCueExecute = false;
	}

	if (InvokeGameplayCueExecute && SpecToUse.Def->GameplayCues.Num())
	{
		// TODO: check replication policy. Right now we will replicate every execute via a multicast RPC

		ABILITY_LOG(Log, TEXT("Invoking Execute GameplayCue for %s"), *SpecToUse.ToSimpleString());

		UAbilitySystemGlobals::Get().GetGameplayCueManager()->InvokeGameplayCueExecuted_FromSpec(Owner, SpecToUse, PredictionKey);
	}

	// Apply any conditional linked effects

	for (const FGameplayEffectSpecHandle TargetSpec : ConditionalEffectSpecs)
	{
		if (TargetSpec.IsValid())
		{
			Owner->ApplyGameplayEffectSpecToSelf(*TargetSpec.Data.Get(), PredictionKey);
		}
	}
}

void FActiveGameplayEffectsContainer::ExecutePeriodicGameplayEffect(FActiveGameplayEffectHandle Handle)
{
	GAMEPLAYEFFECT_SCOPE_LOCK();
	FActiveGameplayEffect* ActiveEffect = GetActiveGameplayEffect(Handle);
	if (ActiveEffect)
	{
		if (UE_LOG_ACTIVE(VLogAbilitySystem, Log))
		{
			ABILITY_VLOG(Owner->OwnerActor, Log, TEXT("Executed Periodic Effect %s"), *ActiveEffect->Spec.Def->GetFName().ToString());
			for (FGameplayModifierInfo Modifier : ActiveEffect->Spec.Def->Modifiers)
			{
				float Magnitude = 0.f;
				Modifier.ModifierMagnitude.AttemptCalculateMagnitude(ActiveEffect->Spec, Magnitude);
				ABILITY_VLOG(Owner->OwnerActor, Log, TEXT("         %s: %s %f"), *Modifier.Attribute.GetName(), *EGameplayModOpToString(Modifier.ModifierOp), Magnitude);
			}
		}

		// Clear modified attributes before each periodic execution
		ActiveEffect->Spec.ModifiedAttributes.Empty();

		// Execute
		ExecuteActiveEffectsFrom(ActiveEffect->Spec);
	}

}

FActiveGameplayEffect* FActiveGameplayEffectsContainer::GetActiveGameplayEffect(const FActiveGameplayEffectHandle Handle)
{
	for (FActiveGameplayEffect& Effect : this)
	{
		if (Effect.Handle == Handle)
		{
			return &Effect;
		}
	}
	return nullptr;
}

const FActiveGameplayEffect* FActiveGameplayEffectsContainer::GetActiveGameplayEffect(const FActiveGameplayEffectHandle Handle) const
{
	for (const FActiveGameplayEffect& Effect : this)
	{
		if (Effect.Handle == Handle)
		{
			return &Effect;
		}
	}
	return nullptr;
}

FAggregatorRef& FActiveGameplayEffectsContainer::FindOrCreateAttributeAggregator(FGameplayAttribute Attribute)
{
	FAggregatorRef* RefPtr = AttributeAggregatorMap.Find(Attribute);
	if (RefPtr)
	{
		return *RefPtr;
	}

	// Create a new aggregator for this attribute.
	float CurrentValueOfProperty = Owner->GetNumericAttribute(Attribute);
	ABILITY_LOG(Log, TEXT("Creating new entry in AttributeAggregatorMap for %s. CurrentValue: %.2f"), *Attribute.GetName(), CurrentValueOfProperty);

	FAggregator* NewAttributeAggregator = new FAggregator(CurrentValueOfProperty);
	
	if (Attribute.IsSystemAttribute() == false)
	{
		NewAttributeAggregator->OnDirty.AddUObject(Owner, &UAbilitySystemComponent::OnAttributeAggregatorDirty, Attribute);
	}

	return AttributeAggregatorMap.Add(Attribute, FAggregatorRef(NewAttributeAggregator));
}

void FActiveGameplayEffectsContainer::OnAttributeAggregatorDirty(FAggregator* Aggregator, FGameplayAttribute Attribute)
{
	ABILITY_LOG_SCOPE(TEXT("FActiveGameplayEffectsContainer::OnAttributeAggregatorDirty"));
	check(AttributeAggregatorMap.FindChecked(Attribute).Get() == Aggregator);

	// Our Aggregator has changed, we need to reevaluate this aggregator and update the current value of the attribute.
	// Note that this is not an execution, so there are no 'source' and 'target' tags to fill out in the FAggregatorEvaluateParameters.
	// ActiveGameplayEffects that have required owned tags will be turned on/off via delegates, and will add/remove themselves from attribute
	// aggregators when that happens.
	
	FAggregatorEvaluateParameters EvaluationParameters;

	if (Owner->IsNetSimulating())
	{
		if (FScopedAggregatorOnDirtyBatch::GlobalFromNetworkUpdate && Aggregator->NetUpdateID != FScopedAggregatorOnDirtyBatch::NetUpdateID)
		{
			// We are a client. The current value of this attribute is the replicated server's "final" value. We dont actually know what the 
			// server's base value is. But we can calculate it with ReverseEvaluate(). Then, we can call Evaluate with IncludePredictiveMods=true
			// to apply our mods and get an accurate predicted value.
			// 
			// It is very important that we only do this exactly one time when we get a new value from the server. Once we set the new local value for this
			// attribute below, recalculating the base would give us the wrong server value. We should only do this when we are coming directly from a network update.
			// 
			// Unfortunately there are two ways we could get here from a network update: from the ActiveGameplayEffect container being updated or from a traditional
			// OnRep on the actual attribute uproperty. Both of these could happen in a single network update, or potentially only one could happen
			// (and in fact it could be either one! the AGE container could change in a way that doesnt change the final attribute value, or we could have the base value
			// of the attribute actually be modified (e.g,. losing health or mana which only results in an OnRep and not in a AGE being applied).
			// 
			// So both paths need to lead to this function, but we should only do it one time per update. Once we update the base value, we need to make sure we dont do it again
			// until we get a new network update. GlobalFromNetworkUpdate and NetUpdateID are what do this.
			// 
			// GlobalFromNetworkUpdate - only set to true when we are coming from an OnRep or when we are coming from an ActiveGameplayEffect container net update.
			// NetUpdateID - updated once whenever an AttributeSet is received over the network. It will be incremented one time per actor that gets an update.
					

			float FinalValue = Owner->GetNumericAttribute(Attribute);
			float BaseValue = Aggregator->ReverseEvaluate(FinalValue, EvaluationParameters);
			Aggregator->SetBaseValue(BaseValue, false);
			Aggregator->NetUpdateID = FScopedAggregatorOnDirtyBatch::NetUpdateID;

			ABILITY_LOG(Log, TEXT("Reverse Evaluated %s. FinalValue: %.2f  BaseValue: %.2f "), *Attribute.GetName(), FinalValue, BaseValue);
		}

		EvaluationParameters.IncludePredictiveMods = true;
	}
	
	float NewValue = Aggregator->Evaluate(EvaluationParameters);

	if (EvaluationParameters.IncludePredictiveMods)
	{
		ABILITY_LOG(Log, TEXT("After Prediction, FinalValue: %.2f"), NewValue);
	}

	InternalUpdateNumericalAttribute(Attribute, NewValue, nullptr);
}

void FActiveGameplayEffectsContainer::OnMagnitudeDependencyChange(FActiveGameplayEffectHandle Handle, const FAggregator* ChangedAgg)
{
	if (Handle.IsValid())
	{
		GAMEPLAYEFFECT_SCOPE_LOCK();
		FActiveGameplayEffect* ActiveEffect = GetActiveGameplayEffect(Handle);
		if (ActiveEffect)
		{
			// This handle registered with the ChangedAgg to be notified when the aggregator changed.
			// At this point we don't know what actually needs to be updated inside this active gameplay effect.
			FGameplayEffectSpec& Spec = ActiveEffect->Spec;

			// We must update attribute aggregators only if we are actually 'on' right now, and if we are non periodic (periodic effects do their thing on execute callbacks)
			bool MustUpdateAttributeAggregators = (ActiveEffect->bIsInhibited == false && (Spec.GetPeriod() <= UGameplayEffect::NO_PERIOD));

			// As we update our modifier magnitudes, we will update our owner's attribute aggregators. When we do this, we have to clear them first of all of our (Handle's) previous mods.
			// Since we could potentially have two mods to the same attribute, one that gets updated, and one that doesnt - we need to do this in two passes.
			TSet<FGameplayAttribute>	AttributesToUpdate;

			// First pass: update magnitudes of our modifiers that changed
			for(int32 ModIdx = 0; ModIdx < Spec.Modifiers.Num(); ++ModIdx)
			{
				const FGameplayModifierInfo& ModDef = Spec.Def->Modifiers[ModIdx];
				FModifierSpec& ModSpec = Spec.Modifiers[ModIdx];

				if (ModDef.ModifierMagnitude.AttemptRecalculateMagnitudeFromDependentChange(Spec, ModSpec.EvaluatedMagnitude, ChangedAgg))
				{
					// We changed, so we need to reapply/update our spot in the attribute aggregator map
					if (MustUpdateAttributeAggregators)
					{
						AttributesToUpdate.Add(ModDef.Attribute);
					}
				}
			}

			// Second pass, update the aggregators that we need to
			UpdateAggregatorModMagnitudes(AttributesToUpdate, *ActiveEffect);
		}
	}
}

void FActiveGameplayEffectsContainer::OnStackCountChange(FActiveGameplayEffect& ActiveEffect)
{
	MarkItemDirty(ActiveEffect);
	UpdateAllAggregatorModMagnitudes(ActiveEffect);
}

void FActiveGameplayEffectsContainer::UpdateAllAggregatorModMagnitudes(FActiveGameplayEffect& ActiveEffect)
{
	const FGameplayEffectSpec& Spec = ActiveEffect.Spec;
	TSet<FGameplayAttribute> AttributesToUpdate;

	for (int32 ModIdx = 0; ModIdx < Spec.Modifiers.Num(); ++ModIdx)
	{
		const FGameplayModifierInfo& ModDef = Spec.Def->Modifiers[ModIdx];
		AttributesToUpdate.Add(ModDef.Attribute);
	}

	UpdateAggregatorModMagnitudes(AttributesToUpdate, ActiveEffect);
}

void FActiveGameplayEffectsContainer::UpdateAggregatorModMagnitudes(const TSet<FGameplayAttribute>& AttributesToUpdate, FActiveGameplayEffect& ActiveEffect)
{
	const FGameplayEffectSpec& Spec = ActiveEffect.Spec;
	for (const FGameplayAttribute& Attribute : AttributesToUpdate)
	{
		FAggregator* Aggregator = FindOrCreateAttributeAggregator(Attribute).Get();
		check(Aggregator);

		// Remove ALL of our mods (this is all we can do! We don't keep track of aggregator mods <=> spec mod mappings)
		// Todo: it would be nice to have a better way of updating this. Some ideas: instead of remove, call an 'invalidate' to NAN out all floats for a given handle, then call an 'update' to set real values.
		Aggregator->RemoveAggregatorMod(ActiveEffect.Handle);

		// Now re-add ALL of our mods
		for(int32 ModIdx = 0; ModIdx < Spec.Modifiers.Num(); ++ModIdx)
		{
			const FGameplayModifierInfo& ModDef = Spec.Def->Modifiers[ModIdx];

			if (ModDef.Attribute == Attribute)
			{
				Aggregator->AddAggregatorMod(Spec.GetModifierMagnitude(ModIdx, true), ModDef.ModifierOp, &ModDef.SourceTags, &ModDef.TargetTags, ActiveEffect.PredictionKey.WasLocallyGenerated(), ActiveEffect.Handle);
			}
		}
	}
}

FActiveGameplayEffect* FActiveGameplayEffectsContainer::FindStackableActiveGameplayEffect(const FGameplayEffectSpec& Spec)
{
	FActiveGameplayEffect* StackableGE = nullptr;
	const UGameplayEffect* GEDef = Spec.Def;
	EGameplayEffectStackingType StackingType = GEDef->StackingType;

	if (StackingType != EGameplayEffectStackingType::None && Spec.GetDuration() != UGameplayEffect::INSTANT_APPLICATION)
	{
		// Iterate through GameplayEffects to see if we find a match. Note that we could cache off a handle in a map but we would still
		// do a linear search through GameplayEffects to find the actual FActiveGameplayEffect (due to unstable nature of the GameplayEffects array).
		// If this becomes a slow point in the profiler, the map may still be useful as an early out to avoid an unnecessary sweep.
		UAbilitySystemComponent* SourceASC = Spec.GetContext().GetInstigatorAbilitySystemComponent();
		for (FActiveGameplayEffect& ActiveEffect: this)
		{
			// Aggregate by source stacking additionally requires the source ability component to match
			if (ActiveEffect.Spec.Def == Spec.Def && ((StackingType == EGameplayEffectStackingType::AggregateByTarget) || (SourceASC && SourceASC == ActiveEffect.Spec.GetContext().GetInstigatorAbilitySystemComponent())))
			{
				StackableGE = &ActiveEffect;
				break;
			}
		}
	}

	return StackableGE;
}

float FActiveGameplayEffectsContainer::ComputeModifiedDurationOfAppliedSpec(const FGameplayEffectSpec& Spec, float BaseValue) const
{
	FAggregator DurationAgg;

	const FGameplayEffectAttributeCaptureSpec* OutgoingCaptureSpec = Spec.CapturedRelevantAttributes.FindCaptureSpecByDefinition(UAbilitySystemComponent::GetOutgoingDurationCapture(), true);
	if (OutgoingCaptureSpec)
	{
		OutgoingCaptureSpec->AttemptAddAggregatorModsToAggregator(DurationAgg);
	}

	const FGameplayEffectAttributeCaptureSpec* IncomingCaptureSpec = Spec.CapturedRelevantAttributes.FindCaptureSpecByDefinition(UAbilitySystemComponent::GetIncomingDurationCapture(), true);
	if (IncomingCaptureSpec)
	{
		IncomingCaptureSpec->AttemptAddAggregatorModsToAggregator(DurationAgg);
	}

	FAggregatorEvaluateParameters Params;
	Params.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	Params.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	return DurationAgg.EvaluateWithBase(BaseValue, Params);
}

bool FActiveGameplayEffectsContainer::HandleActiveGameplayEffectStackOverflow(const FActiveGameplayEffect& ActiveStackableGE, const FGameplayEffectSpec& OldSpec, const FGameplayEffectSpec& OverflowingSpec)
{
	const UGameplayEffect* StackedGE = OldSpec.Def;
	
	const bool bAllowOverflowApplication = !(StackedGE->bDenyOverflowApplication);

	FPredictionKey PredictionKey;
	for (TSubclassOf<UGameplayEffect> OverflowEffect : StackedGE->OverflowEffects)
	{
		if (OverflowEffect)
		{
			FGameplayEffectSpec NewGESpec(OverflowEffect->GetDefaultObject<UGameplayEffect>(), OverflowingSpec.GetContext(), OverflowingSpec.GetLevel());
			// @todo: copy over source tags
			// @todo: scope lock
			Owner->ApplyGameplayEffectSpecToSelf(NewGESpec, PredictionKey);
		}
	}
	// @todo: Scope lock
	if (!bAllowOverflowApplication && StackedGE->bClearStackOnOverflow)
	{
		Owner->RemoveActiveGameplayEffect(ActiveStackableGE.Handle);
	}

	return bAllowOverflowApplication;
}

void FActiveGameplayEffectsContainer::ApplyStackingLogicPostApplyAsSource(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle)
{
	if (SpecApplied.Def->StackingType == EGameplayEffectStackingType::AggregateBySource)
	{
		TArray<FActiveGameplayEffectHandle>& ActiveHandles = SourceStackingMap.FindOrAdd(SpecApplied.Def);

		// This is probably wrong!
		if (ActiveHandles.Num() == SpecApplied.Def->StackLimitCount)
		{
			// We are at the limit, so replace one based on policy
			// For now, just always remove the oldest one applied
		}
	}
}

void FActiveGameplayEffectsContainer::SetBaseAttributeValueFromReplication(FGameplayAttribute Attribute, float ServerValue)
{
	FAggregatorRef* RefPtr = AttributeAggregatorMap.Find(Attribute);
	if (RefPtr && RefPtr->Get())
	{
		FAggregator* Aggregator =  RefPtr->Get();
		
		FScopedAggregatorOnDirtyBatch::GlobalFromNetworkUpdate = true;
		OnAttributeAggregatorDirty(Aggregator, Attribute);
		FScopedAggregatorOnDirtyBatch::GlobalFromNetworkUpdate = false;
	}
}

void FActiveGameplayEffectsContainer::GetAllActiveGameplayEffectSpecs(TArray<FGameplayEffectSpec>& OutSpecCopies)
{
	for (const FActiveGameplayEffect& ActiveEffect : this)	
	{
		OutSpecCopies.Add(ActiveEffect.Spec);
	}
}

float FActiveGameplayEffectsContainer::GetGameplayEffectDuration(FActiveGameplayEffectHandle Handle) const
{
	for (const FActiveGameplayEffect& ActiveEffect : this)
	{
		if (ActiveEffect.Handle == Handle)
		{
			return ActiveEffect.GetDuration();
		}
	}
	
	ABILITY_LOG(Warning, TEXT("GetGameplayEffectDuration called with invalid Handle: %s"), *Handle.ToString() );
	return UGameplayEffect::INFINITE_DURATION;
}

float FActiveGameplayEffectsContainer::GetGameplayEffectMagnitude(FActiveGameplayEffectHandle Handle, FGameplayAttribute Attribute) const
{
	for (const FActiveGameplayEffect& Effect : this)
	{
		if (Effect.Handle == Handle)
		{
			for(int32 ModIdx = 0; ModIdx < Effect.Spec.Modifiers.Num(); ++ModIdx)
			{
				const FGameplayModifierInfo& ModDef = Effect.Spec.Def->Modifiers[ModIdx];
				const FModifierSpec& ModSpec = Effect.Spec.Modifiers[ModIdx];
			
				if (ModDef.Attribute == Attribute)
				{
					return ModSpec.GetEvaluatedMagnitude();
				}
			}
		}
	}

	ABILITY_LOG(Warning, TEXT("GetGameplayEffectMagnitude called with invalid Handle: %s"), *Handle.ToString());
	return -1.f;
}

const FGameplayTagContainer* FActiveGameplayEffectsContainer::GetGameplayEffectSourceTagsFromHandle(FActiveGameplayEffectHandle Handle) const
{
	// @todo: Need to consider this with tag changes
	for (const FActiveGameplayEffect& Effect : this)
	{
		if (Effect.Handle == Handle)
		{
			return Effect.Spec.CapturedSourceTags.GetAggregatedTags();
		}
	}

	return nullptr;
}

const FGameplayTagContainer* FActiveGameplayEffectsContainer::GetGameplayEffectTargetTagsFromHandle(FActiveGameplayEffectHandle Handle) const
{
	// @todo: Need to consider this with tag changes
	const FActiveGameplayEffect* Effect = GetActiveGameplayEffect(Handle);
	if (Effect)
	{
		return Effect->Spec.CapturedTargetTags.GetAggregatedTags();
	}

	return nullptr;
}

void FActiveGameplayEffectsContainer::CaptureAttributeForGameplayEffect(OUT FGameplayEffectAttributeCaptureSpec& OutCaptureSpec)
{
	FAggregatorRef& AttributeAggregator = FindOrCreateAttributeAggregator(OutCaptureSpec.BackingDefinition.AttributeToCapture);
	
	if (OutCaptureSpec.BackingDefinition.bSnapshot)
	{
		OutCaptureSpec.AttributeAggregator.TakeSnapshotOf(AttributeAggregator);
	}
	else
	{
		OutCaptureSpec.AttributeAggregator = AttributeAggregator;
	}
}

void FActiveGameplayEffectsContainer::InternalUpdateNumericalAttribute(FGameplayAttribute Attribute, float NewValue, const FGameplayEffectModCallbackData* ModData)
{
	ABILITY_LOG(Log, TEXT("Property %s new value is: %.2f"), *Attribute.GetName(), NewValue);
	Owner->SetNumericAttribute_Internal(Attribute, NewValue);

	FOnGameplayAttributeChange* Delegate = AttributeChangeDelegates.Find(Attribute);
	if (Delegate)
	{
		// We should only have one: either cached CurrentModcallbackData, or explicit callback data passed directly in.
		if (ModData && CurrentModcallbackData)
		{
			ABILITY_LOG(Warning, TEXT("Had passed in ModData and cached CurrentModcallbackData in FActiveGameplayEffectsContainer::InternalUpdateNumericalAttribute. For attribute %s on %s."), *Attribute.GetName(), *Owner->GetFullName() );
		}

		// Broadcast dirty delegate. If we were given explicit mod data then pass it. 
		Delegate->Broadcast(NewValue, ModData ? ModData : CurrentModcallbackData);
	}
	CurrentModcallbackData = nullptr;
}

void FActiveGameplayEffectsContainer::SetAttributeBaseValue(FGameplayAttribute Attribute, float NewBaseValue)
{
	FAggregatorRef* RefPtr = AttributeAggregatorMap.Find(Attribute);
	if (RefPtr)
	{
		// There is an aggregator for this attribute, so set the base value. The dirty callback chain
		// will update the actual AttributeSet property value for us.
		
		const UAttributeSet* Set = Owner->GetAttributeSubobject( Attribute.GetAttributeSetClass() );
		check(Set);

		Set->PreAttributeBaseChange(Attribute, NewBaseValue);
		RefPtr->Get()->SetBaseValue(NewBaseValue);
	}
	else
	{
		// There is no aggregator yet, so we can just set the numeric value directly
		InternalUpdateNumericalAttribute(Attribute, NewBaseValue, nullptr);
	}
}

bool FActiveGameplayEffectsContainer::InternalExecuteMod(FGameplayEffectSpec& Spec, FGameplayModifierEvaluatedData& ModEvalData)
{
	check(Owner);

	bool bExecuted = false;

	UAttributeSet* AttributeSet = nullptr;
	UClass* AttributeSetClass = ModEvalData.Attribute.GetAttributeSetClass();
	if (AttributeSetClass && AttributeSetClass->IsChildOf(UAttributeSet::StaticClass()))
	{
		AttributeSet = const_cast<UAttributeSet*>(Owner->GetAttributeSubobject(AttributeSetClass));
	}

	if (AttributeSet)
	{
		ABILITY_LOG_SCOPE(TEXT("Executing Attribute Mod %s"), *ModEvalData.ToSimpleString());

		FGameplayEffectModCallbackData ExecuteData(Spec, ModEvalData, *Owner);

		/**
		 *  This should apply 'gamewide' rules. Such as clamping Health to MaxHealth or granting +3 health for every point of strength, etc
		 *	PreAttributeModify can return false to 'throw out' this modification.
		 */
		if (AttributeSet->PreGameplayEffectExecute(ExecuteData))
		{
			float OldValueOfProperty = Owner->GetNumericAttribute(ModEvalData.Attribute);
			ApplyModToAttribute(ModEvalData.Attribute, ModEvalData.ModifierOp, ModEvalData.Magnitude, &ExecuteData);

			FGameplayEffectModifiedAttribute* ModifiedAttribute = Spec.GetModifiedAttribute(ModEvalData.Attribute);
			if (!ModifiedAttribute)
			{
				// If we haven't already created a modified attribute holder, create it
				ModifiedAttribute = Spec.AddModifiedAttribute(ModEvalData.Attribute);
			}
			ModifiedAttribute->TotalMagnitude += ModEvalData.Magnitude;

			/** This should apply 'gamewide' rules. Such as clamping Health to MaxHealth or granting +3 health for every point of strength, etc */
			AttributeSet->PostGameplayEffectExecute(ExecuteData);

#if ENABLE_VISUAL_LOG
			DebugExecutedGameplayEffectData DebugData;
			DebugData.GameplayEffectName = Spec.Def->GetName();
			DebugData.ActivationState = "INSTANT";
			DebugData.Attribute = ModEvalData.Attribute;
			DebugData.Magnitude = Owner->GetNumericAttribute(ModEvalData.Attribute) - OldValueOfProperty;
			DebugExecutedGameplayEffects.Add(DebugData);
#endif // ENABLE_VISUAL_LOG

			bExecuted = true;
		}
	}
	else
	{
		// Our owner doesn't have this attribute, so we can't do anything
		ABILITY_LOG(Log, TEXT("%s does not have attribute %s. Skipping modifier"), *Owner->GetPathName(), *ModEvalData.Attribute.GetName());
	}

	return bExecuted;
}

void FActiveGameplayEffectsContainer::ApplyModToAttribute(const FGameplayAttribute &Attribute, TEnumAsByte<EGameplayModOp::Type> ModifierOp, float ModifierMagnitude, const FGameplayEffectModCallbackData* ModData)
{
	// Do we have active GE's that are already modifying this?
	FAggregatorRef* RefPtr = AttributeAggregatorMap.Find(Attribute);
	if (RefPtr)
	{
		ABILITY_LOG(Log, TEXT("Property %s has active mods. Adding to Aggregator."), *Attribute.GetName());
		FAggregator* Agg = RefPtr->Get();
		
		// We must give the attribute set a change to clamp to base value. Otherwise we may haver aggregator base values get 
		// way out of sync with the final uproperty value.
		float NewBase = Agg->GetBaseValue();
		NewBase = FAggregator::StaticExecModOnBaseValue(NewBase, ModifierOp, ModifierMagnitude);
		
		const UAttributeSet* Set = Owner->GetAttributeSubobject( Attribute.GetAttributeSetClass() );
		check(Set);

		// Cache this off here so we dont have to pipe ModData through the agggregator system. This works because the AGE OnDirty will always be called first before any other possible aggregator dependants.
		CurrentModcallbackData = ModData;

		Set->PreAttributeBaseChange(Attribute, NewBase);
		Agg->SetBaseValue(NewBase);

		if (CurrentModcallbackData)
		{
			// We expect this to be cleared for us in InternalUpdateNumericalAttribute
			ABILITY_LOG(Warning, TEXT("FActiveGameplayEffectsContainer::ApplyModToAttribute CurrentModcallbackData was not consumed For attribute %s on %s."), *Attribute.GetName(), *Owner->GetFullName() );
			CurrentModcallbackData = nullptr;
		}
	}
	else
	{
		// Modify the property in place, without putting it in the AttributeAggregatorMap map
		float		CurrentValueOfProperty = Owner->GetNumericAttribute(Attribute);
		const float NewPropertyValue = FAggregator::StaticExecModOnBaseValue(CurrentValueOfProperty, ModifierOp, ModifierMagnitude);

		InternalUpdateNumericalAttribute(Attribute, NewPropertyValue, ModData);
	}
}

FActiveGameplayEffect* FActiveGameplayEffectsContainer::ApplyGameplayEffectSpec(const FGameplayEffectSpec& Spec, FPredictionKey InPredictionKey)
{
	SCOPE_CYCLE_COUNTER(STAT_ApplyGameplayEffectSpec);

	GAMEPLAYEFFECT_SCOPE_LOCK();

	if (Owner && Owner->OwnerActor && IsNetAuthority())
	{
		Owner->OwnerActor->FlushNetDormancy();
	}

	FActiveGameplayEffect* AppliedActiveGE = nullptr;
	FActiveGameplayEffect* ExistingStackableGE = FindStackableActiveGameplayEffect(Spec);

	bool bSetDuration = true;
	bool bSetPeriod = true;

	// Check if there's an active GE this application should stack upon
	if (ExistingStackableGE)
	{
		// Don't allow prediction of stacking for now
		if (!IsNetAuthority())
		{
			return nullptr;
		}

		FGameplayEffectSpec& ExistingSpec = ExistingStackableGE->Spec;
		
		// How to apply multiple stacks at once? What if we trigger an overflow which can reject the application?
		// We still want to apply the stacks that didnt push us over, but we also want to call HandleActiveGameplayEffectStackOverflow.
		
		// For now: call HandleActiveGameplayEffectStackOverflow only if we are ALREADY at the limit. Else we just clamp stack limit to max.
		if (ExistingSpec.StackCount == ExistingSpec.Def->StackLimitCount)
		{
			if (!HandleActiveGameplayEffectStackOverflow(*ExistingStackableGE, ExistingSpec, Spec))
			{
				return nullptr;
			}
		}
		
		int32 NewStackCount = ExistingSpec.StackCount + Spec.StackCount;
		if (ExistingSpec.Def->StackLimitCount > 0)
		{
			NewStackCount = FMath::Min(NewStackCount, ExistingSpec.Def->StackLimitCount);
		}

		// Need to unregister callbacks because the source aggregators could potentially be different with the new application. They will be
		// re-registered later below, as necessary.
		ExistingSpec.CapturedRelevantAttributes.UnregisterLinkedAggregatorCallbacks(ExistingStackableGE->Handle);

		// @todo: If dynamically granted tags differ (which they shouldn't), we'll actually have to diff them
		// and cause a removal and add of only the ones that have changed. For now, ensure on this happening and come
		// back to this later.
		ensureMsgf(ExistingSpec.DynamicGrantedTags == Spec.DynamicGrantedTags, TEXT("While adding a stack of the gameplay effect: %s, the old stack and the new application had different dynamically granted tags, which is currently not resolved properly!"), *Spec.Def->GetName());
		
		// We only grant abilities on the first apply. So we *dont* want the new spec's GrantedAbilitySpecs list
		TArray<FGameplayAbilitySpecDef>	GrantedSpecTempArray(MoveTemp(ExistingStackableGE->Spec.GrantedAbilitySpecs));

		ExistingStackableGE->Spec = Spec;
		ExistingStackableGE->Spec.StackCount = NewStackCount;

		// Swap in old granted ability spec
		ExistingStackableGE->Spec.GrantedAbilitySpecs = MoveTemp(GrantedSpecTempArray);
		
		AppliedActiveGE = ExistingStackableGE;

		const UGameplayEffect* GEDef = ExistingSpec.Def;

		// Make sure the GE actually wants to refresh its duration
		if (GEDef->StackDurationRefreshPolicy == EGameplayEffectStackingDurationPolicy::NeverRefresh)
		{
			bSetDuration = false;
		}
		else
		{
			ExistingStackableGE->StartGameStateTime = GetGameStateTime();
			ExistingStackableGE->CachedStartGameStateTime = ExistingStackableGE->StartGameStateTime;
			ExistingStackableGE->StartWorldTime = GetWorldTime();
		}

		// Make sure the GE actually wants to reset its period
		if (GEDef->StackPeriodResetPolicy == EGameplayEffectStackingPeriodPolicy::NeverReset)
		{
			bSetPeriod = false;
		}
	}
	else
	{
		FActiveGameplayEffectHandle NewHandle = FActiveGameplayEffectHandle::GenerateNewHandle(Owner);

		if (ScopedLockCount > 0 && GameplayEffects_Internal.GetSlack() <= 0)
		{
			/**
			 *	If we have no more slack and we are scope locked, we need to put this addition on our pending GE list, which will be moved
			 *	onto the real active GE list once the scope lock is over.
			 *	
			 *	To avoid extra heap allocations, each active gameplayeffect container keeps a linked list of pending GEs. This list is allocated
			 *	on demand and re-used in subsequent pending adds. The code below will either 1) Alloc a new pending GE 2) reuse an existing pending GE.
			 *	The move operator is used to move stuff to and from these pending GEs to avoid deep copies.
			 */

			check(PendingGameplayEffectNext);
			if (*PendingGameplayEffectNext == nullptr)
			{
				// We have no memory allocated to put our next pending GE, so make a new one.
				// [#1] If you change this, please change #1-3!!!
				AppliedActiveGE = new FActiveGameplayEffect(NewHandle, Spec, GetWorldTime(), GetGameStateTime(), InPredictionKey);
				*PendingGameplayEffectNext = AppliedActiveGE;
			}
			else
			{
				// We already had memory allocated to put a pending GE, move in.
				// [#2] If you change this, please change #1-3!!!
				**PendingGameplayEffectNext = MoveTemp(FActiveGameplayEffect(NewHandle, Spec, GetWorldTime(), GetGameStateTime(), InPredictionKey));
				AppliedActiveGE = *PendingGameplayEffectNext;
			}

			// The next pending GameplayEffect goes to where our PendingNext points
			PendingGameplayEffectNext = &AppliedActiveGE->PendingNext;
		}
		else
		{

			// [#3] If you change this, please change #1-3!!!
			AppliedActiveGE = new(GameplayEffects_Internal) FActiveGameplayEffect(NewHandle, Spec, GetWorldTime(), GetGameStateTime(), InPredictionKey);
		}
	}

	FGameplayEffectSpec& AppliedEffectSpec = AppliedActiveGE->Spec;
	UAbilitySystemGlobals::Get().GlobalPreGameplayEffectSpecApply(AppliedEffectSpec, Owner);

	// Calc all of our modifier magnitudes now. Some may need to update later based on attributes changing, etc, but those should
	// be done through delegate callbacks.
	AppliedEffectSpec.CaptureAttributeDataFromTarget(Owner);
	AppliedEffectSpec.CalculateModifierMagnitudes();

	// Build ModifiedAttribute list so GCs can have magnitude info if non-period effect
	// Note: One day may want to not call gameplay cues unless ongoing tag requirements are met (will need to move this there)
	{
		const bool HasModifiedAttributes = AppliedEffectSpec.ModifiedAttributes.Num() > 0;
		const bool HasDurationAndNoPeriod = AppliedEffectSpec.GetDuration() > 0.0f && AppliedEffectSpec.GetPeriod() == UGameplayEffect::NO_PERIOD;
		const bool HasPeriodAndNoDuration =	AppliedEffectSpec.GetDuration() == UGameplayEffect::INSTANT_APPLICATION && 
											AppliedEffectSpec.GetPeriod() != UGameplayEffect::NO_PERIOD;
		const bool ShouldBuildModifiedAttributeList = !HasModifiedAttributes && (HasDurationAndNoPeriod || HasPeriodAndNoDuration);
		if (ShouldBuildModifiedAttributeList)
		{
			int32 ModifierIndex = -1;
			for (const FGameplayModifierInfo& Mod : AppliedEffectSpec.Def->Modifiers)
			{
				++ModifierIndex;

				// Take magnitude from evaluated magnitudes
				float Magnitude = 0.0f;
				if (AppliedEffectSpec.Modifiers.IsValidIndex(ModifierIndex))
				{
					const FModifierSpec& ModSpec = AppliedEffectSpec.Modifiers[ModifierIndex];
					Magnitude = ModSpec.GetEvaluatedMagnitude();
				}
				
				// Add to ModifiedAttribute list if it doesn't exist already
				FGameplayEffectModifiedAttribute* ModifiedAttribute = AppliedEffectSpec.GetModifiedAttribute(Mod.Attribute);
				if (!ModifiedAttribute)
				{
					ModifiedAttribute = AppliedEffectSpec.AddModifiedAttribute(Mod.Attribute);
				}
				ModifiedAttribute->TotalMagnitude += Magnitude;
			}

			// Prune list since ModifiedAttributes have only been built to
			AppliedEffectSpec.PruneModifiedAttributes();
		}
	}

	// Register Source and Target non snapshot capture delegates here
	AppliedEffectSpec.CapturedRelevantAttributes.RegisterLinkedAggregatorCallbacks(AppliedActiveGE->Handle);
	
	if (bSetDuration)
	{
		// Re-calculate the duration, as it could rely on target captured attributes
		float DefCalcDuration = 0.f;
		if (AppliedEffectSpec.AttemptCalculateDurationFromDef(DefCalcDuration))
		{
			AppliedEffectSpec.SetDuration(DefCalcDuration, false);
		}

		const float DurationBaseValue = AppliedEffectSpec.GetDuration();

		// Calculate Duration mods if we have a real duration
		if (DurationBaseValue > 0.f)
		{
			float FinalDuration = ComputeModifiedDurationOfAppliedSpec(AppliedEffectSpec, DurationBaseValue);

			// We cannot mod ourselves into an instant or infinite duration effect
			if (FinalDuration <= 0.f)
			{
				ABILITY_LOG(Error, TEXT("GameplayEffect %s Duration was modified to %.2f. Clamping to 0.1s duration."), *AppliedEffectSpec.Def->GetName(), FinalDuration);
				FinalDuration = 0.1f;
			}

			AppliedEffectSpec.SetDuration(FinalDuration, true);

			// ABILITY_LOG(Warning, TEXT("SetDuration for %s. Base: %.2f, Final: %.2f"), *NewEffect.Spec.Def->GetName(), DurationBaseValue, FinalDuration);

			// Register duration callbacks with the timer manager
			if (Owner)
			{
				FTimerManager& TimerManager = Owner->GetWorld()->GetTimerManager();
				FTimerDelegate Delegate = FTimerDelegate::CreateUObject(Owner, &UAbilitySystemComponent::CheckDurationExpired, AppliedActiveGE->Handle);
				TimerManager.SetTimer(AppliedActiveGE->DurationHandle, Delegate, FinalDuration, false);
			}
		}
	}
	
	// Register period callbacks with the timer manager
	if (Owner && (AppliedEffectSpec.GetPeriod() != UGameplayEffect::NO_PERIOD))
	{
		FTimerManager& TimerManager = Owner->GetWorld()->GetTimerManager();
		FTimerDelegate Delegate = FTimerDelegate::CreateUObject(Owner, &UAbilitySystemComponent::ExecutePeriodicEffect, AppliedActiveGE->Handle);
			
		// The timer manager moves things from the pending list to the active list after checking the active list on the first tick so we need to execute here
		if (AppliedEffectSpec.Def->bExecutePeriodicEffectOnApplication)
		{
			TimerManager.SetTimerForNextTick(Delegate);
		}

		if (bSetPeriod)
		{
			TimerManager.SetTimer(AppliedActiveGE->PeriodHandle, Delegate, AppliedEffectSpec.GetPeriod(), true);
		}
	}

	if (InPredictionKey.IsLocalClientKey() == false || IsNetAuthority())	// Clients predicting a GameplayEffect must not call MarkItemDirty
	{
		MarkItemDirty(*AppliedActiveGE);
	}
	else
	{
		// Clients predicting should call MarkArrayDirty to force the internal replication map to be rebuilt.
		MarkArrayDirty();

		// Once replicated state has caught up to this prediction key, we must remove this gameplay effect.
		InPredictionKey.NewRejectOrCaughtUpDelegate(FPredictionKeyEvent::CreateUObject(Owner, &UAbilitySystemComponent::RemoveActiveGameplayEffect_NoReturn, AppliedActiveGE->Handle, -1));
		
	}

	// @note @todo: This is currently assuming (potentially incorrectly) that the inhibition state of a stacked GE won't change
	// as a result of stacking. In reality it could in complicated cases with differing sets of dynamically-granted tags.
	if (ExistingStackableGE)
	{
		OnStackCountChange(*ExistingStackableGE);
	}
	else
	{
		InternalOnActiveGameplayEffectAdded(*AppliedActiveGE);
	}

	return AppliedActiveGE;
}

/** This is called anytime a new ActiveGameplayEffect is added, on both client and server in all cases */
void FActiveGameplayEffectsContainer::InternalOnActiveGameplayEffectAdded(FActiveGameplayEffect& Effect)
{
	if (Effect.Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("FActiveGameplayEffectsContainer serialized new GameplayEffect with NULL Def!"));
		return;
	}

	GAMEPLAYEFFECT_SCOPE_LOCK();

	// Add our ongoing tag requirements to the dependency map. We will actually check for these tags below.
	for (const FGameplayTag& Tag : Effect.Spec.Def->OngoingTagRequirements.IgnoreTags)
	{
		ActiveEffectTagDependencies.FindOrAdd(Tag).Add(Effect.Handle);
	}

	for (const FGameplayTag& Tag : Effect.Spec.Def->OngoingTagRequirements.RequireTags)
	{
		ActiveEffectTagDependencies.FindOrAdd(Tag).Add(Effect.Handle);
	}

	// Check if we should actually be turned on or not (this will turn us on for the first time)
	FGameplayTagContainer OwnerTags;
	Owner->GetOwnedGameplayTags(OwnerTags);
	
	Effect.bIsInhibited = true; // Effect has to start inhibited, if it should be uninhibited, CheckOnGoingTagRequirements will handle that state change
	Effect.CheckOngoingTagRequirements(OwnerTags, *this);
}

void FActiveGameplayEffectsContainer::AddActiveGameplayEffectGrantedTagsAndModifiers(FActiveGameplayEffect& Effect, bool bInvokeGameplayCueEvents)
{
	if (Effect.Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("AddActiveGameplayEffectGrantedTagsAndModifiers called with null Def!"));
		return;
	}

	GAMEPLAYEFFECT_SCOPE_LOCK();

	// Register this ActiveGameplayEffects modifiers with our Attribute Aggregators
	if (Effect.Spec.GetPeriod() <= UGameplayEffect::NO_PERIOD)
	{
		for (int32 ModIdx = 0; ModIdx < Effect.Spec.Modifiers.Num(); ++ModIdx)
		{
			const FGameplayModifierInfo &ModInfo = Effect.Spec.Def->Modifiers[ModIdx];

			// Note we assume the EvaluatedMagnitude is up to do. There is no case currently where we should recalculate magnitude based on
			// Ongoing tags being met. We either calculate magnitude one time, or its done via OnDirty calls (or potentially a frequency timer one day)
					
			FAggregator* Aggregator = FindOrCreateAttributeAggregator(Effect.Spec.Def->Modifiers[ModIdx].Attribute).Get();
			Aggregator->AddAggregatorMod(Effect.Spec.GetModifierMagnitude(ModIdx, true), ModInfo.ModifierOp, &ModInfo.SourceTags, &ModInfo.TargetTags, Effect.PredictionKey.WasLocallyGenerated(), Effect.Handle);
		}
	}

	// Update our owner with the tags this GameplayEffect grants them
	Owner->UpdateTagMap(Effect.Spec.Def->InheritableOwnedTagsContainer.CombinedTags, 1);

	Owner->UpdateTagMap(Effect.Spec.DynamicGrantedTags, 1);

	ApplicationImmunityGameplayTagCountContainer.UpdateTagCount(Effect.Spec.Def->GrantedApplicationImmunityTags.RequireTags, 1);
	ApplicationImmunityGameplayTagCountContainer.UpdateTagCount(Effect.Spec.Def->GrantedApplicationImmunityTags.IgnoreTags, 1);

	for (const FGameplayEffectCue& Cue : Effect.Spec.Def->GameplayCues)
	{
		Owner->UpdateTagMap(Cue.GameplayCueTags, 1);

		if (bInvokeGameplayCueEvents)
		{
			// TODO: Optimize this so only one batched RPC is called
			for (auto It = Cue.GameplayCueTags.CreateConstIterator(); It; ++It)
			{
				Owner->AddGameplayCue(*It);
			}
		}
	}
}

/** Called on server to remove a GameplayEffect */
bool FActiveGameplayEffectsContainer::RemoveActiveGameplayEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove)
{
	// Iterating through manually since this is a removal operation and we need to pass the index into InternalRemoveActiveGameplayEffect
	int32 NumGameplayEffects = GetNumGameplayEffects();
	for (int32 ActiveGEIdx = 0; ActiveGEIdx < NumGameplayEffects; ++ActiveGEIdx)
	{
		FActiveGameplayEffect& Effect = *GetActiveGameplayEffect(ActiveGEIdx);
		if (Effect.Handle == Handle && Effect.IsPendingRemove == false)
		{
			if (UE_LOG_ACTIVE(VLogAbilitySystem, Log))
			{
				ABILITY_VLOG(Owner->OwnerActor, Log, TEXT("Removed %s"), *Effect.Spec.Def->GetFName().ToString());
				for (FGameplayModifierInfo Modifier : Effect.Spec.Def->Modifiers)
				{
					float Magnitude = 0.f;
					Modifier.ModifierMagnitude.AttemptCalculateMagnitude(Effect.Spec, Magnitude);
					ABILITY_VLOG(Owner->OwnerActor, Log, TEXT("         %s: %s %f"), *Modifier.Attribute.GetName(), *EGameplayModOpToString(Modifier.ModifierOp), Magnitude);
				}
			}

			InternalRemoveActiveGameplayEffect(ActiveGEIdx, StacksToRemove, true);
			return true;
		}
	}
	ABILITY_LOG(Log, TEXT("RemoveActiveGameplayEffect called with invalid Handle: %s"), *Handle.ToString());
	return false;
}

/** Called by server to actually remove a GameplayEffect */
bool FActiveGameplayEffectsContainer::InternalRemoveActiveGameplayEffect(int32 Idx, int32 StacksToRemove, bool bPrematureRemoval)
{
	SCOPE_CYCLE_COUNTER(STAT_RemoveActiveGameplayEffect);

	bool IsLocked = (ScopedLockCount > 0);	// Cache off whether we were previously locked
	GAMEPLAYEFFECT_SCOPE_LOCK();			// Apply lock so no one else can change the AGE list (we may still change it if IsLocked is false)
	
	if (ensure(Idx < GetNumGameplayEffects()))
	{
		FActiveGameplayEffect& Effect = *GetActiveGameplayEffect(Idx);
		ensure(!Effect.IsPendingRemove);

		ABILITY_LOG(Verbose, TEXT("InternalRemoveActiveGameplayEffect: Auth: %s Handle: %s Def: %s"), IsNetAuthority() ? TEXT("TRUE") : TEXT("FALSE"), *Effect.Handle.ToString(), Effect.Spec.Def ? *Effect.Spec.Def->GetName() : TEXT("NONE"));

		if (StacksToRemove > 0 && Effect.Spec.StackCount > StacksToRemove)
		{
			// This won't be a full remove, only a change in StackCount.
			Effect.Spec.StackCount -= StacksToRemove;
			OnStackCountChange(Effect);
			return false;
		}

		// Mark the effect as pending removal
		Effect.IsPendingRemove = true;

		// Remove Granted Abilities
		for (FGameplayAbilitySpecDef& AbilitySpecDef : Effect.Spec.GrantedAbilitySpecs)
		{
			if (AbilitySpecDef.AssignedHandle.IsValid())
			{
				switch(AbilitySpecDef.RemovalPolicy)
				{
				case EGameplayEffectGrantedAbilityRemovePolicy::CancelAbilityImmediately:
					Owner->ClearAbility(AbilitySpecDef.AssignedHandle);
					break;
				case EGameplayEffectGrantedAbilityRemovePolicy::RemoveAbilityOnEnd:
					Owner->SetRemoveAbilityOnEnd(AbilitySpecDef.AssignedHandle);
					break;
				}
			}
		}

		// Invoke Remove GameplayCue event
		bool ShouldInvokeGameplayCueEvent = true;
		const bool bIsNetAuthority = IsNetAuthority();
		if (!bIsNetAuthority && Effect.PredictionKey.IsLocalClientKey() && Effect.PredictionKey.WasReceived() == false)
		{
			// This was an effect that we predicted. Don't invoke GameplayCue event if we have another GameplayEffect that shares the same predictionkey and was received from the server
			if (HasReceivedEffectWithPredictedKey(Effect.PredictionKey))
			{
				ShouldInvokeGameplayCueEvent = false;
			}
		}

		// Don't invoke the GC event if the effect is inhibited, and thus the GC is already not active
		ShouldInvokeGameplayCueEvent &= !Effect.bIsInhibited;

		if (ShouldInvokeGameplayCueEvent)
		{
			Owner->InvokeGameplayCueEvent(Effect.Spec, EGameplayCueEvent::Removed);
		}

		InternalOnActiveGameplayEffectRemoved(Effect);

		if (Effect.DurationHandle.IsValid())
		{
			Owner->GetWorld()->GetTimerManager().ClearTimer(Effect.DurationHandle);
		}
		if (Effect.PeriodHandle.IsValid())
		{
			Owner->GetWorld()->GetTimerManager().ClearTimer(Effect.PeriodHandle);
		}

		if (bIsNetAuthority && Owner->OwnerActor)
		{
			Owner->OwnerActor->FlushNetDormancy();
		}

		// Remove this handle from the global map
		Effect.Handle.RemoveFromGlobalMap();

		// Attempt to apply expiration effects, if necessary
		InternalApplyExpirationEffects(Effect.Spec, bPrematureRemoval);

		bool ModifiedArray = false;

		// Finally remove the ActiveGameplayEffect
		if (IsLocked)
		{
			// We are locked, so this removal is now pending.
			PendingRemoves++;

			ABILITY_LOG(Verbose, TEXT("InternalRemoveActiveGameplayEffect while locked; Counting as a Pending Remove: Auth: %s Handle: %s Def: %s"), IsNetAuthority() ? TEXT("TRUE") : TEXT("FALSE"), *Effect.Handle.ToString(), Effect.Spec.Def ? *Effect.Spec.Def->GetName() : TEXT("NONE"));
		}
		else
		{
			// Not locked, so do the removal right away.

			// If we are not scope locked, then there is no way this idx should be referring to something on the pending add list.
			// It is possible to remove a GE that is pending add, but it would happen while the scope lock is still in effect, resulting
			// in a pending remove being set.
			check(Idx < GameplayEffects_Internal.Num());

			GameplayEffects_Internal.RemoveAtSwap(Idx);
			ModifiedArray = true;
		}

		MarkArrayDirty();

		// Hack: force netupdate on owner. This isn't really necessary in real gameplay but is nice
		// during debugging where breakpoints or pausing can mess up network update times. Open issue
		// with network team.
		Owner->GetOwner()->ForceNetUpdate();
		
		return ModifiedArray;
	}

	ABILITY_LOG(Warning, TEXT("InternalRemoveActiveGameplayEffect called with invalid index: %d"), Idx);
	return false;
}

/** Called by client and server: This does cleanup that has to happen whether the effect is being removed locally or due to replication */
void FActiveGameplayEffectsContainer::InternalOnActiveGameplayEffectRemoved(const FActiveGameplayEffect& Effect)
{
	if (Effect.Spec.Def)
	{
		// Remove our tag requirements from the dependency map
		RemoveActiveEffectTagDependency(Effect.Spec.Def->OngoingTagRequirements.IgnoreTags, Effect.Handle);
		RemoveActiveEffectTagDependency(Effect.Spec.Def->OngoingTagRequirements.RequireTags, Effect.Handle);

		RemoveActiveGameplayEffectGrantedTagsAndModifiers(Effect, !Effect.bIsInhibited);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("InternalOnActiveGameplayEffectRemoved called with no GameplayEffect: %s"), *Effect.Handle.ToString());
	}

	Effect.OnRemovedDelegate.Broadcast();
	OnActiveGameplayEffectRemovedDelegate.Broadcast();
}

void FActiveGameplayEffectsContainer::RemoveActiveGameplayEffectGrantedTagsAndModifiers(const FActiveGameplayEffect& Effect, bool bInvokeGameplayCueEvents)
{
	// Update AttributeAggregators: remove mods from this ActiveGE Handle
	if (Effect.Spec.GetPeriod() <= UGameplayEffect::NO_PERIOD)
	{
		for(const FGameplayModifierInfo& Mod : Effect.Spec.Def->Modifiers)
		{
			if(Mod.Attribute.IsValid())
			{
				FAggregatorRef* RefPtr = AttributeAggregatorMap.Find(Mod.Attribute);
				if(RefPtr)
				{
					RefPtr->Get()->RemoveAggregatorMod(Effect.Handle);
				}
			}
		}
	}

	// Update gameplaytag count and broadcast delegate if we are at 0
	IGameplayTagsModule& GameplayTagsModule = IGameplayTagsModule::Get();
	Owner->UpdateTagMap(Effect.Spec.Def->InheritableOwnedTagsContainer.CombinedTags, -1);

	ApplicationImmunityGameplayTagCountContainer.UpdateTagCount(Effect.Spec.Def->GrantedApplicationImmunityTags.RequireTags, -1);
	ApplicationImmunityGameplayTagCountContainer.UpdateTagCount(Effect.Spec.Def->GrantedApplicationImmunityTags.IgnoreTags, -1);

	Owner->UpdateTagMap(Effect.Spec.DynamicGrantedTags, -1);

	for (const FGameplayEffectCue& Cue : Effect.Spec.Def->GameplayCues)
	{
		Owner->UpdateTagMap(Cue.GameplayCueTags, -1);

		if (bInvokeGameplayCueEvents)
		{
			// TODO: Optimize this so only one batched RPC is called
			for (auto It = Cue.GameplayCueTags.CreateConstIterator(); It; ++It)
			{
				Owner->RemoveGameplayCue(*It);
			}
		}
	}
}

void FActiveGameplayEffectsContainer::RemoveActiveEffectTagDependency(const FGameplayTagContainer& Tags, FActiveGameplayEffectHandle Handle)
{
	for(const FGameplayTag& Tag : Tags)
	{
		auto Ptr = ActiveEffectTagDependencies.Find(Tag);
		if (Ptr)
		{
			Ptr->Remove(Handle);
			if (Ptr->Num() <= 0)
			{
				ActiveEffectTagDependencies.Remove(Tag);
			}
		}
	}
}

void FActiveGameplayEffectsContainer::InternalApplyExpirationEffects(const FGameplayEffectSpec& ExpiringSpec, bool bPrematureRemoval)
{
	GAMEPLAYEFFECT_SCOPE_LOCK();

	check(Owner);

	// Don't allow prediction of expiration effects
	if (IsNetAuthority())
	{
		const UGameplayEffect* ExpiringGE = ExpiringSpec.Def;
		if (ExpiringGE)
		{
			// Determine the appropriate type of effect to apply depending on whether the effect is being prematurely removed or not
			const TArray<TSubclassOf<UGameplayEffect>>& ExpiryEffects = (bPrematureRemoval ? ExpiringGE->PrematureExpirationEffectClasses : ExpiringGE->RoutineExpirationEffectClasses);

			for (const TSubclassOf<UGameplayEffect>& CurExpiryEffect : ExpiryEffects)
			{
				if (CurExpiryEffect)
				{
					const UGameplayEffect* CurExpiryCDO = CurExpiryEffect->GetDefaultObject<UGameplayEffect>();
					check(CurExpiryCDO);
									
					const FGameplayEffectContextHandle& ExpiringSpecContextHandle = ExpiringSpec.GetEffectContext();
					FGameplayEffectContextHandle NewContextHandle = FGameplayEffectContextHandle(UAbilitySystemGlobals::Get().AllocGameplayEffectContext());
					
					// Pass along old instigator to new effect context
					// @todo: Creation of this spec needs to include tags, etc. from the original spec as well
					if (NewContextHandle.IsValid())
					{
						NewContextHandle.AddInstigator(ExpiringSpecContextHandle.GetInstigator(), ExpiringSpecContextHandle.GetEffectCauser());
					}
				
					FGameplayEffectSpec NewExpirySpec(CurExpiryCDO, NewContextHandle, ExpiringSpec.GetLevel());
					Owner->ApplyGameplayEffectSpecToSelf(NewExpirySpec);
				}
			}
		}
	}
}

void FActiveGameplayEffectsContainer::OnOwnerTagChange(FGameplayTag TagChange, int32 NewCount)
{
	// It may be beneficial to do a scoped lock on attribute re-evaluation during this function

	auto Ptr = ActiveEffectTagDependencies.Find(TagChange);
	if (Ptr)
	{
		GAMEPLAYEFFECT_SCOPE_LOCK();

		FGameplayTagContainer OwnerTags;
		Owner->GetOwnedGameplayTags(OwnerTags);

		TSet<FActiveGameplayEffectHandle>& Handles = *Ptr;
		for (const FActiveGameplayEffectHandle& Handle : Handles)
		{
			FActiveGameplayEffect* ActiveEffect = GetActiveGameplayEffect(Handle);
			if (ActiveEffect)
			{
				ActiveEffect->CheckOngoingTagRequirements(OwnerTags, *this, true);
			}
		}
	}
}

bool FActiveGameplayEffectsContainer::HasApplicationImmunityToSpec(const FGameplayEffectSpec& SpecToApply) const
{
	SCOPE_CYCLE_COUNTER(STAT_HasApplicationImmunityToSpec)

	const FGameplayTagContainer* AggregatedSourceTags = SpecToApply.CapturedSourceTags.GetAggregatedTags();
	if (!ensure(AggregatedSourceTags))
	{
		return false;
	}

	// Quick map test
	if (!AggregatedSourceTags->MatchesAny(ApplicationImmunityGameplayTagCountContainer.GetExplicitGameplayTags(), false))
	{
		return false;
	}

	for (const FActiveGameplayEffect& Effect :  this)
	{
		if (Effect.Spec.Def->GrantedApplicationImmunityTags.IsEmpty() == false && Effect.Spec.Def->GrantedApplicationImmunityTags.RequirementsMet( *AggregatedSourceTags ))
		{
			return true;
		}
	}

	return false;
}

bool FActiveGameplayEffectsContainer::IsNetAuthority() const
{
	check(Owner);
	return Owner->IsOwnerActorAuthoritative();
}

bool FActiveGameplayEffectsContainer::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
{
	bool RetVal = FastArrayDeltaSerialize<FActiveGameplayEffect>(GameplayEffects_Internal, DeltaParms, *this);

	// After the array has been replicated, invoke GC events ONLY if the effect is not inhibited
	// We postpone this check because in the same net update we could receive multiple GEs that affect if one another is inhibited
	
	if (DeltaParms.Writer == nullptr && Owner != nullptr)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ActiveGameplayEffectsContainer_NetDeltaSerialize_CheckRepGameplayCues);

		for (const FActiveGameplayEffect& Effect : this)
		{
			if (Effect.bIsInhibited == false)
			{
				if (Effect.bPendingRepOnActiveGC)
				{
					Owner->InvokeGameplayCueEvent(Effect.Spec, EGameplayCueEvent::OnActive);
					Effect.bPendingRepOnActiveGC = false;
				}
				if (Effect.bPendingRepWhileActiveGC)
				{
					Owner->InvokeGameplayCueEvent(Effect.Spec, EGameplayCueEvent::WhileActive);
					Effect.bPendingRepWhileActiveGC = false;
				}
			}
		}
	}

	return RetVal;
}

void FActiveGameplayEffectsContainer::PreDestroy()
{
	
}

int32 FActiveGameplayEffectsContainer::GetGameStateTime() const
{
	UWorld* World = Owner->GetWorld();
	AGameState* GameState = World->GetGameState<AGameState>();
	if (GameState)
	{
		return GameState->ElapsedTime;
	}

	return static_cast<int32>(World->GetTimeSeconds());
}

float FActiveGameplayEffectsContainer::GetWorldTime() const
{
	UWorld *World = Owner->GetWorld();
	return World->GetTimeSeconds();
}

void FActiveGameplayEffectsContainer::CheckDuration(FActiveGameplayEffectHandle Handle)
{
	GAMEPLAYEFFECT_SCOPE_LOCK();
	// Intentionally iterating through only the internal list since we need to pass the index for removal
	// and pending effecets will never need to be checked for duration expiration (They will be added to the real list first)
	for (int32 ActiveGEIdx = 0; ActiveGEIdx < GameplayEffects_Internal.Num(); ++ActiveGEIdx)
	{
		FActiveGameplayEffect& Effect = GameplayEffects_Internal[ActiveGEIdx];
		if (Effect.Handle == Handle && Effect.IsPendingRemove == false)
		{
			FTimerManager& TimerManager = Owner->GetWorld()->GetTimerManager();

			// The duration may have changed since we registered this callback with the timer manager.
			// Make sure that this effect should really be destroyed now
			float Duration = Effect.GetDuration();
			float CurrentTime = GetWorldTime();

			if (Duration > 0.f && (((Effect.StartWorldTime + Duration) < CurrentTime) || FMath::IsNearlyZero(CurrentTime - Duration - Effect.StartWorldTime, KINDA_SMALL_NUMBER)))
			{
				// This gameplay effect has hit its duration. Check if it needs to execute one last time before removing it.
				if (Effect.PeriodHandle.IsValid() && TimerManager.TimerExists(Effect.PeriodHandle))
				{
					float PeriodTimeRemaining = TimerManager.GetTimerRemaining(Effect.PeriodHandle);
					if (PeriodTimeRemaining <= KINDA_SMALL_NUMBER)
					{
						ExecuteActiveEffectsFrom(Effect.Spec);
					}

					// Forcibly clear the periodic ticks because this effect is going to be removed
					TimerManager.ClearTimer(Effect.PeriodHandle);
				}

				InternalRemoveActiveGameplayEffect(ActiveGEIdx, -1, false);
			}
			else
			{
				// Always reset the timer, since the duration might have been modified
				FTimerDelegate Delegate = FTimerDelegate::CreateUObject(Owner, &UAbilitySystemComponent::CheckDurationExpired, Effect.Handle);
				TimerManager.SetTimer(Effect.DurationHandle, Delegate, (Effect.StartWorldTime + Duration) - CurrentTime, false);
			}

			break;
		}
	}
}

bool FActiveGameplayEffectsContainer::CanApplyAttributeModifiers(const UGameplayEffect* GameplayEffect, float Level, const FGameplayEffectContextHandle& EffectContext)
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsCanApplyAttributeModifiers);

	FGameplayEffectSpec	Spec(GameplayEffect, EffectContext, Level);

	Spec.CalculateModifierMagnitudes();
	
	for(int32 ModIdx = 0; ModIdx < Spec.Modifiers.Num(); ++ModIdx)
	{
		const FGameplayModifierInfo& ModDef = Spec.Def->Modifiers[ModIdx];
		const FModifierSpec& ModSpec = Spec.Modifiers[ModIdx];
	
		// It only makes sense to check additive operators
		if (ModDef.ModifierOp == EGameplayModOp::Additive)
		{
			if (!ModDef.Attribute.IsValid())
			{
				continue;
			}
			const UAttributeSet* Set = Owner->GetAttributeSubobject(ModDef.Attribute.GetAttributeSetClass());
			float CurrentValue = ModDef.Attribute.GetNumericValueChecked(Set);
			float CostValue = ModSpec.GetEvaluatedMagnitude();

			if (CurrentValue + CostValue < 0.f)
			{
				return false;
			}
		}
	}
	return true;
}

TArray<float> FActiveGameplayEffectsContainer::GetActiveEffectsTimeRemaining(const FActiveGameplayEffectQuery Query) const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsGetActiveEffectsTimeRemaining);

	float CurrentTime = GetWorldTime();

	TArray<float>	ReturnList;

	for (const FActiveGameplayEffect& Effect : this)
	{
		if (!Query.Matches(Effect))
		{
			continue;
		}

		float Elapsed = CurrentTime - Effect.StartWorldTime;
		float Duration = Effect.GetDuration();

		ReturnList.Add(Duration - Elapsed);
	}

	// Note: keep one return location to avoid copy operation.
	return ReturnList;
}

TArray<float> FActiveGameplayEffectsContainer::GetActiveEffectsDuration(const FActiveGameplayEffectQuery Query) const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsGetActiveEffectsDuration);

	TArray<float>	ReturnList;

	for (const FActiveGameplayEffect& Effect : this)
	{
		if (!Query.Matches(Effect))
		{
			continue;
		}

		ReturnList.Add(Effect.GetDuration());
	}

	// Note: keep one return location to avoid copy operation.
	return ReturnList;
}

TArray<FActiveGameplayEffectHandle> FActiveGameplayEffectsContainer::GetActiveEffects(const FActiveGameplayEffectQuery Query) const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsGetActiveEffects);

	TArray<FActiveGameplayEffectHandle> ReturnList;

	for (const FActiveGameplayEffect& Effect : this)
	{
		if (!Query.Matches(Effect))
		{
			continue;
		}

		ReturnList.Add(Effect.Handle);
	}

	return ReturnList;
}

void FActiveGameplayEffectsContainer::ModifyActiveEffectStartTime(FActiveGameplayEffectHandle Handle, float StartTimeDiff)
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsModifyActiveEffectStartTime);

	FActiveGameplayEffect* Effect = GetActiveGameplayEffect(Handle);

	if (Effect)
	{
		Effect->StartWorldTime += StartTimeDiff;
		Effect->StartGameStateTime = FMath::RoundToInt((float)Effect->StartGameStateTime + StartTimeDiff);

		CheckDuration(Handle);

		MarkItemDirty(*Effect);
	}
}

void FActiveGameplayEffectsContainer::RemoveActiveEffects(const FActiveGameplayEffectQuery Query, int32 StacksToRemove)
{
	// Force a lock because the removals could cause other removals earlier in the array, so iterating backwards is not safe all by itself
	GAMEPLAYEFFECT_SCOPE_LOCK();

	// Manually iterating through in reverse because this is a removal operation
	for (int32 idx=GetNumGameplayEffects()-1; idx >= 0; --idx)
	{
		const FActiveGameplayEffect& Effect = *GetActiveGameplayEffect(idx);
		if (Effect.IsPendingRemove==false && Query.Matches(Effect))
		{
			InternalRemoveActiveGameplayEffect(idx, StacksToRemove, true);			
		}
	}
}

FOnGameplayAttributeChange& FActiveGameplayEffectsContainer::RegisterGameplayAttributeEvent(FGameplayAttribute Attribute)
{
	return AttributeChangeDelegates.FindOrAdd(Attribute);
}

bool FActiveGameplayEffectsContainer::HasReceivedEffectWithPredictedKey(FPredictionKey PredictionKey) const
{
	for (const FActiveGameplayEffect& Effect : this)
	{
		if (Effect.PredictionKey == PredictionKey && Effect.PredictionKey.WasReceived() == true)
		{
			return true;
		}
	}

	return false;
}

bool FActiveGameplayEffectsContainer::HasPredictedEffectWithPredictedKey(FPredictionKey PredictionKey) const
{
	for (const FActiveGameplayEffect& Effect : this)
	{
		if (Effect.PredictionKey == PredictionKey && Effect.PredictionKey.WasReceived() == false)
		{
			return true;
		}
	}

	return false;
}

void FActiveGameplayEffectsContainer::GetActiveGameplayEffectDataByAttribute(TMultiMap<FGameplayAttribute, FActiveGameplayEffectsContainer::DebugExecutedGameplayEffectData>& EffectMap) const
{
	EffectMap.Empty();

	// Add all of the active gameplay effects
	for (const FActiveGameplayEffect& Effect : this)
	{
		if (Effect.Spec.Modifiers.Num() == Effect.Spec.Def->Modifiers.Num())
		{
			for (int32 Idx = 0; Idx < Effect.Spec.Modifiers.Num(); ++Idx)
			{
				FActiveGameplayEffectsContainer::DebugExecutedGameplayEffectData Data;
				Data.Attribute = Effect.Spec.Def->Modifiers[Idx].Attribute;
				Data.ActivationState = Effect.bIsInhibited ? TEXT("INHIBITED") : TEXT("ACTIVE");
				Data.GameplayEffectName = Effect.Spec.Def->GetName();
				Data.ModifierOp = Effect.Spec.Def->Modifiers[Idx].ModifierOp;
				Data.Magnitude = Effect.Spec.Modifiers[Idx].GetEvaluatedMagnitude();
				if (Effect.Spec.StackCount > 1)
				{
					Data.Magnitude = GameplayEffectUtilities::ComputeStackedModifierMagnitude(Data.Magnitude, Effect.Spec.StackCount, Data.ModifierOp);
				}
				Data.StackCount = Effect.Spec.StackCount;

				EffectMap.Add(Data.Attribute, Data);
			}
		}
	}
#if ENABLE_VISUAL_LOG
	// Add the executed gameplay effects if we recorded them
	for (FActiveGameplayEffectsContainer::DebugExecutedGameplayEffectData Data : DebugExecutedGameplayEffects)
	{
		EffectMap.Add(Data.Attribute, Data);
	}
#endif // ENABLE_VISUAL_LOG
}

#if ENABLE_VISUAL_LOG
void FActiveGameplayEffectsContainer::GrabDebugSnapshot(FVisualLogEntry* Snapshot) const
{
	FVisualLogStatusCategory ActiveEffectsCategory;
	ActiveEffectsCategory.Category = TEXT("Effects");

	TMultiMap<FGameplayAttribute, FActiveGameplayEffectsContainer::DebugExecutedGameplayEffectData> EffectMap;

	GetActiveGameplayEffectDataByAttribute(EffectMap);

	// For each attribute that was modified go through all of its modifiers and list them
	TArray<FGameplayAttribute> AttributeKeys;
	EffectMap.GetKeys(AttributeKeys);

	for (const FGameplayAttribute& Attribute : AttributeKeys)
	{
		float CombinedModifierValue = 0.f;
		ActiveEffectsCategory.Add(TEXT(" --- Attribute --- "), Attribute.GetName());

		TArray<FActiveGameplayEffectsContainer::DebugExecutedGameplayEffectData> AttributeEffects;
		EffectMap.MultiFind(Attribute, AttributeEffects);

		for (const FActiveGameplayEffectsContainer::DebugExecutedGameplayEffectData& DebugData : AttributeEffects)
		{
			ActiveEffectsCategory.Add(DebugData.GameplayEffectName, DebugData.ActivationState);
			ActiveEffectsCategory.Add(TEXT("Magnitude"), FString::Printf(TEXT("%f"), DebugData.Magnitude));

			if (DebugData.ActivationState != "INHIBITED")
			{
				CombinedModifierValue += DebugData.Magnitude;
			}
		}

		ActiveEffectsCategory.Add(TEXT("Total Modification"), FString::Printf(TEXT("%f"), CombinedModifierValue));
	}

	Snapshot->Status.Add(ActiveEffectsCategory);
}
#endif // ENABLE_VISUAL_LOG

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	Misc
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

namespace GlobalActiveGameplayEffectHandles
{
	static TMap<FActiveGameplayEffectHandle, TWeakObjectPtr<UAbilitySystemComponent>>	Map;
}

FActiveGameplayEffectHandle FActiveGameplayEffectHandle::GenerateNewHandle(UAbilitySystemComponent* OwningComponent)
{
	static int32 GHandleID=0;
	FActiveGameplayEffectHandle NewHandle(GHandleID++);

	TWeakObjectPtr<UAbilitySystemComponent> WeakPtr(OwningComponent);

	GlobalActiveGameplayEffectHandles::Map.Add(NewHandle, WeakPtr);

	return NewHandle;
}

UAbilitySystemComponent* FActiveGameplayEffectHandle::GetOwningAbilitySystemComponent()
{
	TWeakObjectPtr<UAbilitySystemComponent>* Ptr = GlobalActiveGameplayEffectHandles::Map.Find(*this);
	if (Ptr)
	{
		return Ptr->Get();
	}

	return nullptr;	
}

const UAbilitySystemComponent* FActiveGameplayEffectHandle::GetOwningAbilitySystemComponent() const
{
	TWeakObjectPtr<UAbilitySystemComponent>* Ptr = GlobalActiveGameplayEffectHandles::Map.Find(*this);
	if (Ptr)
	{
		return Ptr->Get();
	}

	return nullptr;
}

void FActiveGameplayEffectHandle::RemoveFromGlobalMap()
{
	GlobalActiveGameplayEffectHandles::Map.Remove(*this);
}

// -----------------------------------------------------------------

bool FActiveGameplayEffectQuery::Matches(const FActiveGameplayEffect& Effect) const
{
	// Anything in the ignore handle list is an immediate non-match
	if (IgnoreHandles.Contains(Effect.Handle))
	{
		return false;
	}

	if (CustomMatch.IsBound())
	{
		return CustomMatch.Execute(Effect);
	}

	// if we are looking for owning tags check them on the Granted Tags and Owned Tags Container
	if (OwningTagContainer)
	{
		if (!Effect.Spec.Def->InheritableOwnedTagsContainer.CombinedTags.MatchesAny(*OwningTagContainer, true) &&
			!Effect.Spec.DynamicGrantedTags.MatchesAny(*OwningTagContainer, false))
		{
			// if the GameplayEffect didn't match check the spec for tags that were added when this effect was created
			if (!Effect.Spec.CapturedSourceTags.GetSpecTags().MatchesAny(*OwningTagContainer, false))
			{
				return false;
			}
		}
	}	
	
	// if we are just looking for Tags on the Effect then look at the Gameplay Effect Tags
	if (EffectTagContainer)
	{
		if (!Effect.Spec.Def->InheritableGameplayEffectTags.CombinedTags.MatchesAny(*EffectTagContainer, true))
		{
			// this doesn't match our Tags so bail
			return false;
		}
	}

	// if we are just looking for Tags on the Effect then look at the Gameplay Effect Tags
	if (EffectTagContainer_Rejection)
	{
		if (Effect.Spec.Def->InheritableGameplayEffectTags.CombinedTags.MatchesAny(*EffectTagContainer_Rejection, true))
		{
			// this matches our Rejection Tags so bail
			return false;
		}
	}

	// if we are looking for ModifyingAttribute go over each of the Spec Modifiers and check the Attributes
	if (ModifyingAttribute.IsValid())
	{
		bool FailedModifyingAttributeCheck = true;

		for(int32 ModIdx = 0; ModIdx < Effect.Spec.Modifiers.Num(); ++ModIdx)
		{
			const FGameplayModifierInfo& ModDef = Effect.Spec.Def->Modifiers[ModIdx];
			const FModifierSpec& ModSpec = Effect.Spec.Modifiers[ModIdx];

			if (ModDef.Attribute == ModifyingAttribute)
			{
				FailedModifyingAttributeCheck = false;
				break;
			}
		}
		if (FailedModifyingAttributeCheck)
		{
			return false;
		}
	}

	// check source object
	if (EffectSource)
	{
		if (Effect.Spec.GetEffectContext().GetSourceObject() != EffectSource)
		{
			return false;
		}
	}

	// check definition
	if (EffectDef)
	{
		if (Effect.Spec.Def != EffectDef)
		{
			return false;
		}
	}

	// passed all the checks
	return true;
}

void FInheritedTagContainer::UpdateInheritedTagProperties(const FInheritedTagContainer* Parent)
{
	// Make sure we've got a fresh start
	CombinedTags.RemoveAllTags();

	// Re-add the Parent's tags except the one's we have removed
	if (Parent)
	{
		for (auto Itr = Parent->CombinedTags.CreateConstIterator(); Itr; ++Itr)
		{
			if (!Removed.HasTag(*Itr, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::IncludeParentTags))
			{
				CombinedTags.AddTag(*Itr);
			}
		}
	}

	// Add our own tags
	for (auto Itr = Added.CreateConstIterator(); Itr; ++Itr)
	{
		// Remove trumps add for explicit matches but not for parent tags.
		// This lets us remove all inherited tags starting with Foo but still add Foo.Bar
		if (!Removed.HasTag(*Itr, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit))
		{
			CombinedTags.AddTag(*Itr);
		}
	}
}

void FInheritedTagContainer::PostInitProperties()
{
	// we shouldn't inherit the added and removed tags from our parents
	// make sure that these fields are clear
	Added.RemoveAllTags();
	Removed.RemoveAllTags();
}

void FInheritedTagContainer::AddTag(const FGameplayTag& TagToAdd)
{
	CombinedTags.AddTag(TagToAdd);
}

void FInheritedTagContainer::RemoveTag(FGameplayTag TagToRemove)
{
	CombinedTags.RemoveTag(TagToRemove);
}

void FActiveGameplayEffectsContainer::IncrementLock()
{
	ScopedLockCount++;
}

void FActiveGameplayEffectsContainer::DecrementLock()
{
	if (--ScopedLockCount == 0)
	{
		// ------------------------------------------
		// Move any pending effects onto the real list
		// ------------------------------------------
		FActiveGameplayEffect* PendingGameplayEffect = PendingGameplayEffectHead;
		FActiveGameplayEffect* Stop = *PendingGameplayEffectNext;
		while (PendingGameplayEffect != Stop)
		{
			if (!PendingGameplayEffect->IsPendingRemove)
			{
				GameplayEffects_Internal.Add(MoveTemp(*PendingGameplayEffect));
			}
			PendingGameplayEffect = PendingGameplayEffect->PendingNext;
		}

		// Reset our pending GameplayEffect linked list
		PendingGameplayEffectNext = &PendingGameplayEffectHead;

		// -----------------------------------------
		// Delete any pending remove effects
		// -----------------------------------------
		for (int32 idx=GameplayEffects_Internal.Num()-1; idx >= 0 && PendingRemoves > 0; --idx)
		{
			FActiveGameplayEffect& Effect = GameplayEffects_Internal[idx];

			if (Effect.IsPendingRemove)
			{
				ABILITY_LOG(Verbose, TEXT("DecrementLock decrementing a pending remove: Auth: %s Handle: %s Def: %s"), IsNetAuthority() ? TEXT("TRUE") : TEXT("FALSE"), *Effect.Handle.ToString(), Effect.Spec.Def ? *Effect.Spec.Def->GetName() : TEXT("NONE"));
				GameplayEffects_Internal.RemoveAtSwap(idx, 1, false);
				PendingRemoves--;
			}
		}

		if (!ensure(PendingRemoves == 0))
		{
			ABILITY_LOG(Error, TEXT("~FScopedActiveGameplayEffectLock has %d pending removes after a scope lock removal"), PendingRemoves);
			PendingRemoves = 0;
		}
	}
}

FScopedActiveGameplayEffectLock::FScopedActiveGameplayEffectLock(FActiveGameplayEffectsContainer& InContainer)
	: Container(InContainer)
{
	Container.IncrementLock();
}

FScopedActiveGameplayEffectLock::~FScopedActiveGameplayEffectLock()
{
	Container.DecrementLock();
}
