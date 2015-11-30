// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerPluginPrivatePCH.h"
#include "Misc/CoreMisc.h"
#include "GameplayDebuggerReplicator.h"
#include "GameplayDebuggerModuleSettings.h"
#include "AssetRegistryModule.h"
#include "Debug/GameplayDebuggerBaseObject.h"
#include "GameplayDebuggerBlueprintBaseObject.h"
#if WITH_EDITOR
#include "SourceControlHelpers.h"
#include "Editor/EditorEngine.h"
#include "ISettingsModule.h"
#endif // WITH_EDITOR

#if ENABLED_GAMEPLAY_DEBUGGER
static TAutoConsoleVariable<int32> CVarHighlightSelectedActor(
	TEXT("ai.gd.HighlightSelectedActor"),
	1,
	TEXT("Enable or disable highlight around selected actor.\n")
	TEXT(" 0: Disable highlight\n")
	TEXT(" 1: Enable highlight (default)"),
	ECVF_Cheat);
#endif

UGameplayDebuggerModuleSettings::UGameplayDebuggerModuleSettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bUseAlternateKeys = false;
	bHighlightSelectedActor = true;
	bDoNotUseAlternateKeys = !bUseAlternateKeys;

	ActivationAction.ActionName = TEXT("ai.gd.activate");
	ActivationAction.Key = EKeys::Apostrophe;

	MenuStart = FVector2D(10.000000, 10.000000);
	DebugInfoStart = FVector2D(20.000000, 60.000000);

	{
		InputConfiguration.SelectPlayer = FInputConfig(EKeys::Enter, false, false, false, false);
		InputConfiguration.ZeroCategory = FInputConfig(EKeys::NumPadZero, false, false, false, false);
		InputConfiguration.OneCategory = FInputConfig(EKeys::NumPadOne, false, false, false, false);
		InputConfiguration.TwoCategory = FInputConfig(EKeys::NumPadTwo, false, false, false, false);
		InputConfiguration.ThreeCategory = FInputConfig(EKeys::NumPadThree, false, false, false, false);
		InputConfiguration.FourCategory = FInputConfig(EKeys::NumPadFour, false, false, false, false);
		InputConfiguration.FiveCategory = FInputConfig(EKeys::NumPadFive, false, false, false, false);
		InputConfiguration.SixCategory = FInputConfig(EKeys::NumPadSix, false, false, false, false);
		InputConfiguration.SevenCategory = FInputConfig(EKeys::NumPadSeven, false, false, false, false);
		InputConfiguration.EightCategory = FInputConfig(EKeys::NumPadEight, false, false, false, false);
		InputConfiguration.NineCategory = FInputConfig(EKeys::NumPadNine, false, false, false, false);

		InputConfiguration.DebugCamera = FInputConfig(EKeys::Tab, false, false, false, false);
		InputConfiguration.OnScreenDebugMessages = FInputConfig(EKeys::Tab, false, true, false, false);
		InputConfiguration.GameHUD = FInputConfig(EKeys::Tilde, false, true, false, false);

		InputConfiguration.NextMenuLine = FInputConfig(EKeys::Add, false, false, false, false);
		InputConfiguration.PreviousMenuLine = FInputConfig(EKeys::Subtract, false, false, false, false);
	}

	{
		AlternateInputConfiguration.SelectPlayer = FInputConfig(EKeys::Enter, false, true, false, false);
		AlternateInputConfiguration.ZeroCategory = FInputConfig(EKeys::Zero, false, false, true, false);
		AlternateInputConfiguration.OneCategory = FInputConfig(EKeys::One, false, false, true, false);
		AlternateInputConfiguration.TwoCategory = FInputConfig(EKeys::Two, false, false, true, false);
		AlternateInputConfiguration.ThreeCategory = FInputConfig(EKeys::Three, false, false, true, false);
		AlternateInputConfiguration.FourCategory = FInputConfig(EKeys::Four, false, false, true, false);
		AlternateInputConfiguration.FiveCategory = FInputConfig(EKeys::Five, false, false, true, false);
		AlternateInputConfiguration.SixCategory = FInputConfig(EKeys::Six, false, false, true, false);
		AlternateInputConfiguration.SevenCategory = FInputConfig(EKeys::Seven, false, false, true, false);
		AlternateInputConfiguration.EightCategory = FInputConfig(EKeys::Eight, false, false, true, false);
		AlternateInputConfiguration.NineCategory = FInputConfig(EKeys::Nine, false, false, true, false);

		AlternateInputConfiguration.DebugCamera = FInputConfig(EKeys::Tab, false, false, false, false);
		AlternateInputConfiguration.OnScreenDebugMessages = FInputConfig(EKeys::Tab, false, true, false, false);
		AlternateInputConfiguration.GameHUD = FInputConfig(EKeys::Tilde, false, true, false, false);

		AlternateInputConfiguration.NextMenuLine = FInputConfig(EKeys::Equals, false, false, true, false);
		AlternateInputConfiguration.PreviousMenuLine = FInputConfig(EKeys::Hyphen, false, false, true, false);
	}

#if ENABLED_GAMEPLAY_DEBUGGER
	// Import default settings from DaseEngine.ini
	GConfig->GetVector2D(TEXT("/Script/GameplayDebuggerModule.GameplayDebuggerModuleSettings"), TEXT("MenuStart"), MenuStart, GEngineIni);
	GConfig->GetVector2D(TEXT("/Script/GameplayDebuggerModule.GameplayDebuggerModuleSettings"), TEXT("DebugInfoStart"), DebugInfoStart, GEngineIni);
	{
		FString ActivationActionStr;
		GConfig->GetString(TEXT("/Script/GameplayDebuggerModule.GameplayDebuggerModuleSettings"), TEXT("ActivationAction"), ActivationActionStr, GEngineIni);
		UProperty* FoundProperty = FindField<UProperty>(GetClass(), TEXT("ActivationAction"));
		if (FoundProperty)
		{
			FoundProperty->ImportText(*ActivationActionStr, FoundProperty->ContainerPtrToValuePtr<uint8>(this), 0, this);
		}
	}
	GConfig->GetBool(TEXT("/Script/GameplayDebuggerModule.GameplayDebuggerModuleSettings"), TEXT("bUseAlternateKeys"), bUseAlternateKeys, GEngineIni);
	GConfig->GetBool(TEXT("/Script/GameplayDebuggerModule.GameplayDebuggerModuleSettings"), TEXT("bDoNotUseAlternateKeys"), bDoNotUseAlternateKeys, GEngineIni);
	{
		FString InputConfigurationStr;
		GConfig->GetString(TEXT("/Script/GameplayDebuggerModule.GameplayDebuggerModuleSettings"), TEXT("InputConfiguration"), InputConfigurationStr, GEngineIni);
		UProperty* FoundProperty = FindField<UProperty>(GetClass(), TEXT("InputConfiguration"));
		if (FoundProperty)
		{
			FoundProperty->ImportText(*InputConfigurationStr, FoundProperty->ContainerPtrToValuePtr<uint8>(this), 0, this);
		}
	}
	{
		FString AlternateInputConfigurationStr;
		GConfig->GetString(TEXT("/Script/GameplayDebuggerModule.GameplayDebuggerModuleSettings"), TEXT("AlternateInputConfiguration"), AlternateInputConfigurationStr, GEngineIni);
		UProperty* FoundProperty = FindField<UProperty>(GetClass(), TEXT("AlternateInputConfiguration"));
		if (FoundProperty)
		{
			FoundProperty->ImportText(*AlternateInputConfigurationStr, FoundProperty->ContainerPtrToValuePtr<uint8>(this), 0, this);
		}
	}

	{
		TArray<FString> ArrayOfCategories;
		GConfig->GetArray(TEXT("/Script/GameplayDebuggerModule.GameplayDebuggerModuleSettings"), TEXT("Categories"), ArrayOfCategories, GEngineIni);

		FString ArrayOfCategoriesAsString;
		ArrayOfCategoriesAsString = TEXT("(");
		for (const FString& CurrentCategory : ArrayOfCategories)
		{
			ArrayOfCategoriesAsString += CurrentCategory + TEXT(",");
		}
		ArrayOfCategoriesAsString += TEXT(")");

		UArrayProperty* ArrayProperty = FindField< UArrayProperty >(GetClass(), TEXT("Categories"));
		if (ArrayProperty)
		{
			ArrayProperty->ImportText(*ArrayOfCategoriesAsString, &Categories, 0, nullptr);
		}
	}

	UGameplayDebuggerBaseObject::GetOnClassRegisterEvent().AddUObject(this, &UGameplayDebuggerModuleSettings::RegisterClass);
	UGameplayDebuggerBaseObject::GetOnClassUnregisterEvent().AddUObject(this, &UGameplayDebuggerModuleSettings::UnregisterClass);
#endif
}

void UGameplayDebuggerModuleSettings::BeginDestroy()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	UGameplayDebuggerBaseObject::GetOnClassUnregisterEvent().RemoveAll(this);
	UGameplayDebuggerBaseObject::GetOnClassRegisterEvent().RemoveAll(this);
	Super::BeginDestroy();
#endif
}

void UGameplayDebuggerModuleSettings::UpdateConfigFileWithCheckout()
{
#if WITH_EDITOR && ENABLED_GAMEPLAY_DEBUGGER
	FString TargetFilePath = GetDefaultConfigFilename();
	FText ErrorMessage;
	SourceControlHelpers::CheckoutOrMarkForAdd(TargetFilePath, FText::FromString(TargetFilePath), NULL, ErrorMessage);

	UpdateDefaultConfigFile();
#endif
}

void UGameplayDebuggerModuleSettings::RegisterClass(class UGameplayDebuggerBaseObject* BaseObject)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	RegisterClass(BaseObject ? BaseObject->GetClass() : nullptr);
#endif
}

void UGameplayDebuggerModuleSettings::RegisterClass(UClass* InClass)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (!InClass || InClass->HasAnyClassFlags(CLASS_Abstract) || InClass->IsUnreachable())
	{
		return;
	}

	if (ManualClasses.Find(InClass) == INDEX_NONE && InClass->GetName().Find(TEXT("REINST_")) == INDEX_NONE && InClass->GetName().Find(TEXT("SKEL_")) == INDEX_NONE)
	{
		bool bUpdateConfigFile = false;
		ManualClasses.AddUnique(InClass);
		const UGameplayDebuggerBaseObject *DefaultObj = InClass->GetDefaultObject<UGameplayDebuggerBaseObject>();
		const FString CategoryName = DefaultObj->GetCategoryName();

		FGameplayDebuggerCategorySettings* CategorySettings = Categories.FindByPredicate([CategoryName](const FGameplayDebuggerCategorySettings& Other){ return Other.CategoryName == CategoryName; });
		if (!CategorySettings)
		{
			bUpdateConfigFile = true;
			CategorySettings = &Categories[Categories.Add(FGameplayDebuggerCategorySettings(CategoryName, true, true))];
		}

		const FString ClassPathName = InClass->GetPathName();
		const FGameplayDebuggerClassSettings *ClassSettings = CategorySettings->ArrayOfClasses.FindByPredicate([ClassPathName](const FGameplayDebuggerClassSettings& Other){ return Other.ClassPath == ClassPathName; });
		if (!ClassSettings)
		{
			bUpdateConfigFile = true;
			CategorySettings->ArrayOfClasses.Add(FGameplayDebuggerClassSettings(InClass->GetName(), InClass->GetPathName()));
		}
		if (bUpdateConfigFile)
		{
			UpdateConfigFileWithCheckout();
		}
	}
#endif
}

void UGameplayDebuggerModuleSettings::UnregisterClass(class UGameplayDebuggerBaseObject* BaseObject)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	UClass *InClass = BaseObject ? BaseObject->GetClass() : nullptr;
	const bool bIsValidClass = !(!InClass || InClass->HasAnyClassFlags(CLASS_Abstract) || InClass->IsUnreachable());

	if (bIsValidClass && ManualClasses.Find(InClass) != INDEX_NONE && InClass->GetName().Find(TEXT("REINST_")) == INDEX_NONE && InClass->GetName().Find(TEXT("SKEL_")) == INDEX_NONE)
	{
		ManualClasses.RemoveSwap(InClass);
		const UGameplayDebuggerBaseObject *DefaultObj = InClass->GetDefaultObject<UGameplayDebuggerBaseObject>();
		const FString CategoryName = DefaultObj->GetCategoryName();

		FGameplayDebuggerCategorySettings* CategorySettings = Categories.FindByPredicate([CategoryName](const FGameplayDebuggerCategorySettings& Other){ return Other.CategoryName == CategoryName; });
		if (!CategorySettings)
		{
			return;
		}

		const FString ClassPathName = InClass->GetPathName();
		CategorySettings->ArrayOfClasses.RemoveAll([ClassPathName](const FGameplayDebuggerClassSettings& Other){ return Other.ClassPath == ClassPathName; });
		if (CategorySettings->ArrayOfClasses.Num() == 0)
		{
			Categories.RemoveAll([CategoryName](const FGameplayDebuggerCategorySettings& Other){ return Other.CategoryName == CategoryName; });
		}
		UpdateCategories(true);
	}
#endif
}

void UGameplayDebuggerModuleSettings::LoadAnyMissingClasses()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	TArray<UClass *> Results = UGameplayDebuggerModuleSettings::GetAllGameplayDebuggerClasses();

	TArray<FGameplayDebuggerCategorySettings> CurrentCategories = Categories; // we need local copy as new classes are going to be added in the middle of our loop
	for (const FGameplayDebuggerCategorySettings& CurrentCategorySetting : CurrentCategories)
	{
		for (const FGameplayDebuggerClassSettings& ClassSettings : CurrentCategorySetting.ArrayOfClasses)
		{
			UClass** ClassPtr = Results.FindByPredicate([ClassSettings](const UClass* Other){ return Other && Other->GetName() == ClassSettings.ClassName && Other->GetPathName() == ClassSettings.ClassPath; });
			UClass* Class = ClassPtr ? *ClassPtr : nullptr;
			if (Class == nullptr)
			{
				Class = StaticLoadClass(UObject::StaticClass(), NULL, *ClassSettings.ClassPath, NULL, LOAD_None, NULL);
				if (Class)
				{
					Results.AddUnique(Class);
				}
			}
		}
	}
#endif
}

void UGameplayDebuggerModuleSettings::UpdateCategories(bool bUpdateConfig)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	TArray<UClass *> Results = UGameplayDebuggerModuleSettings::GetAllGameplayDebuggerClasses();
	bool bSaveConfig = false;

	/** remove any old classes from categories */
	for (int32 CategoryIndex = Categories.Num() - 1; CategoryIndex >= 0; --CategoryIndex)
	{
		FGameplayDebuggerCategorySettings& CurrentCategorySetting = Categories[CategoryIndex];
		for (int32 ClassIndex = CurrentCategorySetting.ArrayOfClasses.Num() - 1; ClassIndex >= 0; --ClassIndex)
		{
			const FGameplayDebuggerClassSettings& ClassSettings = CurrentCategorySetting.ArrayOfClasses[ClassIndex];
			UClass** ClassPtr = Results.FindByPredicate([ClassSettings](const UClass* Other){ return Other && Other->GetName() == ClassSettings.ClassName && Other->GetPathName() == ClassSettings.ClassPath; });
			UClass* Class = ClassPtr ? *ClassPtr : nullptr;
			const UGameplayDebuggerBaseObject *DefaultObj = Class ? Class->GetDefaultObject<UGameplayDebuggerBaseObject>() : nullptr;
			const FString CategoryName = DefaultObj ? DefaultObj->GetCategoryName() : FString();

			if (Class == nullptr || CategoryName != CurrentCategorySetting.CategoryName)
			{
				CurrentCategorySetting.ArrayOfClasses.RemoveAt(ClassIndex);
				bSaveConfig = true;
			}
		}

		if (CurrentCategorySetting.ArrayOfClasses.Num() == 0)
		{
			Categories.RemoveAt(CategoryIndex);
			bSaveConfig = true;
		}
	}

	/** add all new categories and classes */
	for (UClass* Class : Results)
	{
		if (Class == nullptr || Class->HasAnyClassFlags(CLASS_Abstract) || Class->IsUnreachable())
		{
			continue;
		}

		UGameplayDebuggerBaseObject const *DefaultObj = Class->GetDefaultObject<UGameplayDebuggerBaseObject>();
		const FString CategoryName = DefaultObj->GetCategoryName();

		FGameplayDebuggerCategorySettings* CategorySettings = Categories.FindByPredicate([CategoryName](const FGameplayDebuggerCategorySettings& Other){ return Other.CategoryName == CategoryName; });
		if (!CategorySettings)
		{
			bSaveConfig = true;
			CategorySettings = &Categories[Categories.Add(FGameplayDebuggerCategorySettings(CategoryName, true, true))];
		}

		const FString ClassPathName = Class->GetPathName();
		const FGameplayDebuggerClassSettings *ClassSettings = CategorySettings->ArrayOfClasses.FindByPredicate([ClassPathName](const FGameplayDebuggerClassSettings& Other){ return Other.ClassPath == ClassPathName; });
		if (!ClassSettings)
		{
			bSaveConfig = true;
			CategorySettings->ArrayOfClasses.Add(FGameplayDebuggerClassSettings(Class->GetName(), Class->GetPathName()));
		}
	}


	if (GIsEditor && bSaveConfig && bUpdateConfig)
	{
		UpdateConfigFileWithCheckout();
	}
#endif
}

const TArray<FGameplayDebuggerCategorySettings>& UGameplayDebuggerModuleSettings::GetCategories()
{
	return Categories;
}

const TArray<UClass*> UGameplayDebuggerModuleSettings::GetAllGameplayDebuggerClasses()
{
	TArray<UClass *> Results(ManualClasses);

#if ENABLED_GAMEPLAY_DEBUGGER
	for (int32 Index = Results.Num() - 1; Index >= 0; --Index)
	{
		UClass* Class = Results[Index];
		if (Class->HasAnyClassFlags(CLASS_Abstract) || Class->GetName().Contains(TEXT("SKEL_")) || Class->GetName().Contains(TEXT("REINST_")))
		{
			Results.RemoveAt(Index);
		}
	}

	const bool bIsGarbageCollectingOnGameThread = IsInGameThread() && IsGarbageCollecting();
	if (!bIsGarbageCollectingOnGameThread)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		static const FString PathName(FString::Printf(TEXT("Class'%s'"), *UGameplayDebuggerBlueprintBaseObject::StaticClass()->GetPathName()));
		static const FName NAME_Blueprint(TEXT("Blueprint"));
		TArray<FAssetData> AssetList;
		TMultiMap<FName, FString> TagsAndValues;
		TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(UBlueprint, ParentClass), PathName);
		AssetRegistry.GetAssetsByTagValues(TagsAndValues, AssetList);

		if (AssetList.Num())

		{
			for (FAssetData& CurrentAsset : AssetList)
			{
				if (CurrentAsset.AssetClass != NAME_Blueprint)
				{
					continue;
				}
				UObject* AssetOb = CurrentAsset.GetAsset();
				UBlueprint* AssetBP = Cast<UBlueprint>(CurrentAsset.GetAsset());
				UClass* AssetClass = AssetBP ? *AssetBP->GeneratedClass : AssetOb ? AssetOb->GetClass() : NULL;
				if (AssetClass->GetName().Contains(TEXT("REINST_")) == false && AssetClass->GetName().Contains(TEXT("SKEL_")) == false)
				{
					Results.AddUnique(AssetClass);
				}
			}
		}
	}
#endif	
	return Results;
}


#if WITH_EDITOR
void UGameplayDebuggerModuleSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static const FName NAME_UseAlternateKeys = GET_MEMBER_NAME_CHECKED(UGameplayDebuggerModuleSettings, bUseAlternateKeys);
	static const FName NAME_DoNotUseAlternateKeys = GET_MEMBER_NAME_CHECKED(UGameplayDebuggerModuleSettings, bDoNotUseAlternateKeys);
	static const FName NAME_InputConfiguration = GET_MEMBER_NAME_CHECKED(UGameplayDebuggerModuleSettings, InputConfiguration);
	static const FName NAME_AlternateInputConfiguration = GET_MEMBER_NAME_CHECKED(UGameplayDebuggerModuleSettings, AlternateInputConfiguration);
	static const FName NAME_RegenerateConfiguration = GET_MEMBER_NAME_CHECKED(UGameplayDebuggerModuleSettings, bRegenerateConfiguration);
	static const FName NAME_HighlightSelectedActor = GET_MEMBER_NAME_CHECKED(UGameplayDebuggerModuleSettings, bHighlightSelectedActor);

	const FName PropName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropName == NAME_UseAlternateKeys || PropName == NAME_AlternateInputConfiguration)
	{
		bDoNotUseAlternateKeys = !bUseAlternateKeys;
	}
	else if (PropName == NAME_DoNotUseAlternateKeys || PropName == NAME_InputConfiguration)
	{
		bUseAlternateKeys = !bDoNotUseAlternateKeys;
	}

	if (PropName == NAME_RegenerateConfiguration)
	{
		bRegenerateConfiguration = false;
	}

	if (PropName == NAME_HighlightSelectedActor)
	{
		IConsoleVariable* cvarHighlightSelectedActor = IConsoleManager::Get().FindConsoleVariable(TEXT("ai.gd.HighlightSelectedActor"));
		if (cvarHighlightSelectedActor)
		{
			cvarHighlightSelectedActor->Set(UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>()->bHighlightSelectedActor, ECVF_SetByConsole);
		}
	}

	UpdateCategories();
	UpdateConfigFileWithCheckout();

	SettingChangedEvent.Broadcast(PropName);
}
#endif
