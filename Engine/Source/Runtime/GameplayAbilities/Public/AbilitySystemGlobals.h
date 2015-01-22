// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

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

/** Holds global data for the skill system. Can be configured per project via config file */
UCLASS(config=Game)
class GAMEPLAYABILITIES_API UAbilitySystemGlobals : public UObject
{
	GENERATED_UCLASS_BODY()

	virtual void InitGlobalData();

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

	/** Fully load all gameplay cues from the object library */
	UPROPERTY(config)
	bool	GameplayCueNotifyFullyLoad;

	/** The class to instantiate as the globals object. Defaults to this class but can be overridden */
	UPROPERTY(config)
	FStringClassReference AbilitySystemGlobalsClassName;

	UCurveTable* GetGlobalCurveTable();

	UDataTable* GetGlobalAttributeMetaDataTable();

	static void DeriveGameplayCueTagFromAssetName(FString AssetName, FGameplayTag& GameplayCueTag, FName& GameplayCueName);

	bool IsAbilitySystemGlobalsInitialized()
	{
		return GlobalAttributeSetInitter.IsValid();
	}

	void AutomationTestOnly_SetGlobalCurveTable(class UCurveTable *InTable)
	{
		GlobalCurveTable = InTable;
	}

	void AutomationTestOnly_SetGlobalAttributeDataTable(class UDataTable *InTable)
	{
		GlobalAttributeMetaDataTable = InTable;
	}

	static UAbilitySystemGlobals& Get();

	static UAbilitySystemComponent* GetAbilitySystemComponentFromActor(AActor* Actor, bool LookForComponent=false);

	/** Should allocate a project specific AbilityActorInfo struct. Caller is responsible for deallocation */
	virtual FGameplayAbilityActorInfo * AllocAbilityActorInfo() const;

	/** Should allocate a project specific GameplayEffectContext struct. Caller is responsible for deallocation */
	virtual FGameplayEffectContext* AllocGameplayEffectContext() const;

	UFunction* GetGameplayCueFunction(const FGameplayTag &Tag, UClass* Class, FName &MatchedTag);

	FAttributeSetInitter* GetAttributeSetInitter() const;

	virtual UGameplayCueManager* GetGameplayCueManager();

	virtual void GlobalPreGameplayEffectSpecApply(FGameplayEffectSpec& Spec, UAbilitySystemComponent* AbilitySystemComponent);

protected:

	virtual void InitAttributeDefaults();
	virtual void ReloadAttributeDefaults();
	virtual void AllocAttributeSetInitter();

private:

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
