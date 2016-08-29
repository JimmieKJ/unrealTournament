// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagsModule.h"
#include "GameplayCueInterface.h"
#include "GameplayCueManager.h"
#include "GameplayTagResponseTable.h"

UAbilitySystemGlobals::UAbilitySystemGlobals(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	AbilitySystemGlobalsClassName = FStringClassReference(TEXT("/Script/GameplayAbilities.AbilitySystemGlobals"));

	PredictTargetGameplayEffects = true;

	MinimalReplicationTagCountBits = 5;

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
	GetGameplayTagResponseTable();
	InitGlobalTags();

	// Register for PreloadMap so cleanup can occur on map transitions
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UAbilitySystemGlobals::HandlePreLoadMap);
}


UCurveTable * UAbilitySystemGlobals::GetGlobalCurveTable()
{
	if (!GlobalCurveTable && GlobalCurveTableName.IsValid())
	{
		GlobalCurveTable = Cast<UCurveTable>(GlobalCurveTableName.TryLoad());
	}
	return GlobalCurveTable;
}

UDataTable * UAbilitySystemGlobals::GetGlobalAttributeMetaDataTable()
{
	if (!GlobalAttributeMetaDataTable && GlobalAttributeMetaDataTableName.IsValid())
	{
		GlobalAttributeMetaDataTable = Cast<UDataTable>(GlobalAttributeMetaDataTableName.TryLoad());
	}
	return GlobalAttributeMetaDataTable;
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
	if (GIsEditor && !IsRunningCommandlet() && InObject)
	{
		UCurveTable* ReimportedCurveTable = Cast<UCurveTable>(InObject);
		if (ReimportedCurveTable && GlobalAttributeDefaultsTables.Contains(ReimportedCurveTable))
		{
			ReloadAttributeDefaults();
		}
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

void UAbilitySystemGlobals::InitGameplayCueParameters(FGameplayCueParameters& CueParameters, const FGameplayEffectSpecForRPC& Spec)
{
	CueParameters.AggregatedSourceTags = Spec.AggregatedSourceTags;
	CueParameters.AggregatedTargetTags = Spec.AggregatedTargetTags;
	CueParameters.GameplayEffectLevel = Spec.GetLevel();
	CueParameters.AbilityLevel = Spec.GetAbilityLevel();
	InitGameplayCueParameters(CueParameters, Spec.GetContext());
}

void UAbilitySystemGlobals::InitGameplayCueParameters_GESpec(FGameplayCueParameters& CueParameters, const FGameplayEffectSpec& Spec)
{
	CueParameters.AggregatedSourceTags = *Spec.CapturedSourceTags.GetAggregatedTags();
	CueParameters.AggregatedTargetTags = *Spec.CapturedTargetTags.GetAggregatedTags();

	// Look for a modified attribute magnitude to pass to the CueParameters
	for (const FGameplayEffectCue& CueDef : Spec.Def->GameplayCues)
	{	
		bool FoundMatch = false;
		if (CueDef.MagnitudeAttribute.IsValid())
		{
			for (const FGameplayEffectModifiedAttribute& ModifiedAttribute : Spec.ModifiedAttributes)
			{
				if (ModifiedAttribute.Attribute == CueDef.MagnitudeAttribute)
				{
					CueParameters.RawMagnitude = ModifiedAttribute.TotalMagnitude;
					FoundMatch = true;
					break;
				}
			}
			if (FoundMatch)
			{
				break;
			}
		}
	}

	CueParameters.GameplayEffectLevel = Spec.GetLevel();
	CueParameters.AbilityLevel = Spec.GetEffectContext().GetAbilityLevel();

	InitGameplayCueParameters(CueParameters, Spec.GetContext());
}

void UAbilitySystemGlobals::InitGameplayCueParameters(FGameplayCueParameters& CueParameters, const FGameplayEffectContextHandle& EffectContext)
{
	if (EffectContext.IsValid())
	{
		// Copy Context over wholesale. Projects may want to override this and not copy over all data
		CueParameters.EffectContext = EffectContext;
	}
}

// --------------------------------------------------------------------

void UAbilitySystemGlobals::StartAsyncLoadingObjectLibraries()
{
	if (GlobalGameplayCueManager != nullptr)
	{
		GlobalGameplayCueManager->LoadObjectLibraryFromPaths(GameplayCueNotifyPaths);
	}
}

// --------------------------------------------------------------------

/** Initialize FAttributeSetInitter. This is virtual so projects can override what class they use */
void UAbilitySystemGlobals::AllocAttributeSetInitter()
{
	GlobalAttributeSetInitter = TSharedPtr<FAttributeSetInitter>(new FAttributeSetInitterDiscreteLevels());
}

FAttributeSetInitter* UAbilitySystemGlobals::GetAttributeSetInitter() const
{
	check(GlobalAttributeSetInitter.IsValid());
	return GlobalAttributeSetInitter.Get();
}

void UAbilitySystemGlobals::InitAttributeDefaults()
{
 	bool bLoadedAnyDefaults = false;
 
	// Handle deprecated, single global table name
	if (GlobalAttributeSetDefaultsTableName.IsValid())
	{
		UCurveTable* AttribTable = Cast<UCurveTable>(GlobalAttributeSetDefaultsTableName.TryLoad());
		if (AttribTable)
		{
			GlobalAttributeDefaultsTables.Add(AttribTable);
			bLoadedAnyDefaults = true;
		}
	}

	// Handle array of global curve tables for attribute defaults
 	for (const FStringAssetReference& AttribDefaultTableName : GlobalAttributeSetDefaultsTableNames)
 	{
		if (AttribDefaultTableName.IsValid())
		{
			UCurveTable* AttribTable = Cast<UCurveTable>(AttribDefaultTableName.TryLoad());
			if (AttribTable)
			{
				GlobalAttributeDefaultsTables.Add(AttribTable);
				bLoadedAnyDefaults = true;
			}
		}
 	}
	
	if (bLoadedAnyDefaults)
	{
		// Subscribe for reimports if in the editor
#if WITH_EDITOR
		if (GIsEditor && !RegisteredReimportCallback)
		{
			GEditor->OnObjectReimported().AddUObject(this, &UAbilitySystemGlobals::OnTableReimported);
			RegisteredReimportCallback = true;
		}
#endif


		ReloadAttributeDefaults();
	}
}

void UAbilitySystemGlobals::ReloadAttributeDefaults()
{
	AllocAttributeSetInitter();
	GlobalAttributeSetInitter->PreloadAttributeSetData(GlobalAttributeDefaultsTables);
}

// --------------------------------------------------------------------

UGameplayCueManager* UAbilitySystemGlobals::GetGameplayCueManager()
{
	if (GlobalGameplayCueManager == nullptr)
	{
		// Load specific gameplaycue manager object if specified
		if (GlobalGameplayCueManagerName.IsValid())
		{
			GlobalGameplayCueManager = LoadObject<UGameplayCueManager>(nullptr, *GlobalGameplayCueManagerName.ToString(), nullptr, LOAD_None, nullptr);
			if (GlobalGameplayCueManager == nullptr)
			{
				ABILITY_LOG(Error, TEXT("Unable to Load GameplayCueManager %s"), *GlobalGameplayCueManagerName.ToString() );
			}
		}

		// Load specific gameplaycue manager class if specified
		if ( GlobalGameplayCueManager == nullptr && GlobalGameplayCueManagerClass.IsValid() )
		{
			UClass* GCMClass = LoadClass<UObject>(NULL, *GlobalGameplayCueManagerClass.ToString(), NULL, LOAD_None, NULL);
			if (GCMClass)
			{
				GlobalGameplayCueManager = NewObject<UGameplayCueManager>(this, GCMClass, NAME_None);
			}
		}

		if ( GlobalGameplayCueManager == nullptr)
		{
			// Fallback to CDO
			GlobalGameplayCueManager = UGameplayCueManager::StaticClass()->GetDefaultObject<UGameplayCueManager>();
		}

		GlobalGameplayCueManager->OnCreated();

		if (GameplayCueNotifyPaths.Num() == 0)
		{
			GameplayCueNotifyPaths.Add(TEXT("/Game"));
			ABILITY_LOG(Warning, TEXT("No GameplayCueNotifyPaths were specified in DefaultGame.ini under [/Script/GameplayAbilities.AbilitySystemGlobals]. Falling back to using all of /Game/. This may be slow on large projects. Consider specifying which paths are to be searched."));
		}
		
		if (GlobalGameplayCueManager->ShouldAsyncLoadObjectLibrariesAtStart())
		{
			StartAsyncLoadingObjectLibraries();
		}
	}

	check(GlobalGameplayCueManager);
	return GlobalGameplayCueManager;
}

UGameplayTagReponseTable* UAbilitySystemGlobals::GetGameplayTagResponseTable()
{
	if (GameplayTagResponseTable == nullptr && GameplayTagResponseTableName.IsValid())
	{
		GameplayTagResponseTable = LoadObject<UGameplayTagReponseTable>(nullptr, *GameplayTagResponseTableName.ToString(), nullptr, LOAD_None, nullptr);
	}

	return GameplayTagResponseTable;
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

void UAbilitySystemGlobals::HandlePreLoadMap(const FString& MapName)
{
	IGameplayCueInterface::ClearTagToFunctionMap();
	FActiveGameplayEffectHandle::ResetGlobalHandleMap();
}
