// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagsModule.h"
#include "GameplayCueInterface.h"
#include "GameplayCueManager.h"


UAbilitySystemGlobals::UAbilitySystemGlobals(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	AbilitySystemGlobalsClassName = FStringClassReference(TEXT("/Script/GameplayAbilities.AbilitySystemGlobals"));
	GlobalGameplayCueManagerName = FStringClassReference(TEXT("/Script/GameplayAbilities.GameplayCueManager"));

	PredictTargetGameplayEffects = true;

#if WITH_EDITORONLY_DATA
	RegisteredReimportCallback = false;
#endif // #if WITH_EDITORONLY_DATA

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bIgnoreAbilitySystemCooldowns = false;
	bIgnoreAbilitySystemCosts = false;
#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UAbilitySystemGlobals::InitGlobalData()
{
	GetGlobalCurveTable();
	GetGlobalAttributeMetaDataTable();
	
	InitAttributeDefaults();

	GetGameplayCueManager();
	InitGlobalTags();
}

UCurveTable * UAbilitySystemGlobals::GetGlobalCurveTable()
{
	return InternalGetLoadTable<UCurveTable>(GlobalCurveTable, GlobalCurveTableName);
}

UDataTable * UAbilitySystemGlobals::GetGlobalAttributeMetaDataTable()
{
	return InternalGetLoadTable<UDataTable>(GlobalAttributeMetaDataTable, GlobalAttributeMetaDataTableName);
}

template <class T>
T* UAbilitySystemGlobals::InternalGetLoadTable(T*& Table, FString TableName)
{
	if (!Table && !TableName.IsEmpty())
	{
		Table = LoadObject<T>(NULL, *TableName, NULL, LOAD_None, NULL);
#if WITH_EDITOR
		// Hook into notifications for object re-imports so that the gameplay tag tree can be reconstructed if the table changes
		if (GIsEditor && Table && !RegisteredReimportCallback)
		{
			GEditor->OnObjectReimported().AddUObject(this, &UAbilitySystemGlobals::OnTableReimported);
			RegisteredReimportCallback = true;
		}
#endif
	}

	return Table;
}

void UAbilitySystemGlobals::DeriveGameplayCueTagFromAssetName(FString AssetName, FGameplayTag& GameplayCueTag, FName& GameplayCueName)
{
	// In the editor, attempt to infer GameplayCueTag from our asset name (if there is no valid GameplayCueTag already).
	#if WITH_EDITOR
	if (GIsEditor)
	{
		if (GameplayCueTag.IsValid() == false)
		{
			AssetName.RemoveFromStart(TEXT("Default__"));
			AssetName.RemoveFromStart(TEXT("GC_"));		// allow GC_ prefix in asset name
			AssetName.RemoveFromEnd(TEXT("_c"));

			AssetName.ReplaceInline(TEXT("_"), TEXT("."), ESearchCase::CaseSensitive);

			if (!AssetName.Contains(TEXT("GameplayCue")))
			{
				AssetName = FString(TEXT("GameplayCue.")) + AssetName;
			}

			IGameplayTagsModule& GameplayTagsModule = IGameplayTagsModule::Get();
			GameplayCueTag = GameplayTagsModule.GetGameplayTagsManager().RequestGameplayTag(FName(*AssetName), false);
		}
		GameplayCueName = GameplayCueTag.GetTagName();
	}
	#endif
}


#if WITH_EDITOR

void UAbilitySystemGlobals::OnTableReimported(UObject* InObject)
{
	if (GIsEditor && !IsRunningCommandlet() && InObject && InObject == GlobalAttributeDefaultsTable)
	{
		ReloadAttributeDefaults();
	}	
}

#endif

FGameplayAbilityActorInfo * UAbilitySystemGlobals::AllocAbilityActorInfo() const
{
	return new FGameplayAbilityActorInfo();
}

FGameplayEffectContext* UAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FGameplayEffectContext();
}

/** This is just some syntax sugar to avoid calling gode to have to IGameplayAbilitiesModule::Get() */
UAbilitySystemGlobals& UAbilitySystemGlobals::Get()
{
	static UAbilitySystemGlobals* GlobalPtr = IGameplayAbilitiesModule::Get().GetAbilitySystemGlobals();
	check(GlobalPtr == IGameplayAbilitiesModule::Get().GetAbilitySystemGlobals());
	check(GlobalPtr);

	return *GlobalPtr;
}

/** Helping function to avoid having to manually cast */
UAbilitySystemComponent* UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(const AActor* Actor, bool LookForComponent)
{
	if (Actor == nullptr)
	{
		return nullptr;
	}

	const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor);
	if (ASI)
	{
		return ASI->GetAbilitySystemComponent();
	}

	if (LookForComponent)
	{
		/** This is slow and not desirable */
		ABILITY_LOG(Warning, TEXT("GetAbilitySystemComponentFromActor called on %s that is not IAbilitySystemInterface. This slow!"), *Actor->GetName());

		return Actor->FindComponentByClass<UAbilitySystemComponent>();
	}

	return nullptr;
}

// --------------------------------------------------------------------

UFunction* UAbilitySystemGlobals::GetGameplayCueFunction(const FGameplayTag& ChildTag, UClass* Class, FName &MatchedTag)
{
	SCOPE_CYCLE_COUNTER(STAT_GetGameplayCueFunction);

	// A global cached map to lookup these functions might be a good idea. Keep in mind though that FindFunctionByName
	// is fast and already gives us a reliable map lookup.
	// 
	// We would get some speed by caching off the 'fully qualified name' to 'best match' lookup. E.g. we can directly map
	// 'GameplayCue.X.Y' to the function 'GameplayCue.X' without having to look for GameplayCue.X.Y first.
	// 
	// The native remapping (Gameplay.X.Y to Gameplay_X_Y) is also annoying and slow and could be fixed by this as well.
	// 
	// Keep in mind that any UFunction* cacheing is pretty unsafe. Classes can be loaded (and unloaded) during runtime
	// and will be regenerated all the time in the editor. Just doing a single pass at startup won't be enough,
	// we'll need a mechanism for registering classes when they are loaded on demand.
	
	IGameplayTagsModule& GameplayTagsModule = IGameplayTagsModule::Get();
	FGameplayTagContainer TagAndParentsContainer = GameplayTagsModule.GetGameplayTagsManager().RequestGameplayTagParents(ChildTag);

	for (auto InnerTagIt = TagAndParentsContainer.CreateConstIterator(); InnerTagIt; ++InnerTagIt)
	{
		FName CueName = InnerTagIt->GetTagName();
		if (UFunction* Func = Class->FindFunctionByName(CueName, EIncludeSuperFlag::IncludeSuper))
		{
			MatchedTag = CueName;
			return Func;
		}

		// Native functions cant be named with ".", so look for them with _. 
		FName NativeCueFuncName = *CueName.ToString().Replace(TEXT("."), TEXT("_"));
		if (UFunction* Func = Class->FindFunctionByName(NativeCueFuncName, EIncludeSuperFlag::IncludeSuper))
		{
			MatchedTag = CueName; // purposefully returning the . qualified name.
			return Func;
		}
	}

	return nullptr;
}

// --------------------------------------------------------------------

/** Initialize FAttributeSetInitter. This is virtual so projects can override what class they use */
void UAbilitySystemGlobals::AllocAttributeSetInitter()
{
	GlobalAttributeSetInitter = TSharedPtr<FAttributeSetInitter>(new FAttributeSetInitter());
}

FAttributeSetInitter* UAbilitySystemGlobals::GetAttributeSetInitter() const
{
	check(GlobalAttributeSetInitter.IsValid());
	return GlobalAttributeSetInitter.Get();
}

void UAbilitySystemGlobals::InitAttributeDefaults()
{
	if (InternalGetLoadTable<UCurveTable>(GlobalAttributeDefaultsTable, GlobalAttributeSetDefaultsTableName))
	{	
		ReloadAttributeDefaults();
	}
}

void UAbilitySystemGlobals::ReloadAttributeDefaults()
{
	AllocAttributeSetInitter();
	GlobalAttributeSetInitter->PreloadAttributeSetData(GlobalAttributeDefaultsTable);
}

// --------------------------------------------------------------------

UGameplayCueManager* UAbilitySystemGlobals::GetGameplayCueManager()
{
	if (GlobalGameplayCueManager == nullptr)
	{
		GlobalGameplayCueManager = LoadObject<UGameplayCueManager>(NULL, *GlobalGameplayCueManagerName.ToString(), NULL, LOAD_None, NULL);
		if (GameplayCueNotifyPaths.Num() > 0)
		{
			GlobalGameplayCueManager->LoadObjectLibraryFromPaths(GameplayCueNotifyPaths);
		}
	}

	return GlobalGameplayCueManager;
}

void UAbilitySystemGlobals::GlobalPreGameplayEffectSpecApply(FGameplayEffectSpec& Spec, UAbilitySystemComponent* AbilitySystemComponent)
{

}

void UAbilitySystemGlobals::ToggleIgnoreAbilitySystemCooldowns()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bIgnoreAbilitySystemCooldowns = !bIgnoreAbilitySystemCooldowns;
#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UAbilitySystemGlobals::ToggleIgnoreAbilitySystemCosts()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bIgnoreAbilitySystemCosts = !bIgnoreAbilitySystemCosts;
#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

bool UAbilitySystemGlobals::ShouldIgnoreCooldowns() const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	return bIgnoreAbilitySystemCooldowns;
#else
	return false;
#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

bool UAbilitySystemGlobals::ShouldIgnoreCosts() const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	return bIgnoreAbilitySystemCosts;
#else
	return false;
#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}
