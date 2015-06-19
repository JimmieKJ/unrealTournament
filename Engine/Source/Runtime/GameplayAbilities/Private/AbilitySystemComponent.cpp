// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemComponent.h"
#include "GameplayCueInterface.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayCueManager.h"

#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "MapErrors.h"
#include "DisplayDebugHelpers.h"
#include "VisualLogger.h"

DEFINE_LOG_CATEGORY(LogAbilitySystemComponent);

#define LOCTEXT_NAMESPACE "AbilitySystemComponent"

/** Enable to log out all render state create, destroy and updatetransform events */
#define LOG_RENDER_STATE 0

UAbilitySystemComponent::UAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, GameplayTagCountContainer()
{
	bWantsInitializeComponent = true;

	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bStartWithTickEnabled = true; // FIXME! Just temp until timer manager figured out
	PrimaryComponentTick.bCanEverTick = true;
	
	ActiveGameplayCues.Owner = this;

	bReplicates = true;

	UserAbilityActivationInhibited = false;

	GenericConfirmInputID = INDEX_NONE;
	GenericCancelInputID = INDEX_NONE;
}

UAbilitySystemComponent::~UAbilitySystemComponent()
{
	ActiveGameplayEffects.PreDestroy();
}

const UAttributeSet* UAbilitySystemComponent::InitStats(TSubclassOf<class UAttributeSet> Attributes, const UDataTable* DataTable)
{
	const UAttributeSet* AttributeObj = NULL;
	if (Attributes)
	{
		AttributeObj = GetOrCreateAttributeSubobject(Attributes);
		if (AttributeObj && DataTable)
		{
			// This const_cast is OK - this is one of the few places we want to directly modify our AttributeSet properties rather
			// than go through a gameplay effect
			const_cast<UAttributeSet*>(AttributeObj)->InitFromMetaDataTable(DataTable);
		}
	}
	return AttributeObj;
}

void UAbilitySystemComponent::K2_InitStats(TSubclassOf<class UAttributeSet> Attributes, const UDataTable* DataTable)
{
	InitStats(Attributes, DataTable);
}

const UAttributeSet* UAbilitySystemComponent::GetOrCreateAttributeSubobject(TSubclassOf<UAttributeSet> AttributeClass)
{
	AActor *OwningActor = GetOwner();
	const UAttributeSet *MyAttributes  = NULL;
	if (OwningActor && AttributeClass)
	{
		MyAttributes = GetAttributeSubobject(AttributeClass);
		if (!MyAttributes)
		{
			UAttributeSet *Attributes = NewObject<UAttributeSet>(OwningActor, AttributeClass);
			SpawnedAttributes.AddUnique(Attributes);
			MyAttributes = Attributes;
		}
	}

	return MyAttributes;
}

const UAttributeSet* UAbilitySystemComponent::GetAttributeSubobjectChecked(const TSubclassOf<UAttributeSet> AttributeClass) const
{
	const UAttributeSet *Set = GetAttributeSubobject(AttributeClass);
	check(Set);
	return Set;
}

const UAttributeSet* UAbilitySystemComponent::GetAttributeSubobject(const TSubclassOf<UAttributeSet> AttributeClass) const
{
	for (const UAttributeSet* Set : SpawnedAttributes)
	{
		if (Set && Set->IsA(AttributeClass))
		{
			return Set;
		}
	}
	return NULL;
}

bool UAbilitySystemComponent::HasAttributeSetForAttribute(FGameplayAttribute Attribute) const
{
	return (Attribute.IsValid() && (Attribute.IsSystemAttribute() || GetAttributeSubobject(Attribute.GetAttributeSetClass()) != nullptr));
}

void UAbilitySystemComponent::OnRegister()
{
	Super::OnRegister();

	// Init starting data
	for (int32 i=0; i < DefaultStartingData.Num(); ++i)
	{
		if (DefaultStartingData[i].Attributes && DefaultStartingData[i].DefaultStartingTable)
		{
			UAttributeSet* Attributes = const_cast<UAttributeSet*>(GetOrCreateAttributeSubobject(DefaultStartingData[i].Attributes));
			Attributes->InitFromMetaDataTable(DefaultStartingData[i].DefaultStartingTable);
		}
	}

	ActiveGameplayEffects.RegisterWithOwner(this);
	ActivatableAbilities.RegisterWithOwner(this);
}

// ---------------------------------------------------------

bool UAbilitySystemComponent::IsOwnerActorAuthoritative() const
{
	return !IsNetSimulating();
}

bool UAbilitySystemComponent::HasNetworkAuthorityToApplyGameplayEffect(FPredictionKey PredictionKey) const
{
	return (IsOwnerActorAuthoritative() || PredictionKey.IsValidForMorePrediction());
}

void UAbilitySystemComponent::SetNumericAttributeBase(const FGameplayAttribute &Attribute, float NewFloatValue)
{
	// Go through our active gameplay effects container so that aggregation/mods are handled properly.
	ActiveGameplayEffects.SetAttributeBaseValue(Attribute, NewFloatValue);
}

void UAbilitySystemComponent::SetNumericAttribute_Internal(const FGameplayAttribute &Attribute, float NewFloatValue)
{
	// Set the attribute directly: update the UProperty on the attribute set.
	const UAttributeSet* AttributeSet = GetAttributeSubobjectChecked(Attribute.GetAttributeSetClass());
	Attribute.SetNumericValueChecked(NewFloatValue, const_cast<UAttributeSet*>(AttributeSet));
}

float UAbilitySystemComponent::GetNumericAttribute(const FGameplayAttribute &Attribute) const
{
	if (Attribute.IsSystemAttribute())
	{
		return 0.f;
	}

	const UAttributeSet* const AttributeSetOrNull = GetAttributeSubobject(Attribute.GetAttributeSetClass());
	if (AttributeSetOrNull == nullptr)
	{
		return 0.f;
	}

	return Attribute.GetNumericValue(AttributeSetOrNull);
}

float UAbilitySystemComponent::GetNumericAttributeChecked(const FGameplayAttribute &Attribute) const
{
	if(Attribute.IsSystemAttribute())
	{
		return 0.f;
	}

	const UAttributeSet* AttributeSet = GetAttributeSubobjectChecked(Attribute.GetAttributeSetClass());
	return Attribute.GetNumericValueChecked(AttributeSet);
}

void UAbilitySystemComponent::ApplyModToAttribute(const FGameplayAttribute &Attribute, TEnumAsByte<EGameplayModOp::Type> ModifierOp, float ModifierMagnitude)
{
	// We can only apply loose mods on the authority. If we ever need to predict these, they would need to be turned into GEs and be given a prediction key so that
	// they can be rolled back.
	if (IsOwnerActorAuthoritative())
	{
		ActiveGameplayEffects.ApplyModToAttribute(Attribute, ModifierOp, ModifierMagnitude);
	}
}

void UAbilitySystemComponent::ApplyModToAttributeUnsafe(const FGameplayAttribute &Attribute, TEnumAsByte<EGameplayModOp::Type> ModifierOp, float ModifierMagnitude)
{
	ActiveGameplayEffects.ApplyModToAttribute(Attribute, ModifierOp, ModifierMagnitude);
}

FGameplayEffectSpecHandle UAbilitySystemComponent::MakeOutgoingSpec(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level, FGameplayEffectContextHandle Context) const
{
	SCOPE_CYCLE_COUNTER(STAT_GetOutgoingSpec);
	if (Context.IsValid() == false)
	{
		Context = GetEffectContext();
	}

	if (GameplayEffectClass)
	{
		UGameplayEffect* GameplayEffect = GameplayEffectClass->GetDefaultObject<UGameplayEffect>();

		FGameplayEffectSpec* NewSpec = new FGameplayEffectSpec(GameplayEffect, Context, Level);
		return FGameplayEffectSpecHandle(NewSpec);
	}

	return FGameplayEffectSpecHandle(nullptr);
}

FGameplayEffectSpecHandle UAbilitySystemComponent::GetOutgoingSpec(const UGameplayEffect* GameplayEffect, float Level, FGameplayEffectContextHandle Contex) const
{
	if (GameplayEffect)
	{
		return MakeOutgoingSpec(GameplayEffect->GetClass(), Level, GetEffectContext());
	}

	return FGameplayEffectSpecHandle(nullptr);
}

FGameplayEffectSpecHandle UAbilitySystemComponent::GetOutgoingSpec(const UGameplayEffect* GameplayEffect, float Level) const
{
	return GetOutgoingSpec(GameplayEffect, Level, GetEffectContext());
}

FGameplayEffectContextHandle UAbilitySystemComponent::GetEffectContext() const
{
	FGameplayEffectContextHandle Context = FGameplayEffectContextHandle(UAbilitySystemGlobals::Get().AllocGameplayEffectContext());
	// By default use the owner and avatar as the instigator and causer
	check(AbilityActorInfo.IsValid());
	
	Context.AddInstigator(AbilityActorInfo->OwnerActor.Get(), AbilityActorInfo->AvatarActor.Get());
	return Context;
}

FActiveGameplayEffectHandle UAbilitySystemComponent::BP_ApplyGameplayEffectToTarget(TSubclassOf<UGameplayEffect> GameplayEffectClass, UAbilitySystemComponent* Target, float Level, FGameplayEffectContextHandle Context)
{
	if (Target == nullptr)
	{
		ABILITY_LOG(Error, TEXT("UAbilitySystemComponent::BP_ApplyGameplayEffectToTarget called with null Target. Context: %s"), *Context.ToString());
		return FActiveGameplayEffectHandle();
	}

	if (GameplayEffectClass == nullptr)
	{
		ABILITY_LOG(Error, TEXT("UAbilitySystemComponent::BP_ApplyGameplayEffectToTarget called with null GameplayEffectClass. Context: %s"), *Context.ToString());
		return FActiveGameplayEffectHandle();
	}

	UGameplayEffect* GameplayEffect = GameplayEffectClass->GetDefaultObject<UGameplayEffect>();
	return ApplyGameplayEffectToTarget(GameplayEffect, Target, Level, Context);	
}

FActiveGameplayEffectHandle UAbilitySystemComponent::K2_ApplyGameplayEffectToTarget(UGameplayEffect *GameplayEffect, UAbilitySystemComponent *Target, float Level, FGameplayEffectContextHandle Context)
{
	return ApplyGameplayEffectToTarget(GameplayEffect, Target, Level, Context);
}

/** This is a helper function used in automated testing, I'm not sure how useful it will be to gamecode or blueprints */
FActiveGameplayEffectHandle UAbilitySystemComponent::ApplyGameplayEffectToTarget(UGameplayEffect *GameplayEffect, UAbilitySystemComponent *Target, float Level, FGameplayEffectContextHandle Context, FPredictionKey PredictionKey)
{
	check(GameplayEffect);
	if (HasNetworkAuthorityToApplyGameplayEffect(PredictionKey))
	{
		if (!Context.IsValid())
		{
			Context = GetEffectContext();
		}

		FGameplayEffectSpec	Spec(GameplayEffect, Context, Level);
		return ApplyGameplayEffectSpecToTarget(Spec, Target, PredictionKey);
	}

	return FActiveGameplayEffectHandle();
}

/** Helper function since we can't have default/optional values for FModifierQualifier in K2 function */
FActiveGameplayEffectHandle UAbilitySystemComponent::BP_ApplyGameplayEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level, FGameplayEffectContextHandle EffectContext)
{
	if ( GameplayEffectClass )
	{
		UGameplayEffect* GameplayEffect = GameplayEffectClass->GetDefaultObject<UGameplayEffect>();
		return ApplyGameplayEffectToSelf(GameplayEffect, Level, EffectContext);
	}

	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle UAbilitySystemComponent::K2_ApplyGameplayEffectToSelf(const UGameplayEffect *GameplayEffect, float Level, FGameplayEffectContextHandle EffectContext)
{
	if ( GameplayEffect )
	{
		return BP_ApplyGameplayEffectToSelf(GameplayEffect->GetClass(), Level, EffectContext);
	}

	return FActiveGameplayEffectHandle();
}

/** This is a helper function - it seems like this will be useful as a blueprint interface at the least, but Level parameter may need to be expanded */
FActiveGameplayEffectHandle UAbilitySystemComponent::ApplyGameplayEffectToSelf(const UGameplayEffect *GameplayEffect, float Level, const FGameplayEffectContextHandle& EffectContext, FPredictionKey PredictionKey)
{
	if (GameplayEffect == nullptr)
	{
		ABILITY_LOG(Error, TEXT("UAbilitySystemComponent::ApplyGameplayEffectToSelf called by Instigator %s with a null GameplayEffect."), *EffectContext.ToString());
		return FActiveGameplayEffectHandle();
	}

	if (HasNetworkAuthorityToApplyGameplayEffect(PredictionKey))
	{
		FGameplayEffectSpec	Spec(GameplayEffect, EffectContext, Level);
		return ApplyGameplayEffectSpecToSelf(Spec, PredictionKey);
	}

	return FActiveGameplayEffectHandle();
}

FOnActiveGameplayEffectRemoved* UAbilitySystemComponent::OnGameplayEffectRemovedDelegate(FActiveGameplayEffectHandle Handle)
{
	FActiveGameplayEffect* ActiveEffect = ActiveGameplayEffects.GetActiveGameplayEffect(Handle);
	if (ActiveEffect)
	{
		return &ActiveEffect->OnRemovedDelegate;
	}

	return nullptr;
}

FOnActiveGameplayEffectRemoved& UAbilitySystemComponent::OnAnyGameplayEffectRemovedDelegate()
{
	return ActiveGameplayEffects.OnActiveGameplayEffectRemovedDelegate;
}

int32 UAbilitySystemComponent::GetNumActiveGameplayEffects() const
{
	return ActiveGameplayEffects.GetNumGameplayEffects();
}

void UAbilitySystemComponent::GetAllActiveGameplayEffectSpecs(TArray<FGameplayEffectSpec>& OutSpecCopies)
{	
	ActiveGameplayEffects.GetAllActiveGameplayEffectSpecs(OutSpecCopies);
}

const FGameplayTagContainer* UAbilitySystemComponent::GetGameplayEffectSourceTagsFromHandle(FActiveGameplayEffectHandle Handle) const
{
	return ActiveGameplayEffects.GetGameplayEffectSourceTagsFromHandle(Handle);
}

const FGameplayTagContainer* UAbilitySystemComponent::GetGameplayEffectTargetTagsFromHandle(FActiveGameplayEffectHandle Handle) const
{
	return ActiveGameplayEffects.GetGameplayEffectTargetTagsFromHandle(Handle);
}

void UAbilitySystemComponent::CaptureAttributeForGameplayEffect(OUT FGameplayEffectAttributeCaptureSpec& OutCaptureSpec)
{
	// Verify the capture is happening on an attribute the component actually has a set for; if not, can't capture the value
	const FGameplayAttribute& AttributeToCapture = OutCaptureSpec.BackingDefinition.AttributeToCapture;
	if (AttributeToCapture.IsValid() && (AttributeToCapture.IsSystemAttribute() || GetAttributeSubobject(AttributeToCapture.GetAttributeSetClass())))
	{
		ActiveGameplayEffects.CaptureAttributeForGameplayEffect(OutCaptureSpec);
	}
}

FOnGameplayEffectTagCountChanged& UAbilitySystemComponent::RegisterGameplayTagEvent(FGameplayTag Tag)
{
	return GameplayTagCountContainer.RegisterGameplayTagEvent(Tag);
}

FOnGameplayEffectTagCountChanged& UAbilitySystemComponent::RegisterGenericGameplayTagEvent()
{
	return GameplayTagCountContainer.RegisterGenericGameplayEvent();
}

FOnGameplayAttributeChange& UAbilitySystemComponent::RegisterGameplayAttributeEvent(FGameplayAttribute Attribute)
{
	return ActiveGameplayEffects.RegisterGameplayAttributeEvent(Attribute);
}

UProperty* UAbilitySystemComponent::GetOutgoingDurationProperty()
{
	static UProperty* DurationProperty = FindFieldChecked<UProperty>(UAbilitySystemComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(UAbilitySystemComponent, OutgoingDuration));
	return DurationProperty;
}

UProperty* UAbilitySystemComponent::GetIncomingDurationProperty()
{
	static UProperty* DurationProperty = FindFieldChecked<UProperty>(UAbilitySystemComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(UAbilitySystemComponent, IncomingDuration));
	return DurationProperty;
}

const FGameplayEffectAttributeCaptureDefinition& UAbilitySystemComponent::GetOutgoingDurationCapture()
{
	// We will just always take snapshots of the source's duration mods
	static FGameplayEffectAttributeCaptureDefinition OutgoingDurationCapture(GetOutgoingDurationProperty(), EGameplayEffectAttributeCaptureSource::Source, true);
	return OutgoingDurationCapture;

}
const FGameplayEffectAttributeCaptureDefinition& UAbilitySystemComponent::GetIncomingDurationCapture()
{
	// Never take snapshots of the target's duration mods: we are going to evaluate this on apply only.
	static FGameplayEffectAttributeCaptureDefinition IncomingDurationCapture(GetIncomingDurationProperty(), EGameplayEffectAttributeCaptureSource::Target, false);
	return IncomingDurationCapture;
}

// ------------------------------------------------------------------------

void UAbilitySystemComponent::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsGetOwnedTags);
	TagContainer.AppendTags(GameplayTagCountContainer.GetExplicitGameplayTags());
}

bool UAbilitySystemComponent::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	SCOPE_CYCLE_COUNTER(STAT_HasMatchingGameplayTag);
	return GameplayTagCountContainer.HasMatchingGameplayTag(TagToCheck);
}

bool UAbilitySystemComponent::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer, bool bCountEmptyAsMatch) const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsHasAllTags);
	return GameplayTagCountContainer.HasAllMatchingGameplayTags(TagContainer, bCountEmptyAsMatch);
}

bool UAbilitySystemComponent::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer, bool bCountEmptyAsMatch) const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayEffectsHasAnyTag);
	return GameplayTagCountContainer.HasAnyMatchingGameplayTags(TagContainer, bCountEmptyAsMatch);
}

void UAbilitySystemComponent::AddLooseGameplayTag(const FGameplayTag& GameplayTag, int32 Count)
{
	UpdateTagMap(GameplayTag, Count);
}

void UAbilitySystemComponent::AddLooseGameplayTags(const FGameplayTagContainer& GameplayTags, int32 Count)
{
	UpdateTagMap(GameplayTags, Count);
}

void UAbilitySystemComponent::RemoveLooseGameplayTag(const FGameplayTag& GameplayTag, int32 Count)
{
	UpdateTagMap(GameplayTag, -Count);
}

void UAbilitySystemComponent::RemoveLooseGameplayTags(const FGameplayTagContainer& GameplayTags, int32 Count)
{
	UpdateTagMap(GameplayTags, -Count);
}

// These are functionally redundant but are called by GEs and GameplayCues that add tags that are not 'loose' (but are handled the same way in practice)

void UAbilitySystemComponent::UpdateTagMap(const FGameplayTag& BaseTag, int32 CountDelta)
{
	GameplayTagCountContainer.UpdateTagCount(BaseTag, CountDelta);
}

void UAbilitySystemComponent::UpdateTagMap(const FGameplayTagContainer& Container, int32 CountDelta)
{
	GameplayTagCountContainer.UpdateTagCount(Container, CountDelta);
}

// ------------------------------------------------------------------------

FActiveGameplayEffectHandle UAbilitySystemComponent::ApplyGameplayEffectSpecToTarget(OUT FGameplayEffectSpec &Spec, UAbilitySystemComponent *Target, FPredictionKey PredictionKey)
{
	if (!UAbilitySystemGlobals::Get().ShouldPredictTargetGameplayEffects())
	{
		// If we don't want to predict target effects, clear prediction key
		PredictionKey = FPredictionKey();
	}

	FActiveGameplayEffectHandle ReturnHandle;

	if (!UAbilitySystemGlobals::Get().ShouldPredictTargetGameplayEffects())
	{
		// If we don't want to predict target effects, clear prediction key
		PredictionKey = FPredictionKey();
	}

	if (Target)
	{
		ReturnHandle = Target->ApplyGameplayEffectSpecToSelf(Spec, PredictionKey);
	}

	return ReturnHandle;
}

FActiveGameplayEffectHandle UAbilitySystemComponent::ApplyGameplayEffectSpecToSelf(OUT FGameplayEffectSpec &Spec, FPredictionKey PredictionKey)
{
	// Scope lock the container after the addition has taken place to prevent the new effect from potentially getting mangled during the remainder
	// of the add operation
	FScopedActiveGameplayEffectLock ScopeLock(ActiveGameplayEffects);

	const bool bIsNetAuthority = IsOwnerActorAuthoritative();

	// Check Network Authority
	if (!HasNetworkAuthorityToApplyGameplayEffect(PredictionKey))
	{
		return FActiveGameplayEffectHandle();
	}

	// Don't allow prediction of periodic effects
	if(PredictionKey.IsValidKey() && Spec.GetPeriod() > 0.f)
	{
		if(IsOwnerActorAuthoritative())
		{
			// Server continue with invalid prediction key
			PredictionKey = FPredictionKey();
		}
		else
		{
			// Client just return now
			return FActiveGameplayEffectHandle();
		}
	}

	// Are we currently immune to this? (ApplicationImmunity)
	if (ActiveGameplayEffects.HasApplicationImmunityToSpec(Spec))
	{
		return FActiveGameplayEffectHandle();
	}

	// Check AttributeSet requirements: do we have everything this GameplayEffectSpec expects?
	// We may want to cache this off in some way to make the runtime check quicker.
	// We also need to handle things in the execution list
	for (const FGameplayModifierInfo& Mod : Spec.Def->Modifiers)
	{
		if (!Mod.Attribute.IsValid())
		{
			ABILITY_LOG(Warning, TEXT("%s has a null modifier attribute."), *Spec.Def->GetPathName());
			return FActiveGameplayEffectHandle();
		}

		if (HasAttributeSetForAttribute(Mod.Attribute) == false)
		{
			return FActiveGameplayEffectHandle();
		}
	}

	// check if the effect being applied actually succeeds
	float ChanceToApply = Spec.GetChanceToApplyToTarget();
	if ((ChanceToApply < 1.f - SMALL_NUMBER) && (FMath::FRand() > ChanceToApply))
	{
		return FActiveGameplayEffectHandle();
	}

	// Get MyTags.
	//	We may want to cache off a GameplayTagContainer instead of rebuilding it every time.
	//	But this will also be where we need to merge in context tags? (Headshot, executing ability, etc?)
	//	Or do we push these tags into (our copy of the spec)?

	FGameplayTagContainer MyTags;
	GetOwnedGameplayTags(MyTags);

	if (Spec.Def->ApplicationTagRequirements.RequirementsMet(MyTags) == false)
	{
		return FActiveGameplayEffectHandle();
	}
	

	// Clients should treat predicted instant effects as if they have infinite duration. The effects will be cleaned up later.
	bool bTreatAsInfiniteDuration = GetOwnerRole() != ROLE_Authority && PredictionKey.IsLocalClientKey() && Spec.GetDuration() == UGameplayEffect::INSTANT_APPLICATION;

	// Make sure we create our copy of the spec in the right place
	FActiveGameplayEffectHandle	MyHandle;
	bool bInvokeGameplayCueApplied = UGameplayEffect::INSTANT_APPLICATION != Spec.GetDuration(); // Cache this now before possibly modifying predictive instant effect to infinite duration effect.

	FActiveGameplayEffect* AppliedEffect = nullptr;

	FGameplayEffectSpec* OurCopyOfSpec = nullptr;
	TSharedPtr<FGameplayEffectSpec> StackSpec;
	float Duration = bTreatAsInfiniteDuration ? UGameplayEffect::INFINITE_DURATION : Spec.GetDuration();
	{
		if (Duration != UGameplayEffect::INSTANT_APPLICATION)
		{
			AppliedEffect = ActiveGameplayEffects.ApplyGameplayEffectSpec(Spec, PredictionKey);
			if (!AppliedEffect)
			{
				return FActiveGameplayEffectHandle();
			}

			MyHandle = AppliedEffect->Handle;
			OurCopyOfSpec = &(AppliedEffect->Spec);

			// Log results of applied GE spec
			if (UE_LOG_ACTIVE(VLogAbilitySystem, Log))
			{
				ABILITY_VLOG(OwnerActor, Log, TEXT("Applied %s"), *OurCopyOfSpec->Def->GetFName().ToString());

				for (FGameplayModifierInfo Modifier : Spec.Def->Modifiers)
				{
					float Magnitude = 0.f;
					Modifier.ModifierMagnitude.AttemptCalculateMagnitude(Spec, Magnitude);
					ABILITY_VLOG(OwnerActor, Log, TEXT("         %s: %s %f"), *Modifier.Attribute.GetName(), *EGameplayModOpToString(Modifier.ModifierOp), Magnitude);
				}
			}
		}

		if (!OurCopyOfSpec)
		{
			StackSpec = TSharedPtr<FGameplayEffectSpec>(new FGameplayEffectSpec(Spec));
			OurCopyOfSpec = StackSpec.Get();
			UAbilitySystemGlobals::Get().GlobalPreGameplayEffectSpecApply(*OurCopyOfSpec, this);
			OurCopyOfSpec->CaptureAttributeDataFromTarget(this);
		}

		// if necessary add a modifier to OurCopyOfSpec to force it to have an infinite duration
		if (bTreatAsInfiniteDuration)
		{
			// This should just be a straight set of the duration float now
			OurCopyOfSpec->SetDuration(UGameplayEffect::INFINITE_DURATION, true);
		}
	}
	

	// We still probably want to apply tags and stuff even if instant?
	if (bInvokeGameplayCueApplied && AppliedEffect && !AppliedEffect->bIsInhibited)
	{
		// We both added and activated the GameplayCue here.
		// On the client, who will invoke the gameplay cue from an OnRep, he will need to look at the StartTime to determine
		// if the Cue was actually added+activated or just added (due to relevancy)

		// Fixme: what if we wanted to scale Cue magnitude based on damage? E.g, scale an cue effect when the GE is buffed?

		if (OurCopyOfSpec->StackCount > Spec.StackCount)
		{
			// Because PostReplicatedChange will get called from modifying the stack count
			// (and not PostReplicatedAdd) we won't know which GE was modified.
			// So instead we need to explicitly RPC the client so it knows the GC needs updating
			NetMulticast_InvokeGameplayCueAddedAndWhileActive_FromSpec(*OurCopyOfSpec, PredictionKey);
		}
		else
		{
			// Otherwise these will get replicated to the client when the GE gets added to the replicated array
			InvokeGameplayCueEvent(*OurCopyOfSpec, EGameplayCueEvent::OnActive);
			InvokeGameplayCueEvent(*OurCopyOfSpec, EGameplayCueEvent::WhileActive);
		}
	}
	
	// Execute the GE at least once (if instant, this will execute once and be done. If persistent, it was added to ActiveGameplayEffects above)
	
	// Execute if this is an instant application effect
	if (Duration == UGameplayEffect::INSTANT_APPLICATION)
	{
		if (OurCopyOfSpec->Def->OngoingTagRequirements.IsEmpty())
		{
			ExecuteGameplayEffect(*OurCopyOfSpec, PredictionKey);
		}
		else
		{
			ABILITY_LOG(Warning, TEXT("%s is instant but has tag requirements. Tag requirements can only be used with gameplay effects that have a duration. This gameplay effect will be ignored."), *Spec.Def->GetPathName());
		}
	}
	else if (bTreatAsInfiniteDuration)
	{
		// This is an instant application but we are treating it as an infinite duration for prediction. We should still predict the execute GameplayCUE.
		// (in non predictive case, this will happen inside ::ExecuteGameplayEffect)

		UAbilitySystemGlobals::Get().GetGameplayCueManager()->InvokeGameplayCueExecuted_FromSpec(this, *OurCopyOfSpec, PredictionKey);
	}

	if (Spec.GetPeriod() != UGameplayEffect::NO_PERIOD && Spec.TargetEffectSpecs.Num() > 0)
	{
		ABILITY_LOG(Warning, TEXT("%s is periodic but also applies GameplayEffects to its target. GameplayEffects will only be applied once, not every period."), *Spec.Def->GetPathName());
	}

	// ------------------------------------------------------
	//	Remove gameplay effects with tags
	//		Remove any active gameplay effects that match the RemoveGameplayEffectsWithTags in the definition for this spec
	//		Only call this if we are the Authoritative owner and we have some RemoveGameplayEffectsWithTags.CombinedTag to remove
	// ------------------------------------------------------
	if (bIsNetAuthority && Spec.Def->RemoveGameplayEffectsWithTags.CombinedTags.Num() > 0)
	{
		// Clear tags is always removing all stacks.
		FActiveGameplayEffectQuery ClearQuery(&Spec.Def->RemoveGameplayEffectsWithTags.CombinedTags);
		if (MyHandle.IsValid())
		{
			ClearQuery.IgnoreHandles.Add(MyHandle);
		}
		ActiveGameplayEffects.RemoveActiveEffects(ClearQuery, -1);
	}
	

	// ------------------------------------------------------
	//	Apply Granted Abilities
	//	
	//	Note: Doing this before apply TargetEffectSpecs, but we could just as easily apply after.
	//	Hedging bet for now that we are more likely to want to have 'passive ability that reacts to linked GE'
	//	over 'need ability to activate after linked GE is applied'.
	//	
	//	Note2: This is allowing instant GEs to permanently grant abilities. This could be disallowed if needed.
	// ------------------------------------------------------
	if (bIsNetAuthority)
	{
		for (FGameplayAbilitySpecDef& AbilitySpecDef : OurCopyOfSpec->GrantedAbilitySpecs)
		{
			// Only do this is we havent assigned the ability yet! This prevents cases where stacking GEs
			// would regrant the ability every time the stack was applied
			if (AbilitySpecDef.AssignedHandle.IsValid() == false)
			{
				GiveAbility( FGameplayAbilitySpec(AbilitySpecDef, MyHandle) );
			}
		}	
	}

	// ------------------------------------------------------
	// Apply Linked effects
	// todo: this is ignoring the returned handles, should we put them into a TArray and return all of the handles?
	// ------------------------------------------------------
	for (const FGameplayEffectSpecHandle TargetSpec: Spec.TargetEffectSpecs)
	{
		if (TargetSpec.IsValid())
		{
			ApplyGameplayEffectSpecToSelf(*TargetSpec.Data.Get(), PredictionKey);
		}
	}

	UAbilitySystemComponent* InstigatorASC = Spec.GetContext().GetInstigatorAbilitySystemComponent();

	// Send ourselves a callback	
	OnGameplayEffectAppliedToSelf(InstigatorASC, *OurCopyOfSpec, MyHandle);

	// Send the instigator a callback
	if (InstigatorASC)
	{
		InstigatorASC->OnGameplayEffectAppliedToTarget(this, *OurCopyOfSpec, MyHandle);
	}

	return MyHandle;
}

void UAbilitySystemComponent::ExecutePeriodicEffect(FActiveGameplayEffectHandle	Handle)
{
	ActiveGameplayEffects.ExecutePeriodicGameplayEffect(Handle);
}

void UAbilitySystemComponent::ExecuteGameplayEffect(FGameplayEffectSpec &Spec, FPredictionKey PredictionKey)
{
	// Should only ever execute effects that are instant application or periodic application
	// Effects with no period and that aren't instant application should never be executed
	check( (Spec.GetDuration() == UGameplayEffect::INSTANT_APPLICATION || Spec.GetPeriod() != UGameplayEffect::NO_PERIOD) );

	if (UE_LOG_ACTIVE(VLogAbilitySystem, Log))
	{
		ABILITY_VLOG(OwnerActor, Log, TEXT("Executed %s"), *Spec.Def->GetFName().ToString());
		
		for (FGameplayModifierInfo Modifier : Spec.Def->Modifiers)
		{
			float Magnitude = 0.f;
			Modifier.ModifierMagnitude.AttemptCalculateMagnitude(Spec, Magnitude);
			ABILITY_VLOG(OwnerActor, Log, TEXT("         %s: %s %f"), *Modifier.Attribute.GetName(), *EGameplayModOpToString(Modifier.ModifierOp), Magnitude);
		}
	}

	ActiveGameplayEffects.ExecuteActiveEffectsFrom(Spec, PredictionKey);
}

void UAbilitySystemComponent::CheckDurationExpired(FActiveGameplayEffectHandle Handle)
{
	ActiveGameplayEffects.CheckDuration(Handle);
}

bool UAbilitySystemComponent::RemoveActiveGameplayEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove)
{
	return ActiveGameplayEffects.RemoveActiveGameplayEffect(Handle, StacksToRemove);
}

void UAbilitySystemComponent::RemoveActiveGameplayEffectBySourceEffect(TSubclassOf<UGameplayEffect> GameplayEffect, UAbilitySystemComponent* InstigatorAbilitySystemComponent, int32 StacksToRemove /*= -1*/)
{
	if (GameplayEffect)
	{
		FActiveGameplayEffectQuery Query;
		Query.CustomMatch.BindLambda([&](const FActiveGameplayEffect& CurEffect)
		{
			bool bMatches = false;

			// First check at matching: backing GE class must be the exact same
			if (CurEffect.Spec.Def && GameplayEffect == CurEffect.Spec.Def->GetClass())
			{
				// If an instigator is specified, matching is dependent upon it
				if (InstigatorAbilitySystemComponent)
				{
					bMatches = (InstigatorAbilitySystemComponent == CurEffect.Spec.GetEffectContext().GetInstigatorAbilitySystemComponent());
				}
				else
				{
					bMatches = true;
				}
			}

			return bMatches;
		});

		ActiveGameplayEffects.RemoveActiveEffects(Query, StacksToRemove);
	}
}

float UAbilitySystemComponent::GetGameplayEffectDuration(FActiveGameplayEffectHandle Handle) const
{
	return ActiveGameplayEffects.GetGameplayEffectDuration(Handle);
}

float UAbilitySystemComponent::GetGameplayEffectMagnitude(FActiveGameplayEffectHandle Handle, FGameplayAttribute Attribute) const
{
	return ActiveGameplayEffects.GetGameplayEffectMagnitude(Handle, Attribute);
}

int32 UAbilitySystemComponent::GetCurrentStackCount(FActiveGameplayEffectHandle Handle) const
{
	if (const FActiveGameplayEffect* ActiveGE = ActiveGameplayEffects.GetActiveGameplayEffect(Handle))
	{
		return ActiveGE->Spec.StackCount;
	}
	return 0;
}

int32 UAbilitySystemComponent::GetCurrentStackCount(FGameplayAbilitySpecHandle Handle) const
{
	FActiveGameplayEffectHandle GEHandle = FindActiveGameplayEffectHandle(Handle);
	if (GEHandle.IsValid())
	{
		return GetCurrentStackCount(GEHandle);
	}
	return 0;
}

FActiveGameplayEffectHandle UAbilitySystemComponent::FindActiveGameplayEffectHandle(FGameplayAbilitySpecHandle Handle) const
{
	for (const FActiveGameplayEffect& ActiveGE : &ActiveGameplayEffects)
	{
		for (const FGameplayAbilitySpecDef& AbiilitySpecDef : ActiveGE.Spec.GrantedAbilitySpecs)
		{
			if (AbiilitySpecDef.AssignedHandle == Handle)
			{
				return ActiveGE.Handle;
			}
		}
	}
	return FActiveGameplayEffectHandle();
}

void UAbilitySystemComponent::InvokeGameplayCueEvent(const FGameplayEffectSpecForRPC &Spec, EGameplayCueEvent::Type EventType)
{
	AActor* ActorAvatar = AbilityActorInfo->AvatarActor.Get();
	if (!Spec.Def)
	{
		ABILITY_LOG(Warning, TEXT("InvokeGameplayCueEvent Actor %s that has no gameplay effect!"), ActorAvatar ? *ActorAvatar->GetName() : TEXT("NULL"));
		return;
	}
	
	float ExecuteLevel = Spec.GetLevel();

	FGameplayCueParameters CueParameters;
	CueParameters.EffectContext = Spec.GetContext();
	CueParameters.AggregatedSourceTags = Spec.AggregatedSourceTags;
	CueParameters.AggregatedTargetTags = Spec.AggregatedTargetTags;

	for (FGameplayEffectCue CueInfo : Spec.Def->GameplayCues)
	{
		if (CueInfo.MagnitudeAttribute.IsValid())
		{
			if (const FGameplayEffectModifiedAttribute* ModifiedAttribute = Spec.GetModifiedAttribute(CueInfo.MagnitudeAttribute))
			{
				CueParameters.RawMagnitude = ModifiedAttribute->TotalMagnitude;
			}
			else
			{
				CueParameters.RawMagnitude = 0.0f;
			}
		}
		else
		{
			CueParameters.RawMagnitude = 0.0f;
		}

		CueParameters.NormalizedMagnitude = CueInfo.NormalizeLevel(ExecuteLevel);

		UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCues(ActorAvatar, CueInfo.GameplayCueTags, EventType, CueParameters);
	}
}

void UAbilitySystemComponent::InvokeGameplayCueEvent(const FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, FGameplayEffectContextHandle EffectContext)
{
	FGameplayCueParameters CueParameters;

	if (EffectContext.IsValid())
	{
		CueParameters.EffectContext = EffectContext;
	}

	CueParameters.NormalizedMagnitude = 1.f;
	CueParameters.RawMagnitude = 0.f;

	InvokeGameplayCueEvent(GameplayCueTag, EventType, CueParameters);
}

void UAbilitySystemComponent::InvokeGameplayCueEvent(const FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& GameplayCueParameters)
{
	AActor* ActorAvatar = AbilityActorInfo->AvatarActor.Get();
	AActor* ActorOwner = AbilityActorInfo->OwnerActor.Get();

	UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCues(ActorAvatar, GameplayCueTag, EventType, GameplayCueParameters);
}

void UAbilitySystemComponent::ExecuteGameplayCue(const FGameplayTag GameplayCueTag, FGameplayEffectContextHandle EffectContext)
{
	// Send to the wrapper on the cue manager
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->InvokeGameplayCueExecuted(this, GameplayCueTag, ScopedPredictionKey, EffectContext);
}

void UAbilitySystemComponent::ExecuteGameplayCue(const FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters)
{
	// Send to the wrapper on the cue manager
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->InvokeGameplayCueExecuted_WithParams(this, GameplayCueTag, ScopedPredictionKey, GameplayCueParameters);
}

void UAbilitySystemComponent::AddGameplayCue(const FGameplayTag GameplayCueTag, FGameplayEffectContextHandle EffectContext)
{
	if (IsOwnerActorAuthoritative())
	{
		bool bWasInList = HasMatchingGameplayTag(GameplayCueTag);

		ForceReplication();
		ActiveGameplayCues.AddCue(GameplayCueTag, ScopedPredictionKey);
		NetMulticast_InvokeGameplayCueAdded(GameplayCueTag, ScopedPredictionKey, EffectContext);

		if (!bWasInList)
		{
			// Call on server here, clients get it from repnotify
			InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::WhileActive);
		}
	}
	else if (ScopedPredictionKey.IsLocalClientKey())
	{
		// Allow for predictive gameplaycue events? Needs more thought
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::OnActive, EffectContext);
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::WhileActive);
	}
}

void UAbilitySystemComponent::RemoveGameplayCue(const FGameplayTag GameplayCueTag)
{
	if (IsOwnerActorAuthoritative())
	{
		bool bWasInList = HasMatchingGameplayTag(GameplayCueTag);

		ActiveGameplayCues.RemoveCue(GameplayCueTag);

		if (bWasInList)
		{
			// Call on server here, clients get it from repnotify
			InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Removed);
		}
		// Don't need to multicast broadcast this, ACtiveGameplayCues replication handles it
	}
	else if (ScopedPredictionKey.IsLocalClientKey())
	{
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Removed);
	}
}

void UAbilitySystemComponent::RemoveAllGameplayCues()
{
	for (int32 i = (ActiveGameplayCues.GameplayCues.Num() - 1); i >= 0; --i)
	{
		RemoveGameplayCue(ActiveGameplayCues.GameplayCues[i].GameplayCueTag);
	}
}

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCueExecuted_FromSpec_Implementation(const FGameplayEffectSpecForRPC Spec, FPredictionKey PredictionKey)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		InvokeGameplayCueEvent(Spec, EGameplayCueEvent::Executed);
	}
}

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCueExecuted_Implementation(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Executed, EffectContext);
	}
}

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCueExecuted_WithParams_Implementation(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Executed, GameplayCueParameters);
	}
}

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCueAdded_Implementation(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::OnActive, EffectContext);
	}
}

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCueAddedAndWhileActive_FromSpec_Implementation(const FGameplayEffectSpecForRPC& Spec, FPredictionKey PredictionKey)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		InvokeGameplayCueEvent(Spec, EGameplayCueEvent::OnActive);
		InvokeGameplayCueEvent(Spec, EGameplayCueEvent::WhileActive);
	}
}

bool UAbilitySystemComponent::IsGameplayCueActive(const FGameplayTag GameplayCueTag) const
{
	return HasMatchingGameplayTag(GameplayCueTag);
}

// ----------------------------------------------------------------------------------------

void UAbilitySystemComponent::SetBaseAttributeValueFromReplication(float NewValue, FGameplayAttribute Attribute)
{
	ActiveGameplayEffects.SetBaseAttributeValueFromReplication(Attribute, NewValue);
}

bool UAbilitySystemComponent::CanApplyAttributeModifiers(const UGameplayEffect *GameplayEffect, float Level, const FGameplayEffectContextHandle& EffectContext)
{
	return ActiveGameplayEffects.CanApplyAttributeModifiers(GameplayEffect, Level, EffectContext);
}

TArray<float> UAbilitySystemComponent::GetActiveEffectsTimeRemaining(const FActiveGameplayEffectQuery Query) const
{
	return ActiveGameplayEffects.GetActiveEffectsTimeRemaining(Query);
}

TArray<float> UAbilitySystemComponent::GetActiveEffectsDuration(const FActiveGameplayEffectQuery Query) const
{
	return ActiveGameplayEffects.GetActiveEffectsDuration(Query);
}

TArray<FActiveGameplayEffectHandle> UAbilitySystemComponent::GetActiveEffects(const FActiveGameplayEffectQuery Query) const
{
	return ActiveGameplayEffects.GetActiveEffects(Query);
}

void UAbilitySystemComponent::ModifyActiveEffectStartTime(FActiveGameplayEffectHandle Handle, float StartTimeDiff)
{
	ActiveGameplayEffects.ModifyActiveEffectStartTime(Handle, StartTimeDiff);
}

void UAbilitySystemComponent::RemoveActiveEffectsWithTags(const FGameplayTagContainer Tags)
{
	AActor *OwningActor = GetOwner();
	if (OwningActor && OwningActor->Role == ROLE_Authority)
	{
		RemoveActiveEffects(FActiveGameplayEffectQuery(&Tags));
	}
}

void UAbilitySystemComponent::RemoveActiveEffects(const FActiveGameplayEffectQuery Query, int32 StacksToRemove)
{
	ActiveGameplayEffects.RemoveActiveEffects(Query, StacksToRemove);
}

// ---------------------------------------------------------------------------------------

void UAbilitySystemComponent::TaskStarted(UAbilityTask* NewTask)
{
	if (NewTask->bTickingTask)
	{
		check(TickingTasks.Contains(NewTask) == false);
		TickingTasks.Add(NewTask);

		// If this is our first ticking task, set this component as active so it begins ticking
		if (TickingTasks.Num() == 1)
		{
			UpdateShouldTick();
		}
		
	}
	if (NewTask->bSimulatedTask)
	{
		check(SimulatedTasks.Contains(NewTask) == false);
		SimulatedTasks.Add(NewTask);
	}
}

void UAbilitySystemComponent::TaskEnded(UAbilityTask* Task)
{
	if (Task->bTickingTask)
	{
		// If we are removing our last ticking task, set this component as inactive so it stops ticking
		TickingTasks.RemoveSingleSwap(Task);
		if (TickingTasks.Num() == 0)
		{
			UpdateShouldTick();
		}
	}

	if (Task->bSimulatedTask)
	{
		SimulatedTasks.RemoveSingleSwap(Task);
	}
}

// ---------------------------------------------------------------------------------------

void UAbilitySystemComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	// Intentionally not calling super: We do not want to replicate bActive which controls ticking. We sometimes need to tick on client predictively.
	

	DOREPLIFETIME(UAbilitySystemComponent, SpawnedAttributes);
	DOREPLIFETIME(UAbilitySystemComponent, ActiveGameplayEffects);
	DOREPLIFETIME(UAbilitySystemComponent, ActiveGameplayCues);
	
	DOREPLIFETIME_CONDITION(UAbilitySystemComponent, ActivatableAbilities, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UAbilitySystemComponent, BlockedAbilityBindings, COND_OwnerOnly)

	DOREPLIFETIME(UAbilitySystemComponent, OwnerActor);
	DOREPLIFETIME(UAbilitySystemComponent, AvatarActor);

	DOREPLIFETIME(UAbilitySystemComponent, ReplicatedPredictionKey);
	DOREPLIFETIME(UAbilitySystemComponent, RepAnimMontageInfo);
	
	DOREPLIFETIME_CONDITION(UAbilitySystemComponent, SimulatedTasks, COND_SkipOwner);
}

void UAbilitySystemComponent::ForceReplication()
{
	AActor *OwningActor = GetOwner();
	if (OwningActor && OwningActor->Role == ROLE_Authority)
	{
		OwningActor->ForceNetUpdate();
	}
}

bool UAbilitySystemComponent::ReplicateSubobjects(class UActorChannel *Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (const UAttributeSet* Set : SpawnedAttributes)
	{
		if (Set)
		{
			WroteSomething |= Channel->ReplicateSubobject(const_cast<UAttributeSet*>(Set), *Bunch, *RepFlags);
		}
	}

	for (UGameplayAbility* Ability : AllReplicatedInstancedAbilities)
	{
		if (Ability && !Ability->HasAnyFlags(RF_PendingKill))
		{
			WroteSomething |= Channel->ReplicateSubobject(Ability, *Bunch, *RepFlags);
		}
	}

	if (!RepFlags->bNetOwner)
	{
		for (UAbilityTask* SimulatedTask : SimulatedTasks)
		{
			if (SimulatedTask && !SimulatedTask->HasAnyFlags(RF_PendingKill))
			{
				WroteSomething |= Channel->ReplicateSubobject(SimulatedTask, *Bunch, *RepFlags);
			}
		}
	}

	return WroteSomething;
}

void UAbilitySystemComponent::GetSubobjectsWithStableNamesForNetworking(TArray<UObject*>& Objs)
{
	for (const UAttributeSet* Set : SpawnedAttributes)
	{
		if (Set && Set->IsNameStableForNetworking())
		{
			Objs.Add(const_cast<UAttributeSet*>(Set));
		}
	}
}

void UAbilitySystemComponent::PreNetReceive()
{
	ActiveGameplayEffects.IncrementLock();
}
	
void UAbilitySystemComponent::PostNetReceive()
{
	ActiveGameplayEffects.DecrementLock();
}

void UAbilitySystemComponent::OnRep_GameplayEffects()
{

}

void UAbilitySystemComponent::OnRep_PredictionKey()
{
	// Every predictive action we've done up to and including the current value of ReplicatedPredictionKey needs to be wiped
	FPredictionKeyDelegates::CatchUpTo(ReplicatedPredictionKey.Current);
}

bool UAbilitySystemComponent::HasAuthorityOrPredictionKey(const FGameplayAbilityActivationInfo* ActivationInfo) const
{
	return ((ActivationInfo->ActivationMode == EGameplayAbilityActivationMode::Authority) || CanPredict());
}

// ---------------------------------------------------------------------------------------

void UAbilitySystemComponent::PrintAllGameplayEffects() const
{
	ABILITY_LOG_SCOPE(TEXT("PrintAllGameplayEffects %s"), *GetName());
	ABILITY_LOG(Log, TEXT("Owner: %s. Avatar: %s"), *GetOwner()->GetName(), *AbilityActorInfo->AvatarActor->GetName());
	ActiveGameplayEffects.PrintAllGameplayEffects();
}

// ------------------------------------------------------------------------

void UAbilitySystemComponent::OnAttributeAggregatorDirty(FAggregator* Aggregator, FGameplayAttribute Attribute)
{
	ActiveGameplayEffects.OnAttributeAggregatorDirty(Aggregator, Attribute);
}

void UAbilitySystemComponent::OnMagnitudeDependencyChange(FActiveGameplayEffectHandle Handle, const FAggregator* ChangedAggregator)
{
	ActiveGameplayEffects.OnMagnitudeDependencyChange(Handle, ChangedAggregator);
}

void UAbilitySystemComponent::OnGameplayEffectAppliedToTarget(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle)
{
	OnGameplayEffectAppliedDelegateToTarget.Broadcast(Target, SpecApplied, ActiveHandle);
	ActiveGameplayEffects.ApplyStackingLogicPostApplyAsSource(Target, SpecApplied, ActiveHandle);
}

void UAbilitySystemComponent::OnGameplayEffectAppliedToSelf(UAbilitySystemComponent* Source, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle)
{
	OnGameplayEffectAppliedDelegateToSelf.Broadcast(Source, SpecApplied, ActiveHandle);
}

TArray<TWeakObjectPtr<UAbilityTask> >&	UAbilitySystemComponent::GetAbilityActiveTasks(UGameplayAbility* Ability)
{
	return Ability->ActiveTasks;
}

// ------------------------------------------------------------------------

FString ASC_CleanupName(FString Str)
{
	Str.RemoveFromStart(TEXT("Default__"));
	Str.RemoveFromEnd(TEXT("_c"));
	return Str;
}

void UAbilitySystemComponent::DisplayDebug(class UCanvas* Canvas, const class FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{

	bool bShowAttributes = true;
	bool bShowGameplayEffects = true;
	bool bShowAbilities = true;

	if (DebugDisplay.IsDisplayOn(FName(TEXT("Ability"))))
	{
		bShowAbilities = true;
		bShowAttributes = false;
		bShowGameplayEffects = false;
	}

	FGameplayTagContainer OwnerTags;
	GetOwnedGameplayTags(OwnerTags);

	Canvas->SetDrawColor(FColor::White);
	YL = Canvas->DrawText(GEngine->GetTinyFont(), FString::Printf(TEXT("Owned Tags: %s"), *OwnerTags.ToStringSimple()), 4.f, YPos);
	YPos += YL;

	if (BlockedAbilityTags.GetExplicitGameplayTags().Num() > 0)
	{
		YL = Canvas->DrawText(GEngine->GetTinyFont(), FString::Printf(TEXT("BlockedAbilityTags: %s"), *BlockedAbilityTags.GetExplicitGameplayTags().ToStringSimple()), 4.f, YPos);
		YPos += YL;
	}

	TSet<FGameplayAttribute> DrawAttributes;

	const float MaxCharHeight = GEngine->GetTinyFont()->GetMaxCharHeight();

	// -------------------------------------------------------------


	if (bShowGameplayEffects || bShowAttributes)
	{
		// Draw the attribute aggregator map.
		for (auto It = ActiveGameplayEffects.AttributeAggregatorMap.CreateConstIterator(); It; ++It)
		{
			FGameplayAttribute Attribute = It.Key();
			const FAggregatorRef& AggregatorRef = It.Value();
			if(AggregatorRef.Get())
			{
				FAggregator& Aggregator = *AggregatorRef.Get();

				float FinalValue = GetNumericAttribute(Attribute);
				float BaseValue = Aggregator.GetBaseValue();

				FString AttributeString = FString::Printf(TEXT("%s %.2f "), *Attribute.GetName(), GetNumericAttribute(Attribute));
				if (FMath::Abs<float>(BaseValue - FinalValue) > SMALL_NUMBER)
				{
					AttributeString += FString::Printf(TEXT(" (Base: %.2f)"), BaseValue);
				}

				Canvas->SetDrawColor(FColor::White);
				YL = Canvas->DrawText(GEngine->GetTinyFont(), AttributeString, 4.f, YPos);
				YPos += YL;

				DrawAttributes.Add(Attribute);

				for (int32 ModOpIdx = 0; ModOpIdx < ARRAY_COUNT(Aggregator.Mods); ++ModOpIdx)
				{
					for (const FAggregatorMod& Mod : Aggregator.Mods[ModOpIdx])
					{
						FAggregatorEvaluateParameters EmptyParams;
						bool IsActivelyModifyingAttribute = Mod.Qualifies(EmptyParams);
						Canvas->SetDrawColor(IsActivelyModifyingAttribute ? FColor::Yellow : FColor(128, 128, 128));

						FActiveGameplayEffect* ActiveGE = ActiveGameplayEffects.GetActiveGameplayEffect(Mod.ActiveHandle);
						FString SrcName = ActiveGE ? ActiveGE->Spec.Def->GetName() : FString(TEXT(""));

						if (IsActivelyModifyingAttribute == false)
						{
							if (Mod.SourceTagReqs) SrcName += FString::Printf(TEXT(" SourceTags: [%s] "), *Mod.SourceTagReqs->ToString());
							if (Mod.TargetTagReqs) SrcName += FString::Printf(TEXT("TargetTags: [%s]"), *Mod.TargetTagReqs->ToString());
						}

						YL = Canvas->DrawText(GEngine->GetTinyFont(), FString::Printf(TEXT("   %s\t %.2f - %s"), *EGameplayModOpToString(ModOpIdx), Mod.EvaluatedMagnitude, *SrcName), 7.f, YPos);
						YPos += YL;

					}
				}
				YPos += MaxCharHeight;
			}
		}
	}

	// -------------------------------------------------------------

	if (bShowGameplayEffects)
	{
		for (FActiveGameplayEffect& ActiveGE : &ActiveGameplayEffects)
		{
			
			Canvas->SetDrawColor(FColor::White);

			FString DurationStr = TEXT("Infinite Duration ");
			if (ActiveGE.GetDuration() > 0.f)
			{
				DurationStr = FString::Printf(TEXT("Duration: %.2f. Remaining: %.2f "), ActiveGE.GetDuration(), ActiveGE.GetTimeRemaining(GetWorld()->GetTimeSeconds()));
			}
			if (ActiveGE.GetPeriod() > 0.f)
			{
				DurationStr += FString::Printf(TEXT("Period: %.2f"), ActiveGE.GetPeriod());
			}

			FString StackString;
			if (ActiveGE.Spec.StackCount > 1)
			{
				
				if (ActiveGE.Spec.Def->StackingType == EGameplayEffectStackingType::AggregateBySource)
				{
					StackString = FString::Printf(TEXT("(Stacks: %d. From: %s) "), ActiveGE.Spec.StackCount, *GetNameSafe(ActiveGE.Spec.GetContext().GetInstigatorAbilitySystemComponent()->AvatarActor));
				}
				else
				{
					StackString = FString::Printf(TEXT("(Stacks: %d) "), ActiveGE.Spec.StackCount);
				}
			}

			Canvas->SetDrawColor(ActiveGE.bIsInhibited ? FColor(128, 128, 128): FColor::White );

			YL = Canvas->DrawText(GEngine->GetTinyFont(), FString::Printf(TEXT("%s %s %s"), *ASC_CleanupName(GetNameSafe(ActiveGE.Spec.Def)), *DurationStr, *StackString ), 4.f, YPos);
			YPos += YL;

			FGameplayTagContainer GrantedTags;
			ActiveGE.Spec.GetAllGrantedTags(GrantedTags);
			if (GrantedTags.Num() > 0)
			{
				YL = Canvas->DrawText(GEngine->GetTinyFont(), FString::Printf(TEXT("Granted Tags: %s"), *GrantedTags.ToStringSimple() ), 7.f, YPos);
				YPos += YL;
			}

			for (int32 ModIdx=0; ModIdx < ActiveGE.Spec.Modifiers.Num(); ++ModIdx)
			{
				const FModifierSpec& ModSpec = ActiveGE.Spec.Modifiers[ModIdx];
				const FGameplayModifierInfo& ModInfo = ActiveGE.Spec.Def->Modifiers[ModIdx];

				// Do a quick Qualifies() check to see if this mod is active.
				FAggregatorMod TempMod;
				TempMod.SourceTagReqs = &ModInfo.SourceTags;
				TempMod.TargetTagReqs = &ModInfo.TargetTags;
				TempMod.IsPredicted = false;

				FAggregatorEvaluateParameters EmptyParams;
				bool IsActivelyModifyingAttribute = TempMod.Qualifies(EmptyParams);

				if (IsActivelyModifyingAttribute == false)
				{
					Canvas->SetDrawColor(FColor(128, 128, 128) );
				}

				YL = Canvas->DrawText(GEngine->GetTinyFont(), FString::Printf(TEXT("Mod: %s. %s. %.2f"), *ModInfo.Attribute.GetName(), *EGameplayModOpToString(ModInfo.ModifierOp), ModSpec.GetEvaluatedMagnitude() ), 7.f, YPos);
				YPos += YL;

				Canvas->SetDrawColor(ActiveGE.bIsInhibited ? FColor(128, 128, 128): FColor::White );
			}

			YPos += MaxCharHeight;
		}
	}

	// -------------------------------------------------------------

	if (bShowAttributes)
	{
		Canvas->SetDrawColor(FColor::White);
		for (UAttributeSet* Set : SpawnedAttributes)
		{
			for (TFieldIterator<UProperty> It(Set->GetClass()); It; ++It)
			{
				FGameplayAttribute	Attribute(*It);

				if(DrawAttributes.Contains(Attribute))
					continue;

				if (Attribute.IsValid())
				{
					float Value = GetNumericAttribute(Attribute);
					YL = Canvas->DrawText(GEngine->GetTinyFont(), FString::Printf(TEXT("%s %.2f"), *Attribute.GetName(), Value ), 4.f, YPos);
					YPos += YL;
				}
			}
		}
		YPos += MaxCharHeight;
	}

	// -------------------------------------------------------------

	if (bShowAbilities)
	{
		for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
		{
			if (AbilitySpec.Ability == nullptr)
				continue;

			FString StatusText;
			FColor AbilityTextColor = FColor(128, 128, 128);
			if (AbilitySpec.IsActive())
			{
				StatusText = FString::Printf(TEXT(" (Active %d)"), AbilitySpec.ActiveCount);
				AbilityTextColor = FColor::Yellow;
			}
			else if (BlockedAbilityBindings.IsValidIndex(AbilitySpec.InputID) && BlockedAbilityBindings[AbilitySpec.InputID])
			{
				StatusText = TEXT(" (InputBlocked)");
				AbilityTextColor = FColor::Red;
			}
			else if (AbilitySpec.Ability->AbilityTags.MatchesAny(BlockedAbilityTags.GetExplicitGameplayTags(), false))
			{
				StatusText = TEXT(" (TagBlocked)");
				AbilityTextColor = FColor::Red;
			}
			else if (AbilitySpec.Ability->CanActivateAbility(AbilitySpec.Handle, AbilityActorInfo.Get()) == false)
			{
				StatusText = TEXT(" (CantActivate)");
				AbilityTextColor = FColor::Red;
			}

			FString InputPressedStr = AbilitySpec.InputPressed ? TEXT("(InputPressed)") : TEXT("");

			Canvas->SetDrawColor(AbilityTextColor);
			YL = Canvas->DrawText(GEngine->GetTinyFont(), FString::Printf(TEXT("%s %s %s"), *ASC_CleanupName(GetNameSafe(AbilitySpec.Ability)), *StatusText, *InputPressedStr), 4.f, YPos);
			YPos += YL;
	
			if (AbilitySpec.IsActive())
			{
				TArray<UGameplayAbility*> Instances = AbilitySpec.GetAbilityInstances();
				for (int32 InstanceIdx=0; InstanceIdx < Instances.Num(); ++InstanceIdx)
				{
					UGameplayAbility* Instance = Instances[InstanceIdx];
					if (!Instance)
						continue;

					Canvas->SetDrawColor(FColor::White);
					for (auto TaskPtr : Instance->ActiveTasks)
					{
						UAbilityTask* Task = TaskPtr.Get();
						if (Task)
						{
							YL = Canvas->DrawText(GEngine->GetTinyFont(), FString::Printf(TEXT("%s"), *Task->GetDebugString()), 7.f, YPos);
							YPos += YL;
						}
					}

					if (InstanceIdx < Instances.Num() - 2)
					{
						Canvas->SetDrawColor(FColor(128, 128, 128));
						YL = Canvas->DrawText(GEngine->GetTinyFont(), FString::Printf(TEXT("--------")), 7.f, YPos);
						YPos += YL;
					}
				}
			}
		}
		YPos += MaxCharHeight;
	}

	YL = MaxCharHeight;
}

#undef LOCTEXT_NAMESPACE
