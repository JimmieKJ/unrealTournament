// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetDataFilter.h"
#include "GameplayCueInterface.h"
#include "AbilitySystemBlueprintLibrary.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitMovementModeChange;
class UAbilityTask_WaitOverlap;
class UAbilityTask_WaitConfirmCancel;

// meta =(RestrictedToClasses="GameplayAbility")
UCLASS()
class GAMEPLAYABILITIES_API UAbilitySystemBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintPure, Category = Ability)
	static UAbilitySystemComponent* GetAbilitySystemComponent(AActor *Actor);

	// NOTE: The Actor passed in must implement IAbilitySystemInterface! or else this function will silently fail to
	// send the event.  The actor needs the interface to find the UAbilitySystemComponent, and if the component isn't
	// found, the event will not be sent.
	UFUNCTION(BlueprintCallable, Category = Ability, Meta = (Tooltip = "This function can be used to trigger an ability on the actor in question with useful payload data."))
	static void SendGameplayEventToActor(AActor* Actor, FGameplayTag EventTag, FGameplayEventData Payload);
	
	// -------------------------------------------------------------------------------
	//		Attribute
	// -------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Ability|Attribute")
	static float GetFloatAttribute(const class AActor* Actor, FGameplayAttribute Attribute, bool& bSuccessfullyFoundAttribute);

	// -------------------------------------------------------------------------------
	//		TargetData
	// -------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle AppendTargetDataHandle(FGameplayAbilityTargetDataHandle TargetHandle, FGameplayAbilityTargetDataHandle HandleToAdd);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle	AbilityTargetDataFromLocations(const FGameplayAbilityTargetingLocationInfo& SourceLocation, const FGameplayAbilityTargetingLocationInfo& TargetLocation);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle	AbilityTargetDataFromHitResult(FHitResult HitResult);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static int32 GetDataCountFromTargetData(FGameplayAbilityTargetDataHandle TargetData);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle	AbilityTargetDataFromActor(AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle	AbilityTargetDataFromActorArray(TArray<TWeakObjectPtr<AActor>> ActorArray, bool OneTargetPerHandle);

	/** Create a new target data handle with filtration performed on the data */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FGameplayAbilityTargetDataHandle	FilterTargetData(FGameplayAbilityTargetDataHandle TargetDataHandle, FGameplayTargetDataFilterHandle ActorFilterClass);

	/** Create a handle for filtering target data, filling out all fields */
	UFUNCTION(BlueprintPure, Category = "Filter")
	static FGameplayTargetDataFilterHandle MakeFilterHandle(FGameplayTargetDataFilter Filter, AActor* FilterActor);

	/** Create a spec handle, filling out all fields */
	UFUNCTION(BlueprintPure, Category = "Spec")
	static FGameplayEffectSpecHandle MakeSpecHandle(UGameplayEffect* InGameplayEffect, AActor* InInstigator, AActor* InEffectCauser, float InLevel = 1.0f);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static TArray<AActor*> GetActorsFromTargetData(FGameplayAbilityTargetDataHandle TargetData, int32 Index);

	/** Returns true if the given TargetData has the actor passed in targeted */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool DoesTargetDataContainActor(FGameplayAbilityTargetDataHandle TargetData, int32 Index, AActor* Actor);

	/** Returns true if the given TargetData has at least 1 actor targeted */
	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool TargetDataHasActor(FGameplayAbilityTargetDataHandle TargetData, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool TargetDataHasHitResult(FGameplayAbilityTargetDataHandle HitResult, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FHitResult GetHitResultFromTargetData(FGameplayAbilityTargetDataHandle HitResult, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool TargetDataHasOrigin(FGameplayAbilityTargetDataHandle TargetData, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FTransform GetTargetDataOrigin(FGameplayAbilityTargetDataHandle TargetData, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static bool TargetDataHasEndPoint(FGameplayAbilityTargetDataHandle TargetData, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FVector GetTargetDataEndPoint(FGameplayAbilityTargetDataHandle TargetData, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|TargetData")
	static FTransform GetTargetDataEndPointTransform(FGameplayAbilityTargetDataHandle TargetData, int32 Index);

	// -------------------------------------------------------------------------------
	//		GameplayEffectContext
	// -------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "IsInstigatorLocallyControlled"))
	static bool EffectContextIsInstigatorLocallyControlled(FGameplayEffectContextHandle EffectContext);

	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetHitResult"))
	static FHitResult EffectContextGetHitResult(FGameplayEffectContextHandle EffectContext);

	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "HasHitResult"))
	static bool EffectContextHasHitResult(FGameplayEffectContextHandle EffectContext);

	/** Gets the location the effect originated from */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetOrigin"))
	static FVector EffectContextGetOrigin(FGameplayEffectContextHandle EffectContext);

	/** Gets the instigating actor (that holds the ability system component) of the EffectContext */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetInstigatorActor"))
	static AActor* EffectContextGetInstigatorActor(FGameplayEffectContextHandle EffectContext);

	/** Gets the original instigator actor that started the chain of events to cause this effect */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetOriginalInstigatorActor"))
	static AActor* EffectContextGetOriginalInstigatorActor(FGameplayEffectContextHandle EffectContext);

	/** Gets the physical actor that caused the effect, possibly a projectile or weapon */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetEffectCauser"))
	static AActor* EffectContextGetEffectCauser(FGameplayEffectContextHandle EffectContext);

	/** Gets the source object of the effect. */
	UFUNCTION(BlueprintPure, Category = "Ability|EffectContext", Meta = (DisplayName = "GetSourceObject"))
	static UObject* EffectContextGetSourceObject(FGameplayEffectContextHandle EffectContext);

	// -------------------------------------------------------------------------------
	//		GameplayCue
	// -------------------------------------------------------------------------------
	
	UFUNCTION(BlueprintPure, Category="Ability|GameplayCue")
	static bool IsInstigatorLocallyControlled(FGameplayCueParameters Parameters);

	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static int32 GetActorCount(FGameplayCueParameters Parameters);

	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static AActor* GetActorByIndex(FGameplayCueParameters Parameters, int32 Index);

	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static FHitResult GetHitResult(FGameplayCueParameters Parameters);

	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static bool HasHitResult(FGameplayCueParameters Parameters);

	/** Forwards the gameplay cue to another gameplay cue interface object */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayCue")
	static void ForwardGameplayCueToTarget(TScriptInterface<IGameplayCueInterface> TargetCueInterface, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters);

	/** Gets the instigating actor (that holds the ability system component) of the GameplayCue */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static AActor* GetInstigatorActor(FGameplayCueParameters Parameters);

	/** Gets instigating world location */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static FTransform GetInstigatorTransform(FGameplayCueParameters Parameters);

	/** Gets instigating world location */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static FVector GetOrigin(FGameplayCueParameters Parameters);

	/** Gets the best end location and normal for this gameplay cue. If there is hit result data, it will return this. Otherwise it will return the target actor's location/rotation. If none of this is available, it will return false. */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static bool GetGameplayCueEndLocationAndNormal(AActor* TargetActor, FGameplayCueParameters Parameters, FVector& Location, FVector& Normal);

	/** Gets the best normalized effect direction for this gameplay cue. This is useful for effects that require the direction of an enemy attack. Returns true if a valid direction could be calculated. */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static bool GetGameplayCueDirection(AActor* TargetActor, FGameplayCueParameters Parameters, FVector& Direction);

	/** Returns true if the aggregated source and target tags from the effect spec meets the tag requirements */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayCue")
	static bool DoesGameplayCueMeetTagRequirements(FGameplayCueParameters Parameters, UPARAM(ref) FGameplayTagRequirements& SourceTagReqs, UPARAM(ref) FGameplayTagRequirements& TargetTagReqs);

	// -------------------------------------------------------------------------------
	//		GameplayEffectSpec
	// -------------------------------------------------------------------------------
	
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle AssignSetByCallerMagnitude(FGameplayEffectSpecHandle SpecHandle, FName DataName, float Magnitude);

	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle SetDuration(FGameplayEffectSpecHandle SpecHandle, float Duration);

	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle AddGrantedTag(FGameplayEffectSpecHandle SpecHandle, FGameplayTag NewGameplayTag);

	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle AddGrantedTags(FGameplayEffectSpecHandle SpecHandle, FGameplayTagContainer NewGameplayTags);

	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle AddLinkedGameplayEffectSpec(FGameplayEffectSpecHandle SpecHandle, FGameplayEffectSpecHandle LinkedGameplayEffectSpec);

	/** Sets the GameplayEffectSpec's StackCount to the specified amount (prior to applying) */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle SetStackCount(FGameplayEffectSpecHandle SpecHandle, int32 StackCount);

	/** Sets the GameplayEffectSpec's StackCount to the max stack count defined in the GameplayEffect definition */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static FGameplayEffectSpecHandle SetStackCountToMax(FGameplayEffectSpecHandle SpecHandle);

	// -------------------------------------------------------------------------------
	//		FActiveGameplayEffectHandle
	// -------------------------------------------------------------------------------
	
	/** Returns current stack count of an active Gameplay Effect. Will return 0 if the GameplayEffect is no longer valid. */
	UFUNCTION(BlueprintCallable, Category = "Ability|GameplayEffect")
	static int32 GetActiveGameplayEffectStackCount(FActiveGameplayEffectHandle ActiveHandle);
};
