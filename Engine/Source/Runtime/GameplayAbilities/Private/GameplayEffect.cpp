// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffectStackingExtension.h"
#include "GameplayTagsModule.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayModMagnitudeCalculation.h"
#include "GameplayEffectExecutionCalculation.h"

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
	ChanceToApplyToTarget.SetValue(1.f);
	ChanceToExecuteOnGameplayEffect.SetValue(1.f);
	StackingPolicy = EGameplayEffectStackingPolicy::Unlimited;
	StackedAttribName = NAME_None;

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

	if (Duration.Curve.CurveTable != nullptr || Duration.Value != 0)
	{
		if (Duration.Value == INFINITE_DURATION)
		{
			DurationPolicy = EGameplayEffectDurationType::Infinite;
		}
		else if (Duration.Value == INSTANT_APPLICATION)
		{
			DurationPolicy = EGameplayEffectDurationType::Instant;
		}
		else
		{
			DurationPolicy = EGameplayEffectDurationType::HasDuration;
		}

		DurationMagnitude.ScalableFloatMagnitude = Duration;
		DurationMagnitude.MagnitudeCalculationType = EGameplayEffectMagnitudeCalculation::ScalableFloat;

		Duration.Curve.CurveTable = nullptr;
		Duration.Value = 0;
	}
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
				OutCalculatedMagnitude = InRelevantSpec.GetMagnitude(SetByCallerMagnitude.DataName);
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

bool FGameplayEffectModifierMagnitude::AttempteRecalculateMagnitudeFromDependantChange(const FGameplayEffectSpec& InRelevantSpec, OUT float& OutCalculatedMagnitude, const FAggregator* ChangedAggregator) const
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

FGameplayEffectSpec::FGameplayEffectSpec(const UGameplayEffect* InDef, const FGameplayEffectContextHandle& InEffectContext, float Level)
: Def(InDef)
{
	check(Def);	
	SetLevel(Level);
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
		TargetEffectSpecs.Add(FGameplayEffectSpecHandle(new FGameplayEffectSpec(TargetDef, EffectContext, Level)));
	}

	// Everything is setup now, capture data from our source
	CaptureDataFromSource();
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

void FGameplayEffectSpec::CaptureDataFromSource()
{
	// Capture source actor tags
	CapturedSourceTags.GetActorTags().RemoveAllTags();
	EffectContext.GetOwnedGameplayTags(CapturedSourceTags.GetActorTags());

	// Capture source Attributes
	// Is this the right place to do it? Do we ever need to create spec and capture attributes at a later time? If so, this will need to move.
	CapturedRelevantAttributes.CaptureAttributes(EffectContext.GetInstigatorAbilitySystemComponent(), EGameplayEffectAttributeCaptureSource::Source);

	// Now that we have source attributes captured, re-evaluate the duration since it could be based on the captured attributes.
	RecalculateDuration();
}

void FGameplayEffectSpec::RecalculateDuration()
{
	check(Def);

	if (Def->DurationPolicy == EGameplayEffectDurationType::Infinite)
	{
		SetDuration(UGameplayEffect::INFINITE_DURATION);
	}
	else if (Def->DurationPolicy == EGameplayEffectDurationType::Instant)
	{
		SetDuration(UGameplayEffect::INSTANT_APPLICATION);
	}
	else
	{
		float NewDuration = 0.f;
		if (Def->DurationMagnitude.AttemptCalculateMagnitude(*this, NewDuration))
		{
			SetDuration(NewDuration);
		}
	}
}

void FGameplayEffectSpec::SetLevel(float InLevel)
{
	Level = InLevel;
	if (Def)
	{
		RecalculateDuration();
		
		Period = Def->Period.GetValueAtLevel(InLevel);
		ChanceToApplyToTarget = Def->ChanceToApplyToTarget.GetValueAtLevel(InLevel);
		ChanceToExecuteOnGameplayEffect = Def->ChanceToExecuteOnGameplayEffect.GetValueAtLevel(InLevel);
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

void FGameplayEffectSpec::SetDuration(float NewDuration)
{
	Duration = NewDuration;
	if (Duration > 0.f)
	{
		// We may  have potential problems one day if a game is applying duration based gameplay effects from instantaneous effects
		// (E.g., everytime fire damage is applied, a DOT is also applied). We may need to for Duration to always be capturd.
		CapturedRelevantAttributes.AddCaptureDefinition(UAbilitySystemComponent::GetOutgoingDurationCapture());
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

float FGameplayEffectSpec::GetChanceToExecuteOnGameplayEffect() const
{
	return ChanceToExecuteOnGameplayEffect;
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

float FGameplayEffectSpec::GetMagnitude(const FGameplayAttribute &Attribute) const
{
	for (int32 ModIdx = 0; ModIdx < Modifiers.Num(); ++ModIdx)
	{
		const FGameplayModifierInfo& ModDef = Def->Modifiers[ModIdx];
		const FModifierSpec& ModSpec = Modifiers[ModIdx];
	
		if (ModDef.Attribute != Attribute)
		{
			continue;
		}

		return ModSpec.EvaluatedMagnitude;
	}

	return 0.f;
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

EGameplayEffectStackingPolicy::Type FGameplayEffectSpec::GetStackingType() const
{
	return Def->StackingPolicy;
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

void FGameplayEffectSpec::SetMagnitude(FName DataName, float Magnitude)
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

float FGameplayEffectSpec::GetMagnitude(FName DataName) const
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
			Agg->AddDependant(Handle);
		}
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

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FActiveGameplayEffect
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------


/** This is the core function that turns the ActiveGE 'on' or 'off */
void FActiveGameplayEffect::CheckOngoingTagRequirements(const FGameplayTagContainer& OwnerTags, FActiveGameplayEffectsContainer& OwningContainer)
{
	bool ShouldBeInhibited = !Spec.Def->OngoingTagRequirements.RequirementsMet(OwnerTags);

	if (IsInhibited != ShouldBeInhibited)
	{
		// All OnDirty callbacks must be inhibited until we update this entire GameplayEffect.
		FScopedAggregatorOnDirtyBatch	AggregatorOnDirtyBatcher;

		if (ShouldBeInhibited)
		{
			// Remove our ActiveGameplayEffects modifiers with our Attribute Aggregators
			OwningContainer.RemoveActiveGameplayEffectGrantedTagsAndModifiers(*this);
		}
		else
		{
			OwningContainer.AddActiveGameplayEffectGrantedTagsAndModifiers(*this);
		}

		IsInhibited = ShouldBeInhibited;
	}
}

bool FActiveGameplayEffect::CanBeStacked(const FActiveGameplayEffect& Other) const
{
	return (Handle != Other.Handle) && Spec.GetStackingType() == Other.Spec.GetStackingType() && Spec.Def->Modifiers[0].Attribute == Other.Spec.Def->Modifiers[0].Attribute;
}

void FActiveGameplayEffect::PreReplicatedRemove(const struct FActiveGameplayEffectsContainer &InArray)
{
	if (Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("Received PreReplicatedRemove with no UGameplayEffect def."));
		return;
	}

	const_cast<FActiveGameplayEffectsContainer&>(InArray).InternalOnActiveGameplayEffectRemoved(*this);	// Const cast is ok. It is there to prevent mutation of the GameplayEffects array, which this wont do.
	
	InArray.Owner->InvokeGameplayCueEvent(Spec, EGameplayCueEvent::Removed);
}

void FActiveGameplayEffect::PostReplicatedAdd(const struct FActiveGameplayEffectsContainer &InArray)
{
	if (Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("Received ReplicatedGameplayEffect with no UGameplayEffect def."));
		return;
	}

	bool ShouldInvokeGameplayCueEvents = true;
	if (PredictionKey.IsValidKey())
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

	if (ShouldInvokeGameplayCueEvents)
	{
		InArray.Owner->InvokeGameplayCueEvent(Spec, EGameplayCueEvent::WhileActive);
	}

	// Was this actually just activated, or are we just finding out about it due to relevancy/join in progress?
	float WorldTimeSeconds = InArray.GetWorldTime();
	int32 GameStateTime = InArray.GetGameStateTime();

	int32 DeltaGameStateTime = GameStateTime - StartGameStateTime;	// How long we think the effect has been playing (only 1 second precision!)

	if (ShouldInvokeGameplayCueEvents && GameStateTime > 0 && FMath::Abs(DeltaGameStateTime) < MAX_DELTA_TIME)
	{
		InArray.Owner->InvokeGameplayCueEvent(Spec, EGameplayCueEvent::OnActive);
	}

	// Set our local start time accordingly
	StartWorldTime = WorldTimeSeconds - static_cast<float>(DeltaGameStateTime);
}

void FActiveGameplayEffect::PostReplicatedChange(const struct FActiveGameplayEffectsContainer &InArray)
{
	if (Spec.Def == nullptr)
	{
		ABILITY_LOG(Error, TEXT("Received ReplicatedGameplayEffect with no UGameplayEffect def."));
	}
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FActiveGameplayEffectsContainer
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

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
	// If there are no modifiers, we always want to apply the GameplayCue. If there are modifiers, we only want to invoke the GameplayCue if one of them went through (could be blocked by immunity or % chance roll)
	bool InvokeGameplayCueExecute = (Spec.Modifiers.Num() == 0);

	// check if this is a stacking effect and if it is the active stacking effect.
	if (Spec.GetStackingType() != EGameplayEffectStackingPolicy::Unlimited)
	{
		// we're a stacking attribute but not active
		if (Spec.bTopOfStack == false)
		{
			return;
		}
	}

	// The process of executing a gameplay spec may cause modifications to the spec, however those modifications
	// should not persist across multiple executions on the same spec. Therefore, if the spec isn't instantaneous, make a 
	// copy that can be modified and pass it along to each part of the execution process.
	TSharedPtr<FGameplayEffectSpec> StackSpec;
	if (Spec.GetDuration() != UGameplayEffect::INSTANT_APPLICATION)
	{
		StackSpec = TSharedPtr<FGameplayEffectSpec>(new FGameplayEffectSpec(Spec));
	}
	FGameplayEffectSpec& SpecToUse = StackSpec.IsValid() ? *(StackSpec.Get()) : Spec;

	float ChanceToExecute = SpecToUse.GetChanceToExecuteOnGameplayEffect();		// Not implemented? Should we just remove ChanceToExecute?

	// Capture our own tags.
	// TODO: We should only capture them if we need to. We may have snapshotted target tags (?) (in the case of dots with exotic setups?)

	SpecToUse.CapturedTargetTags.GetActorTags().RemoveAllTags();
	Owner->GetOwnedGameplayTags(SpecToUse.CapturedTargetTags.GetActorTags());

	SpecToUse.CalculateModifierMagnitudes();

	// ------------------------------------------------------
	//	Modifiers
	//		These will modify the base value of attributes
	// ------------------------------------------------------

	for (int32 ModIdx = 0; ModIdx < SpecToUse.Modifiers.Num(); ++ModIdx)
	{
		const FGameplayModifierInfo& ModDef = SpecToUse.Def->Modifiers[ModIdx];
		const FModifierSpec& ModSpec = SpecToUse.Modifiers[ModIdx];

		FGameplayModifierEvaluatedData EvalData(ModDef.Attribute, ModDef.ModifierOp, ModSpec.GetEvaluatedMagnitude());
		InvokeGameplayCueExecute |= InternalExecuteMod(SpecToUse, EvalData);
	}

	// ------------------------------------------------------
	//	Executions
	//		This will run custom code to 'do stuff'
	// ------------------------------------------------------
	
	for (const FGameplayEffectExecutionDefinition& CurExecDef : SpecToUse.Def->Executions)
	{
		if (CurExecDef.CalculationClass)
		{
			const UGameplayEffectExecutionCalculation* ExecCDO = CurExecDef.CalculationClass->GetDefaultObject<UGameplayEffectExecutionCalculation>();
			check(ExecCDO);

			TArray<FGameplayModifierEvaluatedData> OutModifiers;

			// Run the custom execution
			FGameplayEffectCustomExecutionParameters ExecutionParams(SpecToUse, CurExecDef.CalculationModifiers, Owner);
			ExecCDO->Execute(ExecutionParams, OutModifiers);

			// Execute any mods the custom execution yielded
			for (FGameplayModifierEvaluatedData& CurExecMod : OutModifiers)
			{
				InvokeGameplayCueExecute |= InternalExecuteMod(SpecToUse, CurExecMod);
			}
		}
	}

	// ------------------------------------------------------
	//	Invoke GameplayCue events
	// ------------------------------------------------------
	
	// Prune the modified attributes before we replicate
	SpecToUse.PruneModifiedAttributes();

	if (InvokeGameplayCueExecute && SpecToUse.Def->GameplayCues.Num())
	{
		// TODO: check replication policy. Right now we will replicate every execute via a multicast RPC

		ABILITY_LOG(Log, TEXT("Invoking Execute GameplayCue for %s"), *SpecToUse.ToSimpleString());
		Owner->ForceReplication();
		Owner->NetMulticast_InvokeGameplayCueExecuted_FromSpec(SpecToUse, PredictionKey);
	}
}

void FActiveGameplayEffectsContainer::ExecutePeriodicGameplayEffect(FActiveGameplayEffectHandle Handle)
{
	FScopedActiveGameplayEffectLock AGELock;
	FActiveGameplayEffect* ActiveEffect = GetActiveGameplayEffect(Handle);
	if (ActiveEffect)
	{
		// Execute
		ExecuteActiveEffectsFrom(ActiveEffect->Spec);
	}
}

FActiveGameplayEffect* FActiveGameplayEffectsContainer::GetActiveGameplayEffect(const FActiveGameplayEffectHandle Handle)
{
	// Could make this a map for quicker lookup
	for (FActiveGameplayEffect& Effect : GameplayEffects)
	{
		if (Effect.Handle == Handle)
		{
			return &Effect;
		}
	}
	return nullptr;
}

int32 FScopedActiveGameplayEffectLock::AGELockCount = 0;
TArray<TSharedPtr<FActiveGameplayEffectAction>> FScopedActiveGameplayEffectLock::DeferredAGEActions;
FScopedActiveGameplayEffectLock::FScopedActiveGameplayEffectLock()
{
	++AGELockCount;
}

FScopedActiveGameplayEffectLock::~FScopedActiveGameplayEffectLock()
{
	--AGELockCount;
	if (!IsLockInEffect())
	{
		for (int32 i = 0; i < NumActions(); ++i)
		{
			GetAction(i)->PerformAction();
		}
		ClearActions();
	}
}

FAggregatorRef& FActiveGameplayEffectsContainer::FindOrCreateAttributeAggregator(FGameplayAttribute Attribute)
{
	FAggregatorRef *RefPtr = AttributeAggregatorMap.Find(Attribute);
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

void FActiveGameplayEffectsContainer::OnMagnitudeDependancyChange(FActiveGameplayEffectHandle Handle, const FAggregator* ChangedAgg)
{
	if (Handle.IsValid())
	{
		FActiveGameplayEffect* ActiveEffect = GetActiveGameplayEffect(Handle);
		if (ActiveEffect)
		{
			// This handle registered with the ChangedAgg to be notified when the aggregator changed.
			// At this point we don't know what actually needs to be updated inside this active gameplay effect.
			FGameplayEffectSpec& Spec = ActiveEffect->Spec;

			// We must update attribute aggregators only if we are actually 'on' right now, and if we are non periodic (periodic effects do their thing on execute callbacks)
			bool MustUpdateAttributeAggregators = (ActiveEffect->IsInhibited == false && (Spec.GetDuration() <= UGameplayEffect::NO_PERIOD));

			// As we update our modifier magnitudes, we will update our owner's attribute aggregators. When we do this, we have to clear them first of all of our (Handle's) previous mods.
			// Since we could potentially have two mods to the same attribute, one that gets updated, and one that doesnt - we need to do this in two passes.
			TSet<FGameplayAttribute>	AttributesToUpdate;

			// First pass: update magnitudes of our modifiers that changed
			for(int32 ModIdx = 0; ModIdx < Spec.Modifiers.Num(); ++ModIdx)
			{
				const FGameplayModifierInfo& ModDef = Spec.Def->Modifiers[ModIdx];
				FModifierSpec& ModSpec = Spec.Modifiers[ModIdx];

				if (ModDef.ModifierMagnitude.AttempteRecalculateMagnitudeFromDependantChange(Spec, ModSpec.EvaluatedMagnitude, ChangedAgg))
				{
					// We changed, so we need to reapply/update our spot in the attribute aggregator map
					if (MustUpdateAttributeAggregators)
					{
						AttributesToUpdate.Add(ModDef.Attribute);
					}
				}
			}

			// Second pass, update the aggregators that we need to
			for (const FGameplayAttribute& Attribute : AttributesToUpdate)
			{
				FAggregator* Aggregator = FindOrCreateAttributeAggregator(Attribute).Get();
				check(Aggregator);

				// Remove ALL of our mods (this is all we can do! We don't keep track of aggregator mods <=> spec mod mappings)
				Aggregator->RemoveMod(Handle);

				// Now readd ALL of our mods (even if they weren't recalculated here)
				for(int32 ModIdx = 0; ModIdx < Spec.Modifiers.Num(); ++ModIdx)
				{
					const FGameplayModifierInfo& ModDef = Spec.Def->Modifiers[ModIdx];
					FModifierSpec& ModSpec = Spec.Modifiers[ModIdx];

					if (ModDef.Attribute == Attribute)
					{
						Aggregator->AddMod(ModSpec.GetEvaluatedMagnitude(), ModDef.ModifierOp, &ModDef.SourceTags, &ModDef.TargetTags, ActiveEffect->PredictionKey.WasLocallyGenerated(), Handle);
					}
				}
			}
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

float FActiveGameplayEffectsContainer::GetGameplayEffectDuration(FActiveGameplayEffectHandle Handle) const
{
	// Could make this a map for quicker lookup
	for (int32 idx = 0; idx < GameplayEffects.Num(); ++idx)
	{
		if (GameplayEffects[idx].Handle == Handle)
		{
			return GameplayEffects[idx].GetDuration();
		}
	}
	
	ABILITY_LOG(Warning, TEXT("GetGameplayEffectDuration called with invalid Handle: %s"), *Handle.ToString() );
	return UGameplayEffect::INFINITE_DURATION;
}

float FActiveGameplayEffectsContainer::GetGameplayEffectMagnitude(FActiveGameplayEffectHandle Handle, FGameplayAttribute Attribute) const
{
	// Could make this a map for quicker lookup
	for (FActiveGameplayEffect Effect : GameplayEffects)
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

bool FActiveGameplayEffectsContainer::IsGameplayEffectActive(FActiveGameplayEffectHandle Handle, bool IncludeEffectsBlockedByStackingRules) const
{
	// Could make this a map for quicker lookup
	for (const FActiveGameplayEffect& Effect : GameplayEffects)
	{
		if (Effect.Handle == Handle)
		{
			// stacking effects may return false
			if (Effect.Spec.GetStackingType() != EGameplayEffectStackingPolicy::Unlimited &&
				Effect.Spec.bTopOfStack == false)
			{
				return IncludeEffectsBlockedByStackingRules;
			}
			return true;
		}
	}

	return false;
}

const FGameplayTagContainer* FActiveGameplayEffectsContainer::GetGameplayEffectSourceTagsFromHandle(FActiveGameplayEffectHandle Handle) const
{
	// @todo: Need to consider this with tag changes
	for (const FActiveGameplayEffect& Effect : GameplayEffects)
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
	for (const FActiveGameplayEffect& Effect : GameplayEffects)
	{
		if (Effect.Handle == Handle)
		{
			return Effect.Spec.CapturedTargetTags.GetAggregatedTags();
		}
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
		Delegate->Broadcast(NewValue, ModData);
	}
}

void FActiveGameplayEffectsContainer::SetAttributeBaseValue(FGameplayAttribute Attribute, float NewBaseValue)
{
	FAggregatorRef* RefPtr = AttributeAggregatorMap.Find(Attribute);
	if (RefPtr)
	{
		// There is an aggregator for this attribute, so set the base value. The dirty callback chain
		// will update the actual AttributeSet property value for us.
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
	bool bExecuted = false;

	UAttributeSet* AttributeSet = const_cast<UAttributeSet*>(Owner->GetAttributeSubobject(ModEvalData.Attribute.GetAttributeSetClass()));
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
		RefPtr->Get()->ExecModOnBaseValue(ModifierOp, ModifierMagnitude);
	}
	else
	{
		// Modify the property in place, without putting it in the AttributeAggregatorMap map
		float		CurrentValueOfProperty = Owner->GetNumericAttribute(Attribute);
		const float NewPropertyValue = FAggregator::StaticExecModOnBaseValue(CurrentValueOfProperty, ModifierOp, ModifierMagnitude);

		InternalUpdateNumericalAttribute(Attribute, NewPropertyValue, ModData);
	}
}

void FActiveGameplayEffectsContainer::StacksNeedToRecalculate()
{
	if (!bNeedToRecalculateStacks)
	{
		bNeedToRecalculateStacks = true;
		FTimerManager& TimerManager = Owner->GetWorld()->GetTimerManager();
		FTimerDelegate Delegate = FTimerDelegate::CreateUObject(Owner, &UAbilitySystemComponent::OnRestackGameplayEffects);

		TimerManager.SetTimerForNextTick(Delegate);
	}
}

/**	This Creates a new ActiveGameplayEffect and is only called on the Server or in cases where a client predictively applies a GameplayEffect. */
FActiveGameplayEffect& FActiveGameplayEffectsContainer::CreateNewActiveGameplayEffect(const FGameplayEffectSpec &Spec, FPredictionKey InPredictionKey)
{
	SCOPE_CYCLE_COUNTER(STAT_CreateNewActiveGameplayEffect);
	
	if (Owner && Owner->OwnerActor)
	{
		if ( IsNetAuthority() )
		{
			Owner->OwnerActor->FlushNetDormancy();
		}
	}

	FActiveGameplayEffectHandle NewHandle = FActiveGameplayEffectHandle::GenerateNewHandle(Owner);
	FActiveGameplayEffect& NewEffect = *new (GameplayEffects)FActiveGameplayEffect(NewHandle, Spec, GetWorldTime(), GetGameStateTime(), InPredictionKey);

	UAbilitySystemGlobals::Get().GlobalPreGameplayEffectSpecApply(NewEffect.Spec, Owner);

	// Calc all of our modifier magnitudes now. Some may need to update later based on attributes changing, etc, but those should
	// be done through delegate callbacks.
	NewEffect.Spec.CapturedRelevantAttributes.CaptureAttributes(Owner, EGameplayEffectAttributeCaptureSource::Target);
	NewEffect.Spec.CalculateModifierMagnitudes();

	// Register Source and Target non snapshot capture delegates here
	NewEffect.Spec.CapturedRelevantAttributes.RegisterLinkedAggregatorCallbacks(NewHandle);

	// Calculate Duration mods if we have a real duration
	float DurationBaseValue = NewEffect.Spec.GetDuration();
	if (DurationBaseValue > 0.f)
	{
		FAggregator DurationAgg;

		const FGameplayEffectAttributeCaptureSpec* OutgoingCaptureSpec = NewEffect.Spec.CapturedRelevantAttributes.FindCaptureSpecByDefinition(UAbilitySystemComponent::GetOutgoingDurationCapture(), true);
		if (OutgoingCaptureSpec)
		{
			OutgoingCaptureSpec->AttemptAddAggregatorModsToAggregator(DurationAgg);
		}

		const FGameplayEffectAttributeCaptureSpec* IncomingCaptureSpec = NewEffect.Spec.CapturedRelevantAttributes.FindCaptureSpecByDefinition(UAbilitySystemComponent::GetIncomingDurationCapture(), true);
		if (IncomingCaptureSpec)
		{
			IncomingCaptureSpec->AttemptAddAggregatorModsToAggregator(DurationAgg);
		}

		FAggregatorEvaluateParameters Params;
		Params.SourceTags = NewEffect.Spec.CapturedSourceTags.GetAggregatedTags();
		Params.TargetTags = NewEffect.Spec.CapturedTargetTags.GetAggregatedTags();

		float FinalDuration = DurationAgg.EvaluateWithBase(DurationBaseValue, Params);

		// We cannot mod ourselves into an instant or infinite duration effect
		if (FinalDuration <= 0.f)
		{
			ABILITY_LOG(Error, TEXT("GameplayEffect %s Duration was modified to %.2f. Clamping to 0.1s duration."), *NewEffect.Spec.Def->GetName(), FinalDuration);
			FinalDuration = 0.1f;
		}

		NewEffect.Spec.SetDuration(FinalDuration);

		// ABILITY_LOG(Warning, TEXT("SetDuration for %s. Base: %.2f, Final: %.2f"), *NewEffect.Spec.Def->GetName(), DurationBaseValue, FinalDuration);
	}
	
	// register callbacks with the timer manager
	if (Owner)
	{
		if (NewEffect.Spec.GetDuration() > 0.f)
		{
			FTimerManager& TimerManager = Owner->GetWorld()->GetTimerManager();
			FTimerDelegate Delegate = FTimerDelegate::CreateUObject(Owner, &UAbilitySystemComponent::CheckDurationExpired, NewHandle);
			TimerManager.SetTimer(NewEffect.DurationHandle, Delegate, NewEffect.Spec.GetDuration(), false, NewEffect.Spec.GetDuration());
		}
		// The timer manager moves things from the pending list to the active list after checking the active list on the first tick so we need to execute here
		if (NewEffect.Spec.GetPeriod() != UGameplayEffect::NO_PERIOD)
		{
			FTimerManager& TimerManager = Owner->GetWorld()->GetTimerManager();
			FTimerDelegate Delegate = FTimerDelegate::CreateUObject(Owner, &UAbilitySystemComponent::ExecutePeriodicEffect, NewHandle);

			// If this is a periodic stacking effect, make sure that it's in sync with the others.
			float FirstDelay = -1.f;
			bool bApplyToNextTick = true;
			if (NewEffect.Spec.GetStackingType() != EGameplayEffectStackingPolicy::Unlimited)
			{
				for (FActiveGameplayEffect& Effect : GameplayEffects)
				{
					if (Effect.CanBeStacked(NewEffect))
					{
						FirstDelay = TimerManager.GetTimerRemaining(Effect.PeriodHandle);
						bApplyToNextTick = TimerManager.IsTimerPending(Effect.PeriodHandle);
						break;
					}
				}
			}
			
			if (bApplyToNextTick)
			{
				TimerManager.SetTimerForNextTick(Delegate); // not part of a stack or the first item on the stack, execute once right away
			}

			TimerManager.SetTimer(NewEffect.PeriodHandle, Delegate, NewEffect.Spec.GetPeriod(), true, FirstDelay); // this is going to be off by a frame for stacking because of the pending list
		}
	}
	
	if (InPredictionKey.IsValidKey() == false || IsNetAuthority())	// Clients predicting a GameplayEffect must not call MarkItemDirty
	{
		MarkItemDirty(NewEffect);
	}
	else
	{
		// Clients predicting should call MarkArrayDirty to force the internal replication map to be rebuilt.
		MarkArrayDirty();

		// Once replicated state has caught up to this prediction key, we must remove this gameplay effect.
		InPredictionKey.NewRejectOrCaughtUpDelegate(FPredictionKeyEvent::CreateUObject(Owner, &UAbilitySystemComponent::RemoveActiveGameplayEffect_NoReturn, NewEffect.Handle));
		
	}

	InternalOnActiveGameplayEffectAdded(NewEffect);

	return NewEffect;
}

/** This is called anytime a new ActiveGameplayEffect is added, on both client and server in all cases */
void FActiveGameplayEffectsContainer::InternalOnActiveGameplayEffectAdded(FActiveGameplayEffect& Effect)
{
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
	
	Effect.IsInhibited = true; // Effect has to start inhibited, if it should be uninhibited, CheckOnGoingTagRequirements will handle that state change
	Effect.CheckOngoingTagRequirements(OwnerTags, *this);
}

void FActiveGameplayEffectsContainer::AddActiveGameplayEffectGrantedTagsAndModifiers(FActiveGameplayEffect& Effect)
{
	// Register this ActiveGameplayEffects modifiers with our Attribute Aggregators
	if (Effect.Spec.GetPeriod() <= UGameplayEffect::NO_PERIOD)
	{
		for (int32 ModIdx = 0; ModIdx < Effect.Spec.Modifiers.Num(); ++ModIdx)
		{
			const FModifierSpec &Mod = Effect.Spec.Modifiers[ModIdx];
			const FGameplayModifierInfo &ModInfo = Effect.Spec.Def->Modifiers[ModIdx];

			// Note we assume the EvaluatedMagnitude is up to do. There is no case currently where we should recalculate magnitude based on
			// Ongoing tags being met. We either calculate magnitude one time, or its done via OnDirty calls (or potentially a frequency timer one day)
					
			FAggregator* Aggregator = FindOrCreateAttributeAggregator(Effect.Spec.Def->Modifiers[ModIdx].Attribute).Get();
			Aggregator->AddMod(Mod.GetEvaluatedMagnitude(), ModInfo.ModifierOp, &ModInfo.SourceTags, &ModInfo.TargetTags, Effect.PredictionKey.WasLocallyGenerated(), Effect.Handle);
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
	}
}

/** Called on server to remove a GameplayEffect */
bool FActiveGameplayEffectsContainer::RemoveActiveGameplayEffect(FActiveGameplayEffectHandle Handle)
{
	// Could make this a map for quicker lookup
	for (int32 Idx = 0; Idx < GameplayEffects.Num(); ++Idx)
	{
		if (GameplayEffects[Idx].Handle == Handle)
		{
			//Block removal if we're scope-locked
			if (FScopedActiveGameplayEffectLock::IsLockInEffect())
			{
				FScopedActiveGameplayEffectLock::AddAction()->InitForRemoveGE(TWeakObjectPtr<UAbilitySystemComponent>(Owner), Handle);
				return true;		//We are assuming the "ensure(Idx < GameplayEffects.Num())" would be passed.
			}
			return InternalRemoveActiveGameplayEffect(Idx);
		}
	}
	ABILITY_LOG(Warning, TEXT("RemoveActiveGameplayEffect called with invalid Handle: %s"), *Handle.ToString());
	return false;
}

/** Called by server to actually remove a GameplayEffect */
bool FActiveGameplayEffectsContainer::InternalRemoveActiveGameplayEffect(int32 Idx)
{
	SCOPE_CYCLE_COUNTER(STAT_RemoveActiveGameplayEffect);

	if (ensure(Idx < GameplayEffects.Num()))
	{
		FActiveGameplayEffect& Effect = GameplayEffects[Idx];

		check(!FScopedActiveGameplayEffectLock::IsLockInEffect());

		bool ShouldInvokeGameplayCueEvent = true;
		const bool bIsNetAuthority = IsNetAuthority();
		if (!bIsNetAuthority && Effect.PredictionKey.IsValidKey() && Effect.PredictionKey.WasReceived() == false)
		{
			// This was an effect that we predicted. Don't invoke GameplayCue event if we have another GameplayEffect that shares the same predictionkey and was received from the server
			if (HasReceivedEffectWithPredictedKey(Effect.PredictionKey))
			{
				ShouldInvokeGameplayCueEvent = false;
			}
		}

		if (ShouldInvokeGameplayCueEvent)
		{
			Owner->InvokeGameplayCueEvent(GameplayEffects[Idx].Spec, EGameplayCueEvent::Removed);
		}

		InternalOnActiveGameplayEffectRemoved(Effect);

		if (GameplayEffects[Idx].DurationHandle.IsValid())
		{
			Owner->GetWorld()->GetTimerManager().ClearTimer(GameplayEffects[Idx].DurationHandle);
		}
		if (GameplayEffects[Idx].PeriodHandle.IsValid())
		{
			Owner->GetWorld()->GetTimerManager().ClearTimer(GameplayEffects[Idx].PeriodHandle);
		}

		if (bIsNetAuthority && Owner->OwnerActor)
		{
			Owner->OwnerActor->FlushNetDormancy();
		}

		// Remove this handle from the global map
		Effect.Handle.RemoveFromGlobalMap();

		// Finally remove the ActiveGameplayEffect
		GameplayEffects.RemoveAtSwap(Idx);
		MarkArrayDirty();

		// Hack: force netupdate on owner. This isn't really necessary in real gameplay but is nice
		// during debugging where breakpoints or pausing can mess up network update times. Open issue
		// with network team.
		Owner->GetOwner()->ForceNetUpdate();

		StacksNeedToRecalculate();
		return true;
	}

	ABILITY_LOG(Warning, TEXT("InternalRemoveActiveGameplayEffect called with invalid index: %d"), Idx);
	return false;
}

/** Called by client and server: This does cleanup that has to happen whether the effect is being removed locally or due to replication */
void FActiveGameplayEffectsContainer::InternalOnActiveGameplayEffectRemoved(const FActiveGameplayEffect& Effect)
{
	// Remove our tag requirements from the dependancy map
	RemoveActiveEffectTagDependancy(Effect.Spec.Def->OngoingTagRequirements.IgnoreTags, Effect.Handle);
	RemoveActiveEffectTagDependancy(Effect.Spec.Def->OngoingTagRequirements.RequireTags, Effect.Handle);

	if (Effect.Spec.Def)
	{
		RemoveActiveGameplayEffectGrantedTagsAndModifiers(Effect);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("InternalOnActiveGameplayEffectRemoved called with no GameplayEffect: %s"), *Effect.Handle.ToString());
	}

	Effect.OnRemovedDelegate.Broadcast();
}

void FActiveGameplayEffectsContainer::RemoveActiveGameplayEffectGrantedTagsAndModifiers(const FActiveGameplayEffect& Effect)
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
					RefPtr->Get()->RemoveMod(Effect.Handle);
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
	}
}

void FActiveGameplayEffectsContainer::RemoveActiveEffectTagDependancy(const FGameplayTagContainer& Tags, FActiveGameplayEffectHandle Handle)
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

void FActiveGameplayEffectsContainer::OnOwnerTagChange(FGameplayTag TagChange, int32 NewCount)
{
	// It may be beneficial to do a scoped lock on attribute reeevalluation during this dunction

	auto Ptr = ActiveEffectTagDependencies.Find(TagChange);
	if (Ptr)
	{
		FGameplayTagContainer OwnerTags;
		Owner->GetOwnedGameplayTags(OwnerTags);

		TSet<FActiveGameplayEffectHandle>& Handles = *Ptr;
		for (const FActiveGameplayEffectHandle& Handle : Handles)
		{
			FActiveGameplayEffect* ActiveEffect = GetActiveGameplayEffect(Handle);
			if (ActiveEffect)
			{
				ActiveEffect->CheckOngoingTagRequirements(OwnerTags, *this);
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

	for (const FActiveGameplayEffect& Effect : GameplayEffects)
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

void FActiveGameplayEffectsContainer::PreDestroy()
{
	
}

int32 FActiveGameplayEffectsContainer::GetGameStateTime() const
{
	UWorld *World = Owner->GetWorld();
	AGameState * GameState = World->GetGameState<AGameState>();
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
	for (int32 Idx = 0; Idx < GameplayEffects.Num(); ++Idx)
	{
		FActiveGameplayEffect& Effect = GameplayEffects[Idx];
		if (Effect.Handle == Handle)
		{
			FTimerManager& TimerManager = Owner->GetWorld()->GetTimerManager();

			// The duration may have changed since we registered this callback with the timer manager.
			// Make sure that this effect should really be destroyed now
			float Duration = Effect.GetDuration();
			float CurrentTime = GetWorldTime();
			if (Duration > 0.f && (Effect.StartWorldTime + Duration) < CurrentTime)
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

				InternalRemoveActiveGameplayEffect(Idx);
			}
			else
			{
				// check the time remaining for the current gameplay effect duration timer
				// if it is less than zero create a new callback with the correct time remaining
				float TimeRemaining = TimerManager.GetTimerRemaining(Effect.DurationHandle);
				if (TimeRemaining <= 0.f)
				{
					FTimerDelegate Delegate = FTimerDelegate::CreateUObject(Owner, &UAbilitySystemComponent::CheckDurationExpired, Effect.Handle);
					TimerManager.SetTimer(Effect.DurationHandle, Delegate, (Effect.StartWorldTime + Duration) - CurrentTime, false);
				}
			}

			break;
		}
	}
}

void FActiveGameplayEffectsContainer::RecalculateStacking()
{
	bNeedToRecalculateStacks = false;

	// one or more gameplay effects has been removed or added so we need to go through all of them to see what the best elements are in each stack
	TArray<FActiveGameplayEffect*> StackedEffects;
	TArray<FActiveGameplayEffect*> CustomStackedEffects;

	for (FActiveGameplayEffect& Effect : GameplayEffects)
	{
		// ignore effects that don't stack and effects that replace when they stack
		// effects that replace when they stack only need to be handled when they are added
		if (Effect.Spec.GetStackingType() != EGameplayEffectStackingPolicy::Unlimited &&
			Effect.Spec.GetStackingType() != EGameplayEffectStackingPolicy::Replaces)
		{
			if (Effect.Spec.Def->Modifiers.Num() > 0)
			{
				Effect.Spec.bTopOfStack = false;
				bool bFoundStack = false;

				// group all of the custom stacking effects and deal with them after
				if (Effect.Spec.GetStackingType() == EGameplayEffectStackingPolicy::Callback)
				{
					CustomStackedEffects.Add(&Effect);
					continue;
				}

				for (FActiveGameplayEffect*& StackedEffect : StackedEffects)
				{
					if (StackedEffect->Spec.GetStackingType() == Effect.Spec.GetStackingType() && StackedEffect->Spec.Def->Modifiers[0].Attribute == Effect.Spec.Def->Modifiers[0].Attribute)
					{
						switch (Effect.Spec.GetStackingType())
						{
						case EGameplayEffectStackingPolicy::Highest:
						{
							float BestSpecMagnitude = StackedEffect->Spec.GetMagnitude(StackedEffect->Spec.Def->Modifiers[0].Attribute);
							float CurrSpecMagnitude = Effect.Spec.GetMagnitude(Effect.Spec.Def->Modifiers[0].Attribute);
							if (BestSpecMagnitude < CurrSpecMagnitude)
							{
								StackedEffect = &Effect;
							}
							break;
						}
						case EGameplayEffectStackingPolicy::Lowest:
						{
							float BestSpecMagnitude = StackedEffect->Spec.GetMagnitude(StackedEffect->Spec.Def->Modifiers[0].Attribute);
							float CurrSpecMagnitude = Effect.Spec.GetMagnitude(Effect.Spec.Def->Modifiers[0].Attribute);
							if (BestSpecMagnitude > CurrSpecMagnitude)
							{
								StackedEffect = &Effect;
							}
							break;
						}
						default:
							// we shouldn't ever be here
							ABILITY_LOG(Warning, TEXT("%s uses unhandled stacking rule %s."), *Effect.Spec.Def->GetPathName(), *EGameplayEffectStackingPolicyToString(Effect.Spec.GetStackingType()));
							break;
						};

						bFoundStack = true;
						break;
					}
				}

				// if we didn't find a stack matching this effect we need to add it to our list
				if (!bFoundStack)
				{
					StackedEffects.Add(&Effect);
				}
			}
		}
	}

	// now deal with the custom effects
	while (CustomStackedEffects.Num() > 0)
	{
		int32 StackedIdx = StackedEffects.Add(CustomStackedEffects[0]);
		CustomStackedEffects.RemoveAtSwap(0);

		UGameplayEffectStackingExtension * StackedExt = StackedEffects[StackedIdx]->Spec.Def->StackingExtension->GetDefaultObject<UGameplayEffectStackingExtension>();

		TArray<FActiveGameplayEffect*> Effects;
		
		for (int32 Idx = 0; Idx < CustomStackedEffects.Num(); Idx++)
		{
			UGameplayEffectStackingExtension * CurrentExt = CustomStackedEffects[Idx]->Spec.Def->StackingExtension->GetDefaultObject<UGameplayEffectStackingExtension>();
			if (CurrentExt && StackedExt && CurrentExt->Handle == StackedExt->Handle && 
				StackedEffects[StackedIdx]->Spec.Def->Modifiers[0].Attribute == CustomStackedEffects[Idx]->Spec.Def->Modifiers[0].Attribute)
			{
				Effects.Add(CustomStackedEffects[Idx]);
				CustomStackedEffects.RemoveAtSwap(Idx);
				--Idx;
			}
		}

		UGameplayEffectStackingExtension * Ext = StackedEffects[StackedIdx]->Spec.Def->StackingExtension->GetDefaultObject<UGameplayEffectStackingExtension>();
		Ext->CalculateStack(Effects, *this, *StackedEffects[StackedIdx]);
	}

	// we've found all of the best stacking effects, mark them so that they updated properly
	for (FActiveGameplayEffect* const StackedEffect : StackedEffects)
	{
		StackedEffect->Spec.bTopOfStack = true;
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

	for (const FActiveGameplayEffect& Effect : GameplayEffects)
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

	for (const FActiveGameplayEffect &Effect : GameplayEffects)
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

void FActiveGameplayEffectsContainer::RemoveActiveEffects(const FActiveGameplayEffectQuery Query)
{
	const bool bLockInEffect = FScopedActiveGameplayEffectLock::IsLockInEffect();

	for (int32 idx=0; idx < GameplayEffects.Num(); ++idx)
	{
		const FActiveGameplayEffect& Effect = GameplayEffects[idx];
		if (Query.Matches(Effect))
		{
			// Defer removal if we're scope-locked
			if (bLockInEffect)
			{
				FScopedActiveGameplayEffectLock::AddAction()->InitForRemoveGE(TWeakObjectPtr<UAbilitySystemComponent>(Owner), Effect.Handle);
			}
			else
			{
				InternalRemoveActiveGameplayEffect(idx);
				idx--;
			}
		}
	}
}

FOnGameplayAttributeChange& FActiveGameplayEffectsContainer::RegisterGameplayAttributeEvent(FGameplayAttribute Attribute)
{
	return AttributeChangeDelegates.FindOrAdd(Attribute);
}

bool FActiveGameplayEffectsContainer::HasReceivedEffectWithPredictedKey(FPredictionKey PredictionKey) const
{
	for (const FActiveGameplayEffect& Effect : GameplayEffects)
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
	for (const FActiveGameplayEffect& Effect : GameplayEffects)
	{
		if (Effect.PredictionKey == PredictionKey && Effect.PredictionKey.WasReceived() == false)
		{
			return true;
		}
	}

	return false;
}

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
	// if we are looking for owning tags check them on the Granted Tags and Owned Tags Container
	if (OwningTagContainer)
	{
		if (!Effect.Spec.Def->InheritableOwnedTagsContainer.CombinedTags.MatchesAny(*OwningTagContainer, false) &&
			!Effect.Spec.DynamicGrantedTags.MatchesAny(*OwningTagContainer, false))
		{
			// this doesn't match our Tags so bail
			return false;
		}
	}	
	
	// if we are just looking for Tags on the Effect then look at the Gameplay Effect Tags
	if (EffectTagContainer)
	{
		if (!Effect.Spec.Def->InheritableGameplayEffectTags.CombinedTags.MatchesAny(*EffectTagContainer, false))
		{
			// this doesn't match our Tags so bail
			return false;
		}
	}

	// if we are just looking for Tags on the Effect then look at the Gameplay Effect Tags
	if (EffectTagContainer_Rejection)
	{
		if (Effect.Spec.Def->InheritableGameplayEffectTags.CombinedTags.MatchesAny(*EffectTagContainer_Rejection, false))
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

void FActiveGameplayEffectAction::PerformAction()
{
	check(bIsInitialized);
	if (OwningASC.IsValid())
	{
		if (bRemove)
		{
			OwningASC.Get()->RemoveActiveGameplayEffect(Handle);
		}
		else
		{
			UAbilitySystemComponent* ASC = OwningASC.Get();
			check(!ASC->ActiveGameplayEffects.GameplayEffectsPendingAdd.IsEmpty());
			FActiveGameplayEffect TempEffect;
			ASC->ActiveGameplayEffects.GameplayEffectsPendingAdd.Dequeue(TempEffect);
			ASC->ActiveGameplayEffects.GameplayEffects.Add(TempEffect);
		}
	}
}