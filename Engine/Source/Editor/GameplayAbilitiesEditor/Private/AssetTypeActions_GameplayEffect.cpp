// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemEditorPrivatePCH.h"
#include "AssetTypeActions_GameplayEffect.h"
#include "ObjectTools.h"
#include "GameplayAbility.h"
#include "GameplayEffect.h"
#include "AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

DEFINE_LOG_CATEGORY(LogAssetTypeActions_GameplayEffect);

void CopyInfoFromDataGE(const UGameplayEffect& InGE, UGameplayEffect& OutGE)
{
	OutGE.DurationPolicy = InGE.DurationPolicy;
	OutGE.DurationMagnitude = InGE.DurationMagnitude;
	OutGE.Period = InGE.Period;

	for (FGameplayModifierInfo InModifier : InGE.Modifiers)
	{
		FGameplayModifierInfo Modifier(InModifier);
		OutGE.Modifiers.Add(Modifier);
	}

	OutGE.ChanceToApplyToTarget = InGE.ChanceToApplyToTarget;

	// Don't copy over TargetEffects. These will need to get fixed up later
	// TArray<UDataGameplayEffect*> TargetEffects;

	OutGE.InheritableGameplayEffectTags.Added = InGE.InheritableGameplayEffectTags.Added; 
	OutGE.InheritableGameplayEffectTags.Added.AppendTags(InGE.GameplayEffectTags);
	OutGE.InheritableGameplayEffectTags.Removed = InGE.InheritableGameplayEffectTags.Removed;

	OutGE.InheritableOwnedTagsContainer.Added = InGE.InheritableOwnedTagsContainer.Added; 
	OutGE.InheritableOwnedTagsContainer.Added.AppendTags(InGE.OwnedTagsContainer);
	OutGE.InheritableOwnedTagsContainer.Removed = InGE.InheritableOwnedTagsContainer.Removed;

	OutGE.RemoveGameplayEffectsWithTags.Added = InGE.RemoveGameplayEffectsWithTags.Added;
	OutGE.RemoveGameplayEffectsWithTags.Added.AppendTags(InGE.ClearTagsContainer);
	OutGE.RemoveGameplayEffectsWithTags.Removed = InGE.RemoveGameplayEffectsWithTags.Removed;

	for (FGameplayEffectCue InCue : InGE.GameplayCues)
	{
		OutGE.GameplayCues.Add(InCue);
	}

	OutGE.UpdateInheritedTagProperties();
}

/////////////////////////////////////////////////////
// FAssetTypeActions_GameplayEffect

FText FAssetTypeActions_GameplayEffect::GetName() const
{
	return LOCTEXT("AssetTypeActions_GameplayEffect", "Gameplay Effect");
}

FColor FAssetTypeActions_GameplayEffect::GetTypeColor() const
{
	return FColor(255, 220, 0);
}

UClass* FAssetTypeActions_GameplayEffect::GetSupportedClass() const
{
	return UGameplayEffect::StaticClass();
}

bool FAssetTypeActions_GameplayEffect::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}

void FAssetTypeActions_GameplayEffect::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto GEs = GetTypedWeakObjectPtrs<UGameplayEffect>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("GameplayEffect_ConvertToBlueprint", "Convert to Blueprint"),
		LOCTEXT("GameplayEffect_EditTooltip", "Converts this gameplay effect to a gameplay effect blueprint."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_GameplayEffect::ExecuteConvertToBlueprint, GEs ),
			FCanExecuteAction()
			)
		);
}

uint32 FAssetTypeActions_GameplayEffect::GetCategories()
{
	return EAssetTypeCategories::Gameplay;
}

void FAssetTypeActions_GameplayEffect::ExecuteConvertToBlueprint(FWeakGameplayEffectPointerArray Objects)
{
	UBlueprintFactory* BlueprintFactory = NewObject<UBlueprintFactory>();
	BlueprintFactory->ParentClass = UGameplayEffect::StaticClass();
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	TArray<UObject*> ObjectsToAttemptToDelete;

	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UGameplayEffect* OldGE = (*ObjIt).Get())
		{
			UPackage* Package = OldGE->GetOutermost();

			FString NameStr = OldGE->GetName();

			OldGE->Rename(*NameStr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);

			UBlueprint* NewAsset = Cast<UBlueprint>(BlueprintFactory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*NameStr), RF_Public | RF_Standalone, NULL, GWarn));

			if (NewAsset == NULL || NewAsset->GeneratedClass == NULL)
			{
				UE_LOG(LogAssetTypeActions_GameplayEffect, Warning, TEXT("Failed to create new blueprint or new blueprint did not compile for %s"), *NameStr);
				continue;
			}

			UGameplayEffect* GE = Cast<UGameplayEffect>(NewAsset->GeneratedClass->GetDefaultObject());
			CopyInfoFromDataGE(*OldGE, *GE);

			OldGE->Template = nullptr;

			FixReferencers(Package, OldGE, GE);

			ObjectsToAttemptToDelete.Add(OldGE);
		}
	}

	for ( UObject* ObjectToDelete : ObjectsToAttemptToDelete )
	{
		if (!ObjectTools::DeleteSingleObject(ObjectToDelete, true))
		{
			UE_LOG(LogAssetTypeActions_GameplayEffect, Warning, TEXT("Failed to delete object: %s"), *ObjectToDelete->GetPathName());
		}
	}
}

void FAssetTypeActions_GameplayEffect::FixReferencers(UPackage* Package, UGameplayEffect* OldGE, UGameplayEffect* GE)
{
	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FName> ReferencerNames;
	AssetRegistryModule.Get().GetReferencers(Package->GetFName(), ReferencerNames);

	TArray<UPackage*> RedirectedPackages;
	for (auto Name : ReferencerNames)
	{
		UPackage* ReferencerPackage = LoadPackage(NULL, *Name.ToString(), LOAD_None);

		TArray<UObject*> ObjectsInPackage;
		GetObjectsWithOuter(ReferencerPackage, ObjectsInPackage, false);
		bool bFound = false;
		for (UObject* Obj : ObjectsInPackage)
		{
			TSubclassOf<UGameplayAbility> AbilityClass = Cast<UClass>(Obj);
			if (AbilityClass)
			{
				TArray<UObject*> ObjectsOfAbilityClass;
				GetObjectsOfClass(AbilityClass, ObjectsOfAbilityClass, true, RF_NoFlags);
				for ( UObject* ObjectOfClass : ObjectsOfAbilityClass )
				{
					UGameplayAbility* Ability = Cast<UGameplayAbility>(ObjectOfClass);
					if ( ensure(Ability) )
					{
						Ability->ConvertDeprecatedGameplayEffectReferencesToBlueprintReferences(OldGE, GE->GetClass());
					}
				}
				bFound = true;
			}
			UObjectRedirector* Redirector = Cast<UObjectRedirector>(Obj);

			if (Redirector && Redirector->DestinationObject == OldGE)
			{
				RedirectedPackages.Add(ReferencerPackage);
				bFound = true;
			}

		}
		if (bFound == false)
		{
			UE_LOG(LogAssetTypeActions_GameplayEffect, Warning, TEXT("Couldn't cast package contents %s to known dependency type"), *ReferencerPackage->GetName());
		}
		for (UPackage* RedirectedPackage : RedirectedPackages)
		{
			FixReferencers(RedirectedPackage, OldGE, GE);
		}
	}
}
#undef LOCTEXT_NAMESPACE
