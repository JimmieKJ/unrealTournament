// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagsModule.h"
#include "AbilitySystemGlobals.generated.h"

class AActor;
class UAbilitySystemComponent;
class UCurveTable;
class UDataTable;
class UGameplayCueManager;

struct FGameplayAbilityActorInfo;
struct FGameplayEffectContext;
struct FGameplayTag;
struct FAttributeSetInitter;
struct FGameplayEffectSpec;

/** Holds global data for the ability system. Can be configured per project via config file */
UCLASS(config=Game)
class GAMEPLAYABILITIES_API UAbilitySystemGlobals : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Gets the single instance of the globals object, will create it as necessary */
	static UAbilitySystemGlobals& Get();

	/** Should be called once as part of project setup to load global data tables and tags */
	virtual void InitGlobalData();

	/** Returns true if InitGlobalData has been called */
	bool IsAbilitySystemGlobalsInitialized()
	{
		return GlobalAttributeSetInitter.IsValid();
	}

	/** Returns the curvetable used as the default for scalable floats that don't specify a curve table */
	UCurveTable* GetGlobalCurveTable();

	/** Returns the data table defining attribute metadata (NOTE: Currently not in use) */
	UDataTable* GetGlobalAttributeMetaDataTable();

	/** Returns data used to initialize attributes to their default values */
	FAttributeSetInitter* GetAttributeSetInitter() const;

	/** Searches the passed in actor for an ability system component, will use the AbilitySystemInterface */
	static UAbilitySystemComponent* GetAbilitySystemComponentFromActor(const AActor* Actor, bool LookForComponent=false);

	/** Should allocate a project specific AbilityActorInfo struct. Caller is responsible for deallocation */
	virtual FGameplayAbilityActorInfo * AllocAbilityActorInfo() const;

	/** Should allocate a project specific GameplayEffectContext struct. Caller is responsible for deallocation */
	virtual FGameplayEffectContext* AllocGameplayEffectContext() const;

	/** Global callback that can handle game-specific code that needs to run before applying a gameplay effect spec */
	virtual void GlobalPreGameplayEffectSpecApply(FGameplayEffectSpec& Spec, UAbilitySystemComponent* AbilitySystemComponent);

	/** Returns true if the ability system should try to predict gameplay effects applied to non local targets */
	bool ShouldPredictTargetGameplayEffects() const
	{
		return PredictTargetGameplayEffects;
	}

	/** Searches the passed in class to look for a UFunction implementing the gameplay cue tag, sets MatchedTag to the exact tag found */
	UFunction* GetGameplayCueFunction(const FGameplayTag &Tag, UClass* Class, FName &MatchedTag);

	/** Returns the gameplay cue manager singleton object, creating if necessary */
	virtual UGameplayCueManager* GetGameplayCueManager();

	/** Sets a default gameplay cue tag using the asset's name */
	static void DeriveGameplayCueTagFromAssetName(FString AssetName, FGameplayTag& GameplayCueTag, FName& GameplayCueName);

	/** The class to instantiate as the globals object. Defaults to this class but can be overridden */
	UPROPERTY(config)
	FStringClassReference AbilitySystemGlobalsClassName;

	void AutomationTestOnly_SetGlobalCurveTable(class UCurveTable *InTable)
	{
		GlobalCurveTable = InTable;
	}

	void AutomationTestOnly_SetGlobalAttributeDataTable(class UDataTable *InTable)
	{
		GlobalAttributeMetaDataTable = InTable;
	}

	// Cheat functions

	/** Toggles whether we should ignore ability cooldowns. Does nothing in shipping builds */
	UFUNCTION(exec)
	virtual void ToggleIgnoreAbilitySystemCooldowns();

	/** Toggles whether we should ignore ability costs. Does nothing in shipping builds */
	UFUNCTION(exec)
	virtual void ToggleIgnoreAbilitySystemCosts();

	/** Returns true if ability cooldowns are ignored, returns false otherwise. Always returns false in shipping builds. */
	bool ShouldIgnoreCooldowns() const;

	/** Returns true if ability costs are ignored, returns false otherwise. Always returns false in shipping builds. */
	bool ShouldIgnoreCosts() const;

	// Global Tags

	UPROPERTY()
	FGameplayTag ActivateFailCooldownTag; // TryActivate failed due to being on cooldown
	UPROPERTY(config)
	FName ActivateFailCooldownName;

	UPROPERTY()
	FGameplayTag ActivateFailCostTag; // TryActivate failed due to not being able to spend costs
	UPROPERTY(config)
	FName ActivateFailCostName;

	UPROPERTY()
	FGameplayTag ActivateFailTagsBlockedTag; // TryActivate failed due to being blocked by other abilities
	UPROPERTY(config)
	FName ActivateFailTagsBlockedName;

	UPROPERTY()
	FGameplayTag ActivateFailTagsMissingTag; // TryActivate failed due to missing required tags
	UPROPERTY(config)
	FName ActivateFailTagsMissingName;

	UPROPERTY()
	FGameplayTag ActivateFailNetworkingTag; // Failed to activate due to invalid networking settings, this is designer error
	UPROPERTY(config)
	FName ActivateFailNetworkingName;

	virtual void InitGlobalTags()
	{
		if (ActivateFailCooldownName != NAME_None)
		{
			ActivateFailCooldownTag = IGameplayTagsModule::RequestGameplayTag(ActivateFailCooldownName);
		}

		if (ActivateFailCostName != NAME_None)
		{
			ActivateFailCostTag = IGameplayTagsModule::RequestGameplayTag(ActivateFailCostName);
		}

		if (ActivateFailTagsBlockedName != NAME_None)
		{
			ActivateFailTagsBlockedTag = IGameplayTagsModule::RequestGameplayTag(ActivateFailTagsBlockedName);
		}

		if (ActivateFailTagsMissingName != NAME_None)
		{
			ActivateFailTagsMissingTag = IGameplayTagsModule::RequestGameplayTag(ActivateFailTagsMissingName);
		}

		if (ActivateFailNetworkingName != NAME_None)
		{
			ActivateFailNetworkingTag = IGameplayTagsModule::RequestGameplayTag(ActivateFailNetworkingName);
		}
	}

protected:

	virtual void InitAttributeDefaults();
	virtual void ReloadAttributeDefaults();
	virtual void AllocAttributeSetInitter();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// data used for ability system cheat commands

	/** If we should ignore the cooldowns when activating abilities in the ability system. Set with ToggleIgnoreAbilitySystemCooldowns() */
	bool bIgnoreAbilitySystemCooldowns;

	/** If we should ignore the costs when activating abilities in the ability system. Set with ToggleIgnoreAbilitySystemCosts() */
	bool bIgnoreAbilitySystemCosts;
#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

private:

	/** Holds all of the valid gameplay-related tags that can be applied to assets */
	UPROPERTY(config)
	FString GlobalCurveTableName;

	/** Holds information about the valid attributes' min and max values and stacking rules */
	UPROPERTY(config)
	FString GlobalAttributeMetaDataTableName;

	/** Holds default values for attribute sets, keyed off of Name/Levels. */
	UPROPERTY(config)
	FString GlobalAttributeSetDefaultsTableName;

	/** The class to instantiate as the global GameplayCue manager. This class is responsible for directing GameplayCue events, loading and cooking assets related to GameplayCues. */
	UPROPERTY(config)
	FStringAssetReference GlobalGameplayCueManagerName;

	/** Look in these paths for GameplayCueNotifies */
	UPROPERTY(config)
	TArray<FString>	GameplayCueNotifyPaths;

	/** Set to true if you want clients to try to predict gameplay effects done to targets. If false it will only predict self effects */
	UPROPERTY(config)
	bool	PredictTargetGameplayEffects;

	UPROPERTY()
	UCurveTable* GlobalCurveTable;

	UPROPERTY()
	UCurveTable* GlobalAttributeDefaultsTable;

	UPROPERTY()
	UDataTable* GlobalAttributeMetaDataTable;

	UPROPERTY()
	UGameplayCueManager* GlobalGameplayCueManager;

	TSharedPtr<FAttributeSetInitter> GlobalAttributeSetInitter;

	template <class T>
	T* InternalGetLoadTable(T*& Table, FString TableName);

#if WITH_EDITOR
	void OnTableReimported(UObject* InObject);
#endif

#if WITH_EDITORONLY_DATA
	bool RegisteredReimportCallback;
#endif

};
