// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagsModule.h"
#include "GameplayEffectTypes.h"
#include "AbilitySystemComponent.h"

float GameplayEffectUtilities::GetModifierBiasByModifierOp(EGameplayModOp::Type ModOp)
{
	static const float ModifierOpBiases[EGameplayModOp::Max] = {0.f, 1.f, 1.f, 0.f};
	check(ModOp >= 0 && ModOp < EGameplayModOp::Max);

	return ModifierOpBiases[ModOp];
}

float GameplayEffectUtilities::ComputeStackedModifierMagnitude(float BaseComputedMagnitude, int32 StackCount, EGameplayModOp::Type ModOp)
{
	const float OperationBias = GameplayEffectUtilities::GetModifierBiasByModifierOp(ModOp);

	StackCount = FMath::Clamp<int32>(StackCount, 0, StackCount);

	float StackMag = BaseComputedMagnitude;
	
	// Override modifiers don't care about stack count at all. All other modifier ops need to subtract out their bias value in order to handle
	// stacking correctly
	if (ModOp != EGameplayModOp::Override)
	{
		StackMag -= OperationBias;
		StackMag *= StackCount;
		StackMag += OperationBias;
	}

	return StackMag;
}

bool FGameplayEffectAttributeCaptureDefinition::operator==(const FGameplayEffectAttributeCaptureDefinition& Other) const
{
	return ((AttributeToCapture == Other.AttributeToCapture) && (AttributeSource == Other.AttributeSource) && (bSnapshot == Other.bSnapshot));
}

bool FGameplayEffectAttributeCaptureDefinition::operator!=(const FGameplayEffectAttributeCaptureDefinition& Other) const
{
	return ((AttributeToCapture != Other.AttributeToCapture) || (AttributeSource != Other.AttributeSource) || (bSnapshot != Other.bSnapshot));
}

FString FGameplayEffectAttributeCaptureDefinition::ToSimpleString() const
{
	return FString::Printf(TEXT("Attribute: %s, Capture: %s, Snapshot: %d"), *AttributeToCapture.GetName(), AttributeSource == EGameplayEffectAttributeCaptureSource::Source ? TEXT("Source") : TEXT("Target"), bSnapshot);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FGameplayEffectContext
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

void FGameplayEffectContext::AddInstigator(class AActor *InInstigator, class AActor *InEffectCauser)
{
	Instigator = InInstigator;
	EffectCauser = InEffectCauser;
	InstigatorAbilitySystemComponent = NULL;

	// Cache off his AbilitySystemComponent.
	IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Instigator.Get());
	if (AbilitySystemInterface)
	{
		InstigatorAbilitySystemComponent = AbilitySystemInterface->GetAbilitySystemComponent();
	}
}

void FGameplayEffectContext::AddActors(TArray<TWeakObjectPtr<AActor>> InActors, bool bReset)
{
	if (bReset && Actors.Num())
	{
		Actors.Reset();
	}

	Actors.Append(InActors);
}

void FGameplayEffectContext::AddHitResult(const FHitResult InHitResult, bool bReset)
{
	if (bReset && HitResult.IsValid())
	{
		HitResult.Reset();
		bHasWorldOrigin = false;
	}

	check(!HitResult.IsValid());
	HitResult = TSharedPtr<FHitResult>(new FHitResult(InHitResult));
	if (bHasWorldOrigin == false)
	{
		AddOrigin(InHitResult.TraceStart);
	}
}

bool FGameplayEffectContext::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	uint8 RepBits = 0;
	if (Ar.IsSaving())
	{
		if (Instigator.IsValid() )
		{
			RepBits |= 1 << 0;
		}
		if (EffectCauser.IsValid() )
		{
			RepBits |= 1 << 1;
		}
		if (SourceObject.IsValid() )
		{
			RepBits |= 1 << 2;
		}
		if (Actors.Num() > 0)
		{
			RepBits |= 1 << 3;
		}
		if (HitResult.IsValid())
		{
			RepBits |= 1 << 4;
		}
		if (bHasWorldOrigin)
		{
			RepBits |= 1 << 5;
		}
	}

	Ar.SerializeBits(&RepBits, 6);

	if (RepBits & (1 << 0))
	{
		Ar << Instigator;
	}
	if (RepBits & (1 << 1))
	{
		Ar << EffectCauser;
	}
	if (RepBits & (1 << 2))
	{
		Ar << SourceObject;
	}
	if (RepBits & (1 << 3))
	{
		Ar << Actors;;
	}
	if (RepBits & (1 << 4))
	{
		if (Ar.IsLoading())
		{
			if (!HitResult.IsValid())
			{
				HitResult = TSharedPtr<FHitResult>(new FHitResult());
			}
		}
		HitResult->NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << 5))
	{
		Ar << WorldOrigin;
		bHasWorldOrigin = true;
	}
	else
	{
		bHasWorldOrigin = false;
	}

	if (Ar.IsLoading())
	{
		AddInstigator(Instigator.Get(), EffectCauser.Get()); // Just to initialize InstigatorAbilitySystemComponent
	}	
	
	bOutSuccess = true;
	return true;
}

bool FGameplayEffectContext::IsLocallyControlled() const
{
	APawn* Pawn = Cast<APawn>(Instigator.Get());
	if (!Pawn)
	{
		Pawn = Cast<APawn>(EffectCauser.Get());
	}
	if (Pawn)
	{
		return Pawn->IsLocallyControlled();
	}
	return false;
}

void FGameplayEffectContext::AddOrigin(FVector InOrigin)
{
	bHasWorldOrigin = true;
	WorldOrigin = InOrigin;
}

void FGameplayEffectContext::GetOwnedGameplayTags(OUT FGameplayTagContainer& ActorTagContainer, OUT FGameplayTagContainer& SpecTagContainer) const
{
	IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Instigator.Get());
	if (TagInterface)
	{
		TagInterface->GetOwnedGameplayTags(ActorTagContainer);
	}
	else if (InstigatorAbilitySystemComponent.IsValid())
	{
		InstigatorAbilitySystemComponent->GetOwnedGameplayTags(ActorTagContainer);
	}
}

bool FGameplayEffectContextHandle::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	bool ValidData = Data.IsValid();
	Ar.SerializeBits(&ValidData,1);

	if (ValidData)
	{
		if (Ar.IsLoading())
		{
			// For now, just always reset/reallocate the data when loading.
			// Longer term if we want to generalize this and use it for property replication, we should support
			// only reallocating when necessary
			
			if (Data.IsValid() == false)
			{
				Data = TSharedPtr<FGameplayEffectContext>(UAbilitySystemGlobals::Get().AllocGameplayEffectContext());
			}
		}

		void* ContainerPtr = Data.Get();
		UScriptStruct* ScriptStruct = Data->GetScriptStruct();

		if (ScriptStruct->StructFlags & STRUCT_NetSerializeNative)
		{
			ScriptStruct->GetCppStructOps()->NetSerialize(Ar, Map, bOutSuccess, Data.Get());
		}
		else
		{
			// This won't work since UStructProperty::NetSerializeItem is deprecrated.
			//	1) we have to manually crawl through the topmost struct's fields since we don't have a UStructProperty for it (just the UScriptProperty)
			//	2) if there are any UStructProperties in the topmost struct's fields, we will assert in UStructProperty::NetSerializeItem.

			ABILITY_LOG(Fatal, TEXT("FGameplayEffectContextHandle::NetSerialize called on data struct %s without a native NetSerialize"), *ScriptStruct->GetName());
		}
	}

	bOutSuccess = true;
	return true;
}


// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	Misc
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

FString EGameplayModOpToString(int32 Type)
{
	static UEnum *e = FindObject<UEnum>(ANY_PACKAGE, TEXT("EGameplayModOp"));
	return e->GetEnumName(Type);
}

FString EGameplayModEffectToString(int32 Type)
{
	static UEnum *e = FindObject<UEnum>(ANY_PACKAGE, TEXT("EGameplayModEffect"));
	return e->GetEnumName(Type);
}

FString EGameplayCueEventToString(int32 Type)
{
	static UEnum *e = FindObject<UEnum>(ANY_PACKAGE, TEXT("EGameplayCueEvent"));
	return e->GetEnumName(Type);
}

bool FGameplayTagCountContainer::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	return ExplicitTags.HasTag(TagToCheck, EGameplayTagMatchType::IncludeParentTags, EGameplayTagMatchType::Explicit);
}

bool FGameplayTagCountContainer::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer, bool bCountEmptyAsMatch) const
{
	return ExplicitTags.MatchesAll(TagContainer, bCountEmptyAsMatch);
}


bool FGameplayTagCountContainer::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer, bool bCountEmptyAsMatch) const
{
	return ExplicitTags.MatchesAny(TagContainer, bCountEmptyAsMatch);
}

void FGameplayTagCountContainer::UpdateTagCount(const FGameplayTag& Tag, int32 CountDelta)
{
	if (CountDelta != 0)
	{
		UpdateTagMap_Internal(Tag, CountDelta);
	}
}

void FGameplayTagCountContainer::UpdateTagCount(const FGameplayTagContainer& Container, int32 CountDelta)
{	
	if (CountDelta != 0)
	{
		for (auto TagIt = Container.CreateConstIterator(); TagIt; ++TagIt)
		{
			UpdateTagMap_Internal(*TagIt, CountDelta);
		}
	}
}

FOnGameplayEffectTagCountChanged& FGameplayTagCountContainer::RegisterGameplayTagEvent(const FGameplayTag& Tag)
{
	return GameplayTagEventMap.FindOrAdd(Tag);
}

FOnGameplayEffectTagCountChanged& FGameplayTagCountContainer::RegisterGenericGameplayEvent()
{
	return OnAnyTagChangeDelegate;
}

const FGameplayTagContainer& FGameplayTagCountContainer::GetExplicitGameplayTags() const
{
	return ExplicitTags;
}

void FGameplayTagCountContainer::UpdateTagMap_Internal(const FGameplayTag& Tag, int32 CountDelta)
{
	const bool bTagAlreadyExplicitlyExists = ExplicitTags.HasTag(Tag, EGameplayTagMatchType::Explicit, EGameplayTagMatchType::Explicit);

	// Need special case handling to maintain the explicit tag list correctly, adding the tag to the list if it didn't previously exist and a
	// positive delta comes in, and removing it from the list if it did exist and a negative delta comes in.
	if (!bTagAlreadyExplicitlyExists)
	{
		// Brand new tag with a positive delta needs to be explicitly added
		if (CountDelta > 0)
		{
			ExplicitTags.AddTag(Tag);
		}
		// Block attempted reduction of non-explicit tags, as they were never truly added to the container directly
		else
		{
			// only warn about tags that are in the container but will not be removed because they aren't explicitly in the container
			if (ExplicitTags.HasTag(Tag, EGameplayTagMatchType::IncludeParentTags, EGameplayTagMatchType::Explicit))
			{
				ABILITY_LOG(Warning, TEXT("Attempted to remove tag: %s from tag count container, but it is not explicitly in the container!"), *Tag.ToString());
			}
			return;
		}
	}

	// Update the explicit tag count map. This has to be separate than the map below because otherwise the count of nested tags ends up wrong
	int32& ExistingCount = ExplicitTagCountMap.FindOrAdd(Tag);

	ExistingCount = FMath::Max(ExistingCount + CountDelta, 0);

	// If our new count is 0, remove us from the explicit tag list
	if (ExistingCount <= 0)
	{
		// Remove from the explicit list
		ExplicitTags.RemoveTag(Tag);
	}

	// Check if change delegates are required to fire for the tag or any of its parents based on the count change
	FGameplayTagContainer TagAndParentsContainer = IGameplayTagsModule::Get().GetGameplayTagsManager().RequestGameplayTagParents(Tag);
	for (auto CompleteTagIt = TagAndParentsContainer.CreateConstIterator(); CompleteTagIt; ++CompleteTagIt)
	{
		const FGameplayTag& CurTag = *CompleteTagIt;

		// Get the current count of the specified tag. NOTE: Stored as a reference, so subsequent changes propogate to the map.
		int32& TagCount = GameplayTagCountMap.FindOrAdd(CurTag);

		const int32 OldCount = TagCount;

		// Apply the delta to the count in the map
		TagCount = FMath::Max(TagCount + CountDelta, 0);

		// If a significant change (new addition or total removal) occurred, trigger related delegates
		if (OldCount == 0 || TagCount == 0)
		{
			OnAnyTagChangeDelegate.Broadcast(CurTag, TagCount);

			FOnGameplayEffectTagCountChanged* CountChangeDelegate = GameplayTagEventMap.Find(CurTag);
			if (CountChangeDelegate)
			{
				CountChangeDelegate->Broadcast(CurTag, TagCount);
			}
		}
	}
}

bool FGameplayTagRequirements::RequirementsMet(const FGameplayTagContainer& Container) const
{
	bool HasRequired = Container.MatchesAll(RequireTags, true);
	bool HasIgnored = Container.MatchesAny(IgnoreTags, false);

	return HasRequired && !HasIgnored;
}

bool FGameplayTagRequirements::IsEmpty() const
{
	return (RequireTags.Num() == 0 && IgnoreTags.Num() == 0);
}

FString FGameplayTagRequirements::ToString() const
{
	FString Str;

	if (RequireTags.Num() > 0)
	{
		Str += FString::Printf(TEXT("require: %s "), *RequireTags.ToStringSimple());
	}
	if (IgnoreTags.Num() >0)
	{
		Str += FString::Printf(TEXT("ignore: %s "), *IgnoreTags.ToStringSimple());
	}

	return Str;
}

void FActiveGameplayEffectsContainer::PrintAllGameplayEffects() const
{
	ABILITY_LOG_SCOPE(TEXT("ActiveGameplayEffects. Num: %d"), GetNumGameplayEffects());

	for (const FActiveGameplayEffect& Effect : this)
	{
		Effect.PrintAll();
	}
}

void FActiveGameplayEffect::PrintAll() const
{
	ABILITY_LOG(Log, TEXT("Handle: %s"), *Handle.ToString());
	ABILITY_LOG(Log, TEXT("StartWorldTime: %.2f"), StartWorldTime);
	Spec.PrintAll();
}

void FGameplayEffectSpec::PrintAll() const
{
	ABILITY_LOG_SCOPE(TEXT("GameplayEffectSpec"));
	ABILITY_LOG(Log, TEXT("Def: %s"), *Def->GetName());

	ABILITY_LOG(Log, TEXT("Duration: %.2f"), GetDuration());

	ABILITY_LOG(Log, TEXT("Period: %.2f"), GetPeriod());

	ABILITY_LOG(Log, TEXT("Modifiers:"));
}

const FGameplayTagContainer* FTagContainerAggregator::GetAggregatedTags() const
{
	if (CacheIsValid == false)
	{
		CacheIsValid = true;
		CachedAggregator.RemoveAllTags(CapturedActorTags.Num() + CapturedSpecTags.Num() + ScopedTags.Num());
		CachedAggregator.AppendTags(CapturedActorTags);
		CachedAggregator.AppendTags(CapturedSpecTags);
		CachedAggregator.AppendTags(ScopedTags);
	}

	return &CachedAggregator;
}

FGameplayTagContainer& FTagContainerAggregator::GetActorTags()
{
	CacheIsValid = false;
	return CapturedActorTags;
}

const FGameplayTagContainer& FTagContainerAggregator::GetActorTags() const
{
	return CapturedActorTags;
}

FGameplayTagContainer& FTagContainerAggregator::GetSpecTags()
{
	CacheIsValid = false;
	return CapturedSpecTags;
}

const FGameplayTagContainer& FTagContainerAggregator::GetSpecTags() const
{
	CacheIsValid = false;
	return CapturedSpecTags;
}


FGameplayEffectSpecHandle::FGameplayEffectSpecHandle()
{

}

FGameplayEffectSpecHandle::FGameplayEffectSpecHandle(FGameplayEffectSpec* DataPtr)
	: Data(DataPtr)
{

}