// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagAssetInterface.h"
#include "AttributeSet.h"
#include "GameplayPrediction.h"
#include "AbilitySystemLog.h"
#include "GameplayEffectTypes.generated.h"

#define SKILL_SYSTEM_AGGREGATOR_DEBUG 1

#if SKILL_SYSTEM_AGGREGATOR_DEBUG
	#define SKILL_AGG_DEBUG( Format, ... ) *FString::Printf(Format, ##__VA_ARGS__)
#else
	#define SKILL_AGG_DEBUG( Format, ... ) NULL
#endif

class UAbilitySystemComponent;
class UGameplayEffect;
class UGameplayAbility;

struct FGameplayEffectSpec;
struct FGameplayEffectModCallbackData;

GAMEPLAYABILITIES_API FString EGameplayModOpToString(int32 Type);

GAMEPLAYABILITIES_API FString EGameplayModToString(int32 Type);

GAMEPLAYABILITIES_API FString EGameplayModEffectToString(int32 Type);

GAMEPLAYABILITIES_API FString EGameplayCueEventToString(int32 Type);

UENUM(BlueprintType)
namespace EGameplayModOp
{
	enum Type
	{		
		// Numeric
		Additive = 0		UMETA(DisplayName="Add"),
		Multiplicitive		UMETA(DisplayName="Multiply"),
		Division			UMETA(DisplayName="Divide"),

		// Other
		Override 			UMETA(DisplayName="Override"),	// This should always be the first non numeric ModOp

		// This must always be at the end
		Max					UMETA(DisplayName="Invalid")
	};
}

namespace GameplayEffectUtilities
{
	/**
	 * Helper function to retrieve the modifier bias based upon modifier operation
	 * 
	 * @param ModOp	Modifier operation to retrieve the modifier bias for
	 * 
	 * @return Modifier bias for the specified operation
	 */
	GAMEPLAYABILITIES_API float GetModifierBiasByModifierOp(EGameplayModOp::Type ModOp);

	/**
	 * Helper function to compute the stacked modifier magnitude from a base magnitude, given a stack count and modifier operation
	 * 
	 * @param BaseComputedMagnitude	Base magnitude to compute from
	 * @param StackCount			Stack count to use for the calculation
	 * @param ModOp					Modifier operation to use
	 * 
	 * @return Computed modifier magnitude with stack count factored in
	 */
	GAMEPLAYABILITIES_API float ComputeStackedModifierMagnitude(float BaseComputedMagnitude, int32 StackCount, EGameplayModOp::Type ModOp);
}


/** Enumeration for options of where to capture gameplay attributes from for gameplay effects */
UENUM()
enum class EGameplayEffectAttributeCaptureSource : uint8
{
	// Source (caster) of the gameplay effect
	Source,	
	// Target (recipient) of the gameplay effect
	Target	
};

/** Enumeration for ways a single GameplayEffect asset can stack */
UENUM()
enum class EGameplayEffectStackingType : uint8
{
	// No stacking. Multiple applications of this GameplayEffect are treated as separate instances.
	None,
	// Each caster has its own stack
	AggregateBySource,
	// Each target has its own stack
	AggregateByTarget,
};

/**
 * This handle is required for things outside of FActiveGameplayEffectsContainer to refer to a specific active GameplayEffect
 *	For example if a skill needs to create an active effect and then destroy that specific effect that it created, it has to do so
 *	through a handle. a pointer or index into the active list is not sufficient.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FActiveGameplayEffectHandle
{
	GENERATED_USTRUCT_BODY()

	FActiveGameplayEffectHandle()
		: Handle(INDEX_NONE)
	{

	}

	FActiveGameplayEffectHandle(int32 InHandle)
		: Handle(InHandle)
	{

	}

	bool IsValid() const
	{
		return Handle != INDEX_NONE;
	}

	static FActiveGameplayEffectHandle GenerateNewHandle(UAbilitySystemComponent* OwningComponent);

	UAbilitySystemComponent* GetOwningAbilitySystemComponent();
	const UAbilitySystemComponent* GetOwningAbilitySystemComponent() const;

	void RemoveFromGlobalMap();

	bool operator==(const FActiveGameplayEffectHandle& Other) const
	{
		return Handle == Other.Handle;
	}

	bool operator!=(const FActiveGameplayEffectHandle& Other) const
	{
		return Handle != Other.Handle;
	}

	friend uint32 GetTypeHash(const FActiveGameplayEffectHandle& InHandle)
	{
		return InHandle.Handle;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%d"), Handle);
	}

private:

	UPROPERTY()
	int32 Handle;
};

USTRUCT()
struct FGameplayModifierEvaluatedData
{
	GENERATED_USTRUCT_BODY()

	FGameplayModifierEvaluatedData()
		: Attribute()
		, ModifierOp(EGameplayModOp::Additive)
		, Magnitude(0.f)
		, IsValid(false)
	{
	}

	FGameplayModifierEvaluatedData(const FGameplayAttribute& InAttribute, TEnumAsByte<EGameplayModOp::Type> InModOp, float InMagnitude, FActiveGameplayEffectHandle InHandle = FActiveGameplayEffectHandle())
		: Attribute(InAttribute)
		, ModifierOp(InModOp)
		, Magnitude(InMagnitude)
		, Handle(InHandle)
		, IsValid(true)
	{
	}

	UPROPERTY()
	FGameplayAttribute Attribute;

	/** The numeric operation of this modifier: Override, Add, Multiply, etc  */
	UPROPERTY()
	TEnumAsByte<EGameplayModOp::Type> ModifierOp;

	UPROPERTY()
	float Magnitude;

	/** Handle of the active gameplay effect that originated us. Will be invalid in many cases */
	UPROPERTY()
	FActiveGameplayEffectHandle	Handle;

	UPROPERTY()
	bool IsValid;

	FString ToSimpleString() const
	{
		return FString::Printf(TEXT("%s %s EvalMag: %f"), *Attribute.GetName(), *EGameplayModOpToString(ModifierOp), Magnitude);
	}
};

/** Struct defining gameplay attribute capture options for gameplay effects */
USTRUCT()
struct GAMEPLAYABILITIES_API FGameplayEffectAttributeCaptureDefinition
{
	GENERATED_USTRUCT_BODY()

	FGameplayEffectAttributeCaptureDefinition()
	{

	}

	FGameplayEffectAttributeCaptureDefinition(FGameplayAttribute InAttribute, EGameplayEffectAttributeCaptureSource InSource, bool InSnapshot)
		: AttributeToCapture(InAttribute), AttributeSource(InSource), bSnapshot(InSnapshot)
	{

	}

	/** Gameplay attribute to capture */
	UPROPERTY(EditDefaultsOnly, Category=Capture)
	FGameplayAttribute AttributeToCapture;

	/** Source of the gameplay attribute */
	UPROPERTY(EditDefaultsOnly, Category=Capture)
	EGameplayEffectAttributeCaptureSource AttributeSource;

	/** Whether the attribute should be snapshotted or not */
	UPROPERTY(EditDefaultsOnly, Category=Capture)
	bool bSnapshot;

	/** Equality/Inequality operators */
	bool operator==(const FGameplayEffectAttributeCaptureDefinition& Other) const;
	bool operator!=(const FGameplayEffectAttributeCaptureDefinition& Other) const;

	/**
	 * Get type hash for the capture definition; Implemented to allow usage in TMap
	 *
	 * @param CaptureDef Capture definition to get the type hash of
	 */
	friend uint32 GetTypeHash(const FGameplayEffectAttributeCaptureDefinition& CaptureDef)
	{
		uint32 Hash = 0;
		Hash = HashCombine(Hash, GetTypeHash(CaptureDef.AttributeToCapture));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(CaptureDef.AttributeSource)));
		Hash = HashCombine(Hash, GetTypeHash(CaptureDef.bSnapshot));
		return Hash;
	}

	FString ToSimpleString() const;
};

/**
 * FGameplayEffectContext
 *	Data struct for an instigator and related data. This is still being fleshed out. We will want to track actors but also be able to provide some level of tracking for actors that are destroyed.
 *	We may need to store some positional information as well.
 */
USTRUCT()
struct GAMEPLAYABILITIES_API FGameplayEffectContext
{
	GENERATED_USTRUCT_BODY()

	FGameplayEffectContext()
		: bHasWorldOrigin(false)
	{
	}

	FGameplayEffectContext(AActor* InInstigator, AActor* InEffectCauser)
		: bHasWorldOrigin(false)
	{
		AddInstigator(InInstigator, InEffectCauser);
	}

	virtual ~FGameplayEffectContext()
	{
	}

	/** Returns the list of gameplay tags applicable to this effect, defaults to the owner's tags */
	virtual void GetOwnedGameplayTags(OUT FGameplayTagContainer& ActorTagContainer, OUT FGameplayTagContainer& SpecTagContainer) const;

	/** Sets the instigator and effect causer. Instigator is who owns the ability that spawned this, EffectCauser is the actor that is the physical source of the effect, such as a weapon. They can be the same. */
	virtual void AddInstigator(class AActor *InInstigator, class AActor *InEffectCauser);

	/** Returns the immediate instigator that applied this effect */
	virtual AActor* GetInstigator() const
	{
		return Instigator.Get();
	}

	/** Returns the ability system component of the instigator of this effect */
	virtual UAbilitySystemComponent* GetInstigatorAbilitySystemComponent() const
	{
		return InstigatorAbilitySystemComponent.Get();
	}

	/** Returns the physical actor tied to the application of this effect */
	virtual AActor* GetEffectCauser() const
	{
		return EffectCauser.Get();
	}

	/** Should always return the original instigator that started the whole chain. Subclasses can override what this does */
	virtual AActor* GetOriginalInstigator() const
	{
		return Instigator.Get();
	}

	/** Returns the ability system component of the instigator that started the whole chain */
	virtual UAbilitySystemComponent* GetOriginalInstigatorAbilitySystemComponent() const
	{
		return InstigatorAbilitySystemComponent.Get();
	}

	/** Sets the object this effect was created from. */
	virtual void AddSourceObject(const UObject* NewSourceObject)
	{
		SourceObject = NewSourceObject;
	}

	/** Returns the object this effect was created from. */
	virtual const UObject* GetSourceObject() const
	{
		return SourceObject.Get();
	}

	virtual void AddActors(TArray<TWeakObjectPtr<AActor>> InActor, bool bReset = false);

	virtual void AddHitResult(const FHitResult InHitResult, bool bReset = false);

	virtual const TArray<TWeakObjectPtr<AActor>> GetActors() const
	{
		return Actors;
	}

	virtual const FHitResult* GetHitResult() const
	{
		if (HitResult.IsValid())
		{
			return HitResult.Get();
		}
		return NULL;
	}

	virtual void AddOrigin(FVector InOrigin);

	virtual const FVector& GetOrigin() const
	{
		return WorldOrigin;
	}

	virtual bool HasOrigin() const
	{
		return bHasWorldOrigin;
	}

	virtual FString ToString() const
	{
		return Instigator.IsValid() ? Instigator->GetName() : FString(TEXT("NONE"));
	}

	virtual UScriptStruct* GetScriptStruct() const
	{
		return FGameplayEffectContext::StaticStruct();
	}

	/** Creates a copy of this context, used to duplicate for later modifications */
	virtual FGameplayEffectContext* Duplicate() const
	{
		FGameplayEffectContext* NewContext = new FGameplayEffectContext();
		*NewContext = *this;
		NewContext->AddActors(Actors);
		if (GetHitResult())
		{
			// Does a deep copy of the hit result
			NewContext->AddHitResult(*GetHitResult());
		}
		return NewContext;
	}

	virtual bool IsLocallyControlled() const;

	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

protected:
	// The object pointers here have to be weak because contexts aren't necessarily tracked by GC in all cases

	/** Instigator actor, the actor that owns the ability system component */
	UPROPERTY()
	TWeakObjectPtr<AActor> Instigator;

	/** The physical actor that actually did the damage, can be a weapon or projectile */
	UPROPERTY()
	TWeakObjectPtr<AActor> EffectCauser;

	/** Object this effect was created from, can be an actor or static object. Useful to bind an effect to a gameplay object */
	UPROPERTY()
	TWeakObjectPtr<const UObject> SourceObject;

	/** The ability system component that's bound to instigator */
	UPROPERTY(NotReplicated)
	TWeakObjectPtr<UAbilitySystemComponent> InstigatorAbilitySystemComponent;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Actors;

	/** Trace information - may be NULL in many cases */
	TSharedPtr<FHitResult>	HitResult;

	UPROPERTY()
	FVector	WorldOrigin;

	UPROPERTY()
	bool bHasWorldOrigin;
};

template<>
struct TStructOpsTypeTraits< FGameplayEffectContext > : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true		// Necessary so that TSharedPtr<FHitResult> Data is copied around
	};
};

/**
 * Handle that wraps a FGameplayEffectContext or subclass, to allow it to be polymorphic and replicate properly
 */
USTRUCT(BlueprintType)
struct FGameplayEffectContextHandle
{
	GENERATED_USTRUCT_BODY()

	FGameplayEffectContextHandle()
	{
	}

	/** Constructs from an existing context, should be allocated by new */
	explicit FGameplayEffectContextHandle(FGameplayEffectContext* DataPtr)
	{
		Data = TSharedPtr<FGameplayEffectContext>(DataPtr);
	}

	/** Sets from an existing context, should be allocated by new */
	void operator=(FGameplayEffectContext* DataPtr)
	{
		Data = TSharedPtr<FGameplayEffectContext>(DataPtr);
	}

	void Clear()
	{
		Data.Reset();
	}

	bool IsValid() const
	{
		return Data.IsValid();
	}

	FGameplayEffectContext* Get()
	{
		return IsValid() ? Data.Get() : NULL;
	}

	const FGameplayEffectContext* Get() const
	{
		return IsValid() ? Data.Get() : NULL;
	}

	/** Returns the list of gameplay tags applicable to this effect, defaults to the owner's tags */
	void GetOwnedGameplayTags(OUT FGameplayTagContainer& ActorTagContainer, OUT FGameplayTagContainer& SpecTagContainer) const
	{
		if (IsValid())
		{
			Data->GetOwnedGameplayTags(ActorTagContainer, SpecTagContainer);
		}
	}

	/** Sets the instigator and effect causer. Instigator is who owns the ability that spawned this, EffectCauser is the actor that is the physical source of the effect, such as a weapon. They can be the same. */
	void AddInstigator(class AActor *InInstigator, class AActor *InEffectCauser)
	{
		if (IsValid())
		{
			Data->AddInstigator(InInstigator, InEffectCauser);
		}
	}

	/** Returns the immediate instigator that applied this effect */
	virtual AActor* GetInstigator() const
	{
		if (IsValid())
		{
			return Data->GetInstigator();
		}
		return NULL;
	}

	/** Returns the ability system component of the instigator of this effect */
	virtual UAbilitySystemComponent* GetInstigatorAbilitySystemComponent() const
	{
		if (IsValid())
		{
			return Data->GetInstigatorAbilitySystemComponent();
		}
		return NULL;
	}

	/** Returns the physical actor tied to the application of this effect */
	virtual AActor* GetEffectCauser() const
	{
		if (IsValid())
		{
			return Data->GetEffectCauser();
		}
		return NULL;
	}

	/** Should always return the original instigator that started the whole chain. Subclasses can override what this does */
	AActor* GetOriginalInstigator() const
	{
		if (IsValid())
		{
			return Data->GetOriginalInstigator();
		}
		return NULL;
	}

	/** Returns the ability system component of the instigator that started the whole chain */
	UAbilitySystemComponent* GetOriginalInstigatorAbilitySystemComponent() const
	{
		if (IsValid())
		{
			return Data->GetOriginalInstigatorAbilitySystemComponent();
		}
		return NULL;
	}

	/** Sets the object this effect was created from. */
	void AddSourceObject(const UObject* NewSourceObject)
	{
		if (IsValid())
		{
			Data->AddSourceObject(NewSourceObject);
		}
	}

	/** Returns the object this effect was created from. */
	const UObject* GetSourceObject() const
	{
		if (IsValid())
		{
			return Data->GetSourceObject();
		}
		return NULL;
	}

	/** Returns if the instigator is locally controlled */
	bool IsLocallyControlled() const
	{
		if (IsValid())
		{
			return Data->IsLocallyControlled();
		}
		return false;
	}

	void AddActors(TArray<TWeakObjectPtr<AActor>> InActors, bool bReset = false)
	{
		if (IsValid())
		{
			Data->AddActors(InActors, bReset);
		}
	}

	void AddHitResult(const FHitResult InHitResult, bool bReset = false)
	{
		if (IsValid())
		{
			Data->AddHitResult(InHitResult, bReset);
		}
	}

	TArray<TWeakObjectPtr<AActor>> GetActors()
	{
		return Data->GetActors();
	}

	const FHitResult* GetHitResult() const
	{
		if (IsValid())
		{
			return Data->GetHitResult();
		}
		return NULL;
	}

	void AddOrigin(FVector InOrigin)
	{
		if (IsValid())
		{
			Data->AddOrigin(InOrigin);
		}
	}

	virtual const FVector& GetOrigin() const
	{
		if (IsValid())
		{
			return Data->GetOrigin();
		}
		return FVector::ZeroVector;
	}

	virtual bool HasOrigin() const
	{
		if (IsValid())
		{
			return Data->HasOrigin();
		}
		return false;
	}

	FString ToString() const
	{
		return IsValid() ? Data->ToString() : FString(TEXT("NONE"));
	}

	/** Creates a deep copy of this handle, used before modifying */
	FGameplayEffectContextHandle Duplicate() const
	{
		if (IsValid())
		{
			FGameplayEffectContext* NewContext = Data->Duplicate();
			return FGameplayEffectContextHandle(NewContext);
		}
		else
		{
			return FGameplayEffectContextHandle();
		}
	}

	/** Comparison operator */
	bool operator==(FGameplayEffectContextHandle const& Other) const
	{
		if (Data.IsValid() != Other.Data.IsValid())
		{
			return false;
		}
		if (Data.Get() != Other.Data.Get())
		{
			return false;
		}
		return true;
	}

	/** Comparison operator */
	bool operator!=(FGameplayEffectContextHandle const& Other) const
	{
		return !(FGameplayEffectContextHandle::operator==(Other));
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

private:

	TSharedPtr<FGameplayEffectContext> Data;
};

template<>
struct TStructOpsTypeTraits<FGameplayEffectContextHandle> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithCopy = true,		// Necessary so that TSharedPtr<FGameplayEffectContext> Data is copied around
		WithNetSerializer = true,
		WithIdenticalViaEquality = true,
	};
};

// -----------------------------------------------------------


USTRUCT(BlueprintType)
struct FGameplayCueParameters
{
	GENERATED_USTRUCT_BODY()

	FGameplayCueParameters()
	: NormalizedMagnitude(0.0f)
	, RawMagnitude(0.0f)
	, MatchedTagName(NAME_None)
	{}

	/** Magnitude of source gameplay effect, normalzed from 0-1. Use this for "how strong is the gameplay effect" (0=min, 1=,max) */
	UPROPERTY(BlueprintReadWrite, Category=GameplayCue)
	float NormalizedMagnitude;

	/** Raw final magnitude of source gameplay effect. Use this is you need to display numbers or for other informational purposes. */
	UPROPERTY(BlueprintReadWrite, Category=GameplayCue)
	float RawMagnitude;

	/** Effect context, contains information about hit reslt, etc */
	UPROPERTY(BlueprintReadWrite, Category=GameplayCue)
	FGameplayEffectContextHandle EffectContext;

	/** The tag name that matched this specific gameplay cue handler */
	UPROPERTY(BlueprintReadWrite, Category=GameplayCue)
	FName MatchedTagName;

	/** The original tag of the gameplay cue */
	UPROPERTY(BlueprintReadWrite, Category=GameplayCue)
	FGameplayTag OriginalTag;

	/** The aggregated source tags taken from the effect spec */
	UPROPERTY(BlueprintReadWrite, Category=GameplayCue)
	FGameplayTagContainer AggregatedSourceTags;

	/** The aggregated target tags taken from the effect spec */
	UPROPERTY(BlueprintReadWrite, Category=GameplayCue)
	FGameplayTagContainer AggregatedTargetTags;
};

UENUM(BlueprintType)
namespace EGameplayCueEvent
{
	enum Type
	{
		OnActive,		// Called when GameplayCue is activated.
		WhileActive,	// Called when GameplayCue is active, even if it wasn't actually just applied (Join in progress, etc)
		Executed,		// Called when a GameplayCue is executed: instant effects or periodic tick
		Removed			// Called when GameplayCue is removed
	};
}

DECLARE_DELEGATE_OneParam(FOnGameplayAttributeEffectExecuted, struct FGameplayModifierEvaluatedData&);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGameplayEffectTagCountChanged, const FGameplayTag, int32);

DECLARE_MULTICAST_DELEGATE(FOnActiveGameplayEffectRemoved);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGameplayAttributeChange, float, const FGameplayEffectModCallbackData*);

DECLARE_DELEGATE_RetVal(FGameplayTagContainer, FGetGameplayTags);

DECLARE_DELEGATE_RetVal_OneParam(FOnGameplayEffectTagCountChanged&, FRegisterGameplayTagChangeDelegate, FGameplayTag);

// -----------------------------------------------------------

/**
 * Struct that tracks the number/count of tag applications within it. Explicitly tracks the tags added or removed,
 * while simultaneously tracking the count of parent tags as well. Events/delegates are fired whenever the tag counts
 * of any tag (explicit or parent) are modified.
 */
struct GAMEPLAYABILITIES_API FGameplayTagCountContainer
{
	// Constructor
	FGameplayTagCountContainer()
	{}

	/**
	 * Check if the count container has a gameplay tag that matches against the specified tag (expands to include parents of asset tags)
	 * 
	 * @param TagToCheck	Tag to check for a match
	 * 
	 * @return True if the count container has a gameplay tag that matches, false if not
	 */
	bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const;

	/**
	 * Check if the count container has gameplay tags that matches against all of the specified tags (expands to include parents of asset tags)
	 * 
	 * @param TagContainer			Tag container to check for a match
	 * @param bCountEmptyAsMatch	If true, the parameter tag container will count as matching, even if it's empty
	 * 
	 * @return True if the count container matches all of the gameplay tags
	 */
	bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer, bool bCountEmptyAsMatch = true) const;
	
	/**
	 * Check if the count container has gameplay tags that matches against any of the specified tags (expands to include parents of asset tags)
	 * 
	 * @param TagContainer			Tag container to check for a match
	 * @param bCountEmptyAsMatch	If true, the parameter tag container will count as matching, even if it's empty
	 * 
	 * @return True if the count container matches any of the gameplay tags
	 */
	bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer, bool bCountEmptyAsMatch = true) const;
	
	/**
	 * Update the specified container of tags by the specified delta, potentially causing an additional or removal from the explicit tag list
	 * 
	 * @param Container		Container of tags to update
	 * @param CountDelta	Delta of the tag count to apply
	 */
	void UpdateTagCount(const FGameplayTagContainer& Container, int32 CountDelta);
	
	/**
	 * Update the specified tag by the specified delta, potentially causing an additional or removal from the explicit tag list
	 * 
	 * @param Tag			Tag to update
	 * @param CountDelta	Delta of the tag count to apply
	 */
	void UpdateTagCount(const FGameplayTag& Tag, int32 CountDelta);

	/**
	 * Return delegate that can be bound to for when the specific tag's count changes to or off of zero
	 *
	 * @param Tag	Tag to get a delegate for
	 * 
	 * @return Delegate for when the specified tag's count changes to or off of zero
	 */
	FOnGameplayEffectTagCountChanged& RegisterGameplayTagEvent(const FGameplayTag& Tag);
	
	/**
	 * Return delegate that can be bound to for when the any tag's count changes to or off of zero
	 * 
	 * @return Delegate for when any tag's count changes to or off of zero
	 */
	FOnGameplayEffectTagCountChanged& RegisterGenericGameplayEvent();

	/** Simple accessor to the explicit gameplay tag list */
	const FGameplayTagContainer& GetExplicitGameplayTags() const;

private:

	/** Map of tag to delegate that will be fired when the count for the key tag changes to or away from zero */
	TMap<FGameplayTag, FOnGameplayEffectTagCountChanged> GameplayTagEventMap;

	/** Map of tag to active count of that tag */
	TMap<FGameplayTag, int32> GameplayTagCountMap;

	/** Map of tag to explicit count of that tag. Cannot share with above map because it's not safe to merge explicit and generic counts */
	TMap<FGameplayTag, int32> ExplicitTagCountMap;

	/** Delegate fired whenever any tag's count changes to or away from zero */
	FOnGameplayEffectTagCountChanged OnAnyTagChangeDelegate;

	/** Container of tags that were explicitly added */
	FGameplayTagContainer ExplicitTags;

	/** Internal helper function to adjust the explicit tag list & corresponding maps/delegates/etc. as necessary */
	void UpdateTagMap_Internal(const FGameplayTag& Tag, int32 CountDelta);
};

// -----------------------------------------------------------

/** Encapsulate require and ignore tags */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayTagRequirements
{
	GENERATED_USTRUCT_BODY()

	/** All of these tags must be present */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayModifier)
	FGameplayTagContainer RequireTags;

	/** None of these tags may be present */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayModifier)
	FGameplayTagContainer IgnoreTags;

	bool	RequirementsMet(const FGameplayTagContainer& Container) const;
	bool	IsEmpty() const;

	static FGetGameplayTags	SnapshotTags(FGetGameplayTags TagDelegate);

	FString ToString() const;
};

USTRUCT()
struct GAMEPLAYABILITIES_API FTagContainerAggregator
{
	GENERATED_USTRUCT_BODY()

	FTagContainerAggregator() : CacheIsValid(false) {}

	FTagContainerAggregator(FTagContainerAggregator&& Other)
		: CapturedActorTags(MoveTemp(Other.CapturedActorTags))
		, CapturedSpecTags(MoveTemp(Other.CapturedSpecTags))
		, ScopedTags(MoveTemp(Other.ScopedTags))
		, CachedAggregator(MoveTemp(Other.CachedAggregator))
		, CacheIsValid(Other.CacheIsValid)
	{
	}

	FTagContainerAggregator(const FTagContainerAggregator& Other)
		: CapturedActorTags(Other.CapturedActorTags)
		, CapturedSpecTags(Other.CapturedSpecTags)
		, ScopedTags(Other.ScopedTags)
		, CachedAggregator(Other.CachedAggregator)
		, CacheIsValid(Other.CacheIsValid)
	{
	}

	FTagContainerAggregator& operator=(FTagContainerAggregator&& Other)
	{
		CapturedActorTags = MoveTemp(Other.CapturedActorTags);
		CapturedSpecTags = MoveTemp(Other.CapturedSpecTags);
		ScopedTags = MoveTemp(Other.ScopedTags);
		CachedAggregator = MoveTemp(Other.CachedAggregator);
		CacheIsValid = Other.CacheIsValid;
		return *this;
	}

	FTagContainerAggregator& operator=(const FTagContainerAggregator& Other)
	{
		CapturedActorTags = Other.CapturedActorTags;
		CapturedSpecTags = Other.CapturedSpecTags;
		ScopedTags = Other.ScopedTags;
		CachedAggregator = Other.CachedAggregator;
		CacheIsValid = Other.CacheIsValid;
		return *this;
	}

	FGameplayTagContainer& GetActorTags();
	const FGameplayTagContainer& GetActorTags() const;

	FGameplayTagContainer& GetSpecTags();
	const FGameplayTagContainer& GetSpecTags() const;

	const FGameplayTagContainer* GetAggregatedTags() const;

private:

	UPROPERTY()
	FGameplayTagContainer CapturedActorTags;

	UPROPERTY()
	FGameplayTagContainer CapturedSpecTags;

	UPROPERTY()
	FGameplayTagContainer ScopedTags;

	mutable FGameplayTagContainer CachedAggregator;
	mutable bool CacheIsValid;
};


/** Allows blueprints to generate a GameplayEffectSpec once and then reference it by handle, to apply it multiple times/multiple targets. */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayEffectSpecHandle
{
	GENERATED_USTRUCT_BODY()

	FGameplayEffectSpecHandle();
	FGameplayEffectSpecHandle(FGameplayEffectSpec* DataPtr);

	TSharedPtr<FGameplayEffectSpec>	Data;

	bool IsValidCache;

	void Clear()
	{
		Data.Reset();
	}

	bool IsValid() const
	{
		return Data.IsValid();
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		ABILITY_LOG(Fatal, TEXT("FGameplayEffectSpecHandle should not be NetSerialized"));
		return false;
	}

	/** Comparison operator */
	bool operator==(FGameplayEffectSpecHandle const& Other) const
	{
		// Both invalid structs or both valid and Pointer compare (???) // deep comparison equality
		bool bBothValid = IsValid() && Other.IsValid();
		bool bBothInvalid = !IsValid() && !Other.IsValid();
		return (bBothInvalid || (bBothValid && (Data.Get() == Other.Data.Get())));
	}

	/** Comparison operator */
	bool operator!=(FGameplayEffectSpecHandle const& Other) const
	{
		return !(FGameplayEffectSpecHandle::operator==(Other));
	}
};

template<>
struct TStructOpsTypeTraits<FGameplayEffectSpecHandle> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithCopy = true,		// Necessary so that TSharedPtr<FGameplayAbilityTargetData> Data is copied around
		WithNetSerializer = true,
		WithIdenticalViaEquality = true,
	};
};