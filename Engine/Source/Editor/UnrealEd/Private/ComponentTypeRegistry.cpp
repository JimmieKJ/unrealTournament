// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"

#include "AssetRegistryModule.h"
#include "ComponentTypeRegistry.h"
#include "EdGraphSchema_K2.h"
#include "KismetEditorUtilities.h"

#define LOCTEXT_NAMESPACE "ComponentTypeRegistry"

struct FComponentTypeRegistryData
	: public FTickableEditorObject
{
	FComponentTypeRegistryData();
	void RefreshComponentList();

	/** Implementation of FTickableEditorObject */
	virtual void Tick(float) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FTypeDatabaseUpdater, STATGROUP_Tickables); }
	/** End implementation of FTickableEditorObject */

	TArray<FComponentClassComboEntryPtr> ComponentClassList;
	TArray<FComponentTypeEntry> ComponentTypeList;
	TArray<FAssetData> PendingAssetData;
	FOnComponentTypeListChanged ComponentListChanged;
};

static const FString CommonClassGroup(TEXT("Common"));
// This has to stay in sync with logic in FKismetCompilerContext::FinishCompilingClass
static const FString BlueprintComponents(TEXT("Custom"));

static void AddBasicShapeComponents(TArray<FComponentClassComboEntryPtr>& SortedClassList)
{
	FString BasicShapesHeading = LOCTEXT("BasicShapesHeading", "Basic Shapes").ToString();

	const auto OnBasicShapeCreated = [](UActorComponent* Component)
	{
		UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Component);
		if (SMC)
		{
			SMC->SetMaterial(0, LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
		}
	};

	{
		FComponentEntryCustomizationArgs Args;
		Args.AssetOverride = LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicCube.ToString());
		Args.OnComponentCreated = FOnComponentCreated::CreateStatic(OnBasicShapeCreated);
		Args.ComponentNameOverride = LOCTEXT("BasicCubeShapeDisplayName", "Cube").ToString();
		Args.IconOverrideBrushName = FName("ClassIcon.Cube");

		FComponentClassComboEntryPtr NewShape = MakeShareable(new FComponentClassComboEntry(BasicShapesHeading, UStaticMeshComponent::StaticClass(), true, EComponentCreateAction::SpawnExistingClass, Args));
		SortedClassList.Add(NewShape);
	}

	{
		FComponentEntryCustomizationArgs Args;
		Args.AssetOverride = LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicSphere.ToString());
		Args.OnComponentCreated = FOnComponentCreated::CreateStatic(OnBasicShapeCreated);
		Args.ComponentNameOverride = LOCTEXT("BasicSphereShapeDisplayName", "Sphere").ToString();
		Args.IconOverrideBrushName = FName("ClassIcon.Sphere");

		FComponentClassComboEntryPtr NewShape = MakeShareable(new FComponentClassComboEntry(BasicShapesHeading, UStaticMeshComponent::StaticClass(), true, EComponentCreateAction::SpawnExistingClass, Args));
		SortedClassList.Add(NewShape);
	}

	{
		FComponentEntryCustomizationArgs Args;
		Args.AssetOverride = LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicCylinder.ToString());
		Args.OnComponentCreated = FOnComponentCreated::CreateStatic(OnBasicShapeCreated);
		Args.ComponentNameOverride = LOCTEXT("BasicCylinderShapeDisplayName", "Cylinder").ToString();
		Args.IconOverrideBrushName = FName("ClassIcon.Cylinder");

		FComponentClassComboEntryPtr NewShape = MakeShareable(new FComponentClassComboEntry(BasicShapesHeading, UStaticMeshComponent::StaticClass(), true, EComponentCreateAction::SpawnExistingClass, Args));
		SortedClassList.Add(NewShape);
	}

	{
		FComponentEntryCustomizationArgs Args;
		Args.AssetOverride = LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicCone.ToString());
		Args.OnComponentCreated = FOnComponentCreated::CreateStatic(OnBasicShapeCreated);
		Args.ComponentNameOverride = LOCTEXT("BasicConeShapeDisplayName", "Cone").ToString();
		Args.IconOverrideBrushName = FName("ClassIcon.Cone");

		FComponentClassComboEntryPtr NewShape = MakeShareable(new FComponentClassComboEntry(BasicShapesHeading, UStaticMeshComponent::StaticClass(), true, EComponentCreateAction::SpawnExistingClass, Args));
		SortedClassList.Add(NewShape);
	}
}

FComponentTypeRegistryData::FComponentTypeRegistryData()
{
	const auto HandleAdded = [](const FAssetData& Data, FComponentTypeRegistryData* Parent)
	{
		Parent->PendingAssetData.Push(Data);
	};

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnAssetAdded().AddStatic(HandleAdded, this);
	AssetRegistryModule.Get().OnAssetRemoved().AddStatic(HandleAdded, this);

	const auto HandleRenamed = [](const FAssetData& Data, const FString&, FComponentTypeRegistryData* Parent)
	{
		Parent->PendingAssetData.Push(Data);
	};
	AssetRegistryModule.Get().OnAssetRenamed().AddStatic(HandleRenamed, this);
}

void FComponentTypeRegistryData::RefreshComponentList()
{
	ComponentClassList.Empty();
	ComponentTypeList.Empty();

	struct SortComboEntry
	{
		bool operator () (const FComponentClassComboEntryPtr& A, const FComponentClassComboEntryPtr& B) const
		{
			bool bResult = false;

			// check headings first, if they are the same compare the individual entries
			int32 HeadingCompareResult = FCString::Stricmp(*A->GetHeadingText(), *B->GetHeadingText());
			if (HeadingCompareResult == 0)
			{
				bResult = FCString::Stricmp(*A->GetClassName(), *B->GetClassName()) < 0;
			}
			else if (CommonClassGroup == A->GetHeadingText())
			{
				bResult = true;
			}
			else if (CommonClassGroup == B->GetHeadingText())
			{
				bResult = false;
			}
			else
			{
				bResult = HeadingCompareResult < 0;
			}

			return bResult;
		}
	};

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	if (GetDefault<UEditorExperimentalSettings>()->bBlueprintableComponents)
	{
		FString NewComponentsHeading = LOCTEXT("NewComponentsHeading", "Scripting").ToString();
		// Add new C++ component class
		FComponentClassComboEntryPtr NewClassHeader = MakeShareable(new FComponentClassComboEntry(NewComponentsHeading));
		ComponentClassList.Add(NewClassHeader);

		FComponentClassComboEntryPtr NewCPPClass = MakeShareable(new FComponentClassComboEntry(NewComponentsHeading, UActorComponent::StaticClass(), true, EComponentCreateAction::CreateNewCPPClass));
		ComponentClassList.Add(NewCPPClass);

		FComponentClassComboEntryPtr NewBPClass = MakeShareable(new FComponentClassComboEntry(NewComponentsHeading, UActorComponent::StaticClass(), true, EComponentCreateAction::CreateNewBlueprintClass));
		ComponentClassList.Add(NewBPClass);

		FComponentClassComboEntryPtr NewSeparator(new FComponentClassComboEntry());
		ComponentClassList.Add(NewSeparator);
	}

	TArray<FComponentClassComboEntryPtr> SortedClassList;
	TArray<FName> InMemoryClasses;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		// If this is a subclass of Actor Component, not abstract, and tagged as spawnable from Kismet
		if (Class->IsChildOf(UActorComponent::StaticClass()))
		{
			InMemoryClasses.Push(Class->GetFName());
			if (!Class->HasAnyClassFlags(CLASS_Abstract) && Class->HasMetaData(FBlueprintMetadata::MD_BlueprintSpawnableComponent) && !FKismetEditorUtilities::IsClassABlueprintSkeleton(Class)) //@TODO: Fold this logic together with the one in UEdGraphSchema_K2::GetAddComponentClasses
			{
				TArray<FString> ClassGroupNames;
				Class->GetClassGroupNames(ClassGroupNames);

				if (ClassGroupNames.Contains(CommonClassGroup))
				{
					FString ClassGroup = CommonClassGroup;
					FComponentClassComboEntryPtr NewEntry(new FComponentClassComboEntry(ClassGroup, Class, ClassGroupNames.Num() <= 1, EComponentCreateAction::SpawnExistingClass));
					SortedClassList.Add(NewEntry);
				}
				if (ClassGroupNames.Num() && !ClassGroupNames[0].Equals(CommonClassGroup))
				{
					const bool bIncludeInFilter = true;

					FString ClassGroup = ClassGroupNames[0];
					FComponentClassComboEntryPtr NewEntry(new FComponentClassComboEntry(ClassGroup, Class, bIncludeInFilter, EComponentCreateAction::SpawnExistingClass));
					SortedClassList.Add(NewEntry);
				}
			}

			FComponentTypeEntry Entry = { Class->GetName(), FString(), Class };
			ComponentTypeList.Add(Entry);
		}
	}

	AddBasicShapeComponents(SortedClassList);

	{
		// make sure that we add any user created classes immediately, generally this will not create anything (because assets have not been discovered yet), but asset discovery
		// should be allowed to take place at any time:
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FName> ClassNames;
		ClassNames.Add(UActorComponent::StaticClass()->GetFName());
		TSet<FName> DerivedClassNames;
		AssetRegistryModule.Get().GetDerivedClassNames(ClassNames, TSet<FName>(), DerivedClassNames);

		const bool bIncludeInFilter = true;

		auto InMemoryClassesSet = TSet<FName>(InMemoryClasses);
		auto OnDiskClasses = DerivedClassNames.Difference(InMemoryClassesSet);

		// GetAssetsByClass call is a cludget to get the full asset paths for the blueprints we care about, Bob T. thinks 
		// that the Asset Registry could just keep asset paths:
		TArray<FAssetData> BlueprintAssetData;
		AssetRegistryModule.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetFName(), BlueprintAssetData);

		for (auto OnDiskClass : OnDiskClasses)
		{
			FName AssetPath;
			FString FixedString = OnDiskClass.ToString();
			FixedString.RemoveFromEnd(TEXT("_C"));
			for (auto Blueprint : BlueprintAssetData)
			{
				if (Blueprint.AssetName.ToString() == FixedString)
				{
					AssetPath = Blueprint.ObjectPath;
					break;
				}
			}

			FComponentTypeEntry Entry = { FixedString, AssetPath.ToString(), nullptr };
			ComponentTypeList.Add(Entry);

			FComponentClassComboEntryPtr NewEntry(new FComponentClassComboEntry(BlueprintComponents, FixedString, AssetPath, bIncludeInFilter));
			SortedClassList.Add(NewEntry);
		}
	}

	if (SortedClassList.Num() > 0)
	{
		Sort(SortedClassList.GetData(), SortedClassList.Num(), SortComboEntry());

		FString PreviousHeading;
		for (int32 ClassIndex = 0; ClassIndex < SortedClassList.Num(); ClassIndex++)
		{
			FComponentClassComboEntryPtr& CurrentEntry = SortedClassList[ClassIndex];

			const FString& CurrentHeadingText = CurrentEntry->GetHeadingText();

			if (CurrentHeadingText != PreviousHeading)
			{
				// This avoids a redundant separator being added to the very top of the list
				if (ClassIndex > 0)
				{
					FComponentClassComboEntryPtr NewSeparator(new FComponentClassComboEntry());
					ComponentClassList.Add(NewSeparator);
				}
				FComponentClassComboEntryPtr NewHeading(new FComponentClassComboEntry(CurrentHeadingText));
				ComponentClassList.Add(NewHeading);

				PreviousHeading = CurrentHeadingText;
			}

			ComponentClassList.Add(CurrentEntry);
		}
	}
}

void FComponentTypeRegistryData::Tick(float)
{
	bool bRequiresRefresh = false;

	if (PendingAssetData.Num() != 0)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FName> ClassNames;
		ClassNames.Add(UActorComponent::StaticClass()->GetFName());
		TSet<FName> DerivedClassNames;
		AssetRegistryModule.Get().GetDerivedClassNames(ClassNames, TSet<FName>(), DerivedClassNames);

		for (auto Asset : PendingAssetData)
		{
			const FName BPParentClassName(TEXT("ParentClass"));

			bool FoundBPNotify = false;
			for (TMap<FName, FString>::TConstIterator TagIt(Asset.TagsAndValues); TagIt; ++TagIt)
			{
				FString TagValue = Asset.TagsAndValues.FindRef(BPParentClassName);
				const FString ObjectPath = FPackageName::ExportTextPathToObjectPath(TagValue);
				FName ObjectName = FName(*FPackageName::ObjectPathToObjectName(ObjectPath));
				if (DerivedClassNames.Contains(ObjectName))
				{
					bRequiresRefresh = true;
					break;
				}
			}
		}
		PendingAssetData.Empty();
	}

	if (bRequiresRefresh)
	{
		RefreshComponentList();
		ComponentListChanged.Broadcast();
	}
}

FComponentTypeRegistry& FComponentTypeRegistry::Get()
{
	static FComponentTypeRegistry ComponentRegistry;
	return ComponentRegistry;
}

FOnComponentTypeListChanged& FComponentTypeRegistry::SubscribeToComponentList(TArray<FComponentClassComboEntryPtr>*& OutComponentList)
{
	check(Data);
	OutComponentList = &Data->ComponentClassList;
	return Data->ComponentListChanged;
}

FOnComponentTypeListChanged& FComponentTypeRegistry::SubscribeToComponentList(const TArray<FComponentTypeEntry>*& OutComponentList)
{
	check(Data);
	OutComponentList = &Data->ComponentTypeList;
	return Data->ComponentListChanged;
}

FOnComponentTypeListChanged& FComponentTypeRegistry::GetOnComponentTypeListChanged()
{
	check(Data);
	return Data->ComponentListChanged;
}

FComponentTypeRegistry::FComponentTypeRegistry()
	: Data(nullptr)
{
	Data = new FComponentTypeRegistryData();
	Data->RefreshComponentList();
}

#undef LOCTEXT_NAMESPACE