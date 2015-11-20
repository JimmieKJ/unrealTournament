// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LogVisualizer.h"
#include "Misc/CoreMisc.h"
#include "LogVisualizerSettings.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "ISettingsModule.h"
#include "UnrealEdMisc.h"
#endif // WITH_EDITOR

ULogVisualizerSettings::ULogVisualizerSettings(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
	, DebugMeshMaterialFakeLightName(TEXT("/Engine/EngineDebugMaterials/DebugMeshMaterialFakeLight.DebugMeshMaterialFakeLight"))
{
	TrivialLogsThreshold = 1;
	DefaultCameraDistance = 150;
	bSearchInsideLogs = true;
	GraphsBackgroundColor = FColor(0, 0, 0, 70);
	bResetDataWithNewSession = false;
	bDrawExtremesOnGraphs = false;
	bUsePlayersOnlyForPause = true;
}

class UMaterial* ULogVisualizerSettings::GetDebugMeshMaterial()
{
	if (DebugMeshMaterialFakeLight == nullptr)
	{
		DebugMeshMaterialFakeLight = LoadObject<UMaterial>(NULL, *DebugMeshMaterialFakeLightName, NULL, LOAD_None, NULL);
	}

	return DebugMeshMaterialFakeLight;
}

void ULogVisualizerSettings::SavePresistentData()
{
	if (bPresistentFilters)
	{
		PresistentFilters = FVisualLoggerFilters::Get();
	}
	else
	{
		PresistentFilters = FVisualLoggerFilters();
	}
	SaveConfig();
}

void ULogVisualizerSettings::ClearPresistentData()
{
	if (bPresistentFilters)
	{
		PresistentFilters = FVisualLoggerFilters();
	}
}

void ULogVisualizerSettings::LoadPresistentData()
{
	if (bPresistentFilters)
	{
		static_cast<FVisualLoggerFiltersData&>(FVisualLoggerFilters::Get()) = PresistentFilters;
	}
	else
	{
		FVisualLoggerFilters::Get() = FVisualLoggerFilters();
	}
}

#if WITH_EDITOR
void ULogVisualizerSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (!FUnrealEdMisc::Get().IsDeletePreferences())
	{
		SaveConfig();
	}

	SettingChangedEvent.Broadcast(Name);
}
#endif

//////////////////////////////////////////////////////////////////////////
// FVisualLoggerFilters
//////////////////////////////////////////////////////////////////////////
TSharedPtr< struct FVisualLoggerFilters > FVisualLoggerFilters::StaticInstance;

FVisualLoggerFilters& FVisualLoggerFilters::Get()
{
	return *StaticInstance;
}

void FVisualLoggerFilters::Initialize()
{
	StaticInstance = MakeShareable(new FVisualLoggerFilters);
	FVisualLoggerDatabase::Get().GetEvents().OnNewItem.AddRaw(StaticInstance.Get(), &FVisualLoggerFilters::OnNewItemHandler);
}

void FVisualLoggerFilters::Shutdown()
{
	FVisualLoggerDatabase::Get().GetEvents().OnNewItem.RemoveAll(StaticInstance.Get());
	StaticInstance.Reset();
}

void FVisualLoggerFilters::OnNewItemHandler(const FVisualLoggerDBRow& DBRow, int32 ItemIndex)
{
	const FVisualLogDevice::FVisualLogEntryItem& Item = DBRow.GetItems()[ItemIndex];
	TArray<FVisualLoggerCategoryVerbosityPair> Categories;
	FVisualLoggerHelpers::GetCategories(Item.Entry, Categories);
	for (auto& CategoryAndVerbosity : Categories)
	{
		AddCategory(CategoryAndVerbosity.CategoryName.ToString(), ELogVerbosity::All);
	}

	TMap<FString, TArray<FString> > OutCategories;
	FVisualLoggerHelpers::GetHistogramCategories(Item.Entry, OutCategories);
	for (const auto& CurrentCategory : OutCategories)
	{
		for (const auto& CurrentDataName : CurrentCategory.Value)
		{
			const FString GraphFilterName = CurrentCategory.Key + TEXT("$") + CurrentDataName;	
			AddCategory(GraphFilterName, ELogVerbosity::All);
		}
	}
}

void FVisualLoggerFilters::AddCategory(FString InName, ELogVerbosity::Type InVerbosity)
{
	const int32 Num = Categories.Num();
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const FCategoryFilter& Filter = Categories[Index];
		if (Filter.CategoryName == InName)
		{
			return;
		}
	}

	FCategoryFilter Filter;
	Filter.CategoryName = InName;
	Filter.LogVerbosity = InVerbosity;
	Filter.Enabled = true;
	Categories.Add(Filter);

	FastCategoryFilterMap.Reset(); // we need to recreate cache - pointers can be broken
	FastCategoryFilterMap.Reserve(Categories.Num());
	for (int32 Index = 0; Index < Categories.Num(); Index++)
	{
		FastCategoryFilterMap.Add(*Categories[Index].CategoryName, &Categories[Index]);
	}

	OnFilterCategoryAdded.Broadcast(InName, InVerbosity);
}

void FVisualLoggerFilters::RemoveCategory(FString InName)
{
	for (int32 Index = 0; Index < Categories.Num(); ++Index)
	{
		const FCategoryFilter& Filter = Categories[Index];
		if (Filter.CategoryName == InName)
		{
			Categories.RemoveAt(Index);
			FastCategoryFilterMap.Remove(*InName);
			break;
		}
	}

	OnFilterCategoryRemoved.Broadcast(InName);
}

FCategoryFilter& FVisualLoggerFilters::GetCategoryByName(const FName& InName)
{
	FCategoryFilter* FilterPtr = FastCategoryFilterMap.Contains(InName) ? FastCategoryFilterMap[InName] : nullptr;
	if (FilterPtr)
	{
		return *FilterPtr;
	}

	static FCategoryFilter NoCategory;
	return NoCategory;
}

FCategoryFilter& FVisualLoggerFilters::GetCategoryByName(const FString& InName)
{
	const FName NameAsName = *InName;
	FCategoryFilter* FilterPtr = FastCategoryFilterMap.Contains(NameAsName) ? FastCategoryFilterMap[NameAsName] : nullptr;
	if (FilterPtr)
	{
		return *FilterPtr;
	}

	static FCategoryFilter NoCategory;
	return NoCategory;
}

bool FVisualLoggerFilters::MatchObjectName(FString String)
{
	return SelectedClasses.Num() == 0 || SelectedClasses.Find(String) != INDEX_NONE;
}


void FVisualLoggerFilters::SelectObject(FString ObjectName)
{
	SelectedClasses.AddUnique(ObjectName);
}

void FVisualLoggerFilters::RemoveObjectFromSelection(FString ObjectName)
{
	SelectedClasses.Remove(ObjectName);
}

const TArray<FString>& FVisualLoggerFilters::GetSelectedObjects() const
{
	return SelectedClasses;
}

bool FVisualLoggerFilters::MatchCategoryFilters(FString String, ELogVerbosity::Type Verbosity)
{
	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	bool bFoundFilter = false;
	for (const FCategoryFilter& Filter : Categories)
	{
		bFoundFilter = Filter.CategoryName == String;
		if (bFoundFilter)
		{
			const bool bFineWithSearchString = Settings->bSearchInsideLogs == true || (SearchBoxFilter.Len() == 0 || Filter.CategoryName.Find(SearchBoxFilter) != INDEX_NONE);
			return bFineWithSearchString && Verbosity <= Filter.LogVerbosity && Filter.Enabled;
		}
	}

	return bFoundFilter;
}

void FVisualLoggerFilters::DeactivateAllButThis(const FString& InName)
{
	for (FCategoryFilter& Filter : Categories)
	{
		Filter.Enabled = Filter.CategoryName != InName ? false : true;
	}
}

void FVisualLoggerFilters::EnableAllCategories()
{
	for (FCategoryFilter& Filter : Categories)
	{
		Filter.Enabled = true;
	}
}


void FVisualLoggerFilters::Reset()
{
	FastCategoryFilterMap.Reset();
	SearchBoxFilter = FString();
	ObjectNameFilter = FString();

	Categories.Reset();
	SelectedClasses.Reset();
}

void FVisualLoggerFilters::DisableGraphData(FName GraphName, FName DataName, bool SetAsDisabled)
{
	const FName FullName = *(GraphName.ToString() + TEXT("$") + DataName.ToString());
	if (SetAsDisabled)
	{
		DisabledGraphDatas.AddUnique(FullName);
	}
	else
	{
		DisabledGraphDatas.RemoveSwap(FullName);
	}
}


bool FVisualLoggerFilters::IsGraphDataDisabled(FName GraphName, FName DataName)
{
	const FName FullName = *(GraphName.ToString() + TEXT("$") + DataName.ToString());
	return DisabledGraphDatas.Find(FullName) != INDEX_NONE;
}
