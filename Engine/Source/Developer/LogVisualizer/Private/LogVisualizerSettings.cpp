// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LogVisualizer.h"
#include "Misc/CoreMisc.h"
#include "LogVisualizerSettings.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "ISettingsModule.h"
#include "GeometryEdMode.h"
#include "UnrealEdMisc.h"
#endif // WITH_EDITOR

FCategoryFiltersManager FCategoryFiltersManager::StaticManager;

ULogVisualizerSettings::ULogVisualizerSettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	TrivialLogsThreshold = 1;
	DefaultCameraDistance = 150;
	bSearchInsideLogs = true;
	GraphsBackgroundColor = FColor(0, 0, 0, 70);
	bResetDataWithNewSession = false;
	bDrawExtremesOnGraphs = false;
	bUsePlayersOnlyForPause = true;
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

bool FCategoryFiltersManager::MatchCategoryFilters(FString String, ELogVerbosity::Type Verbosity)
{
	const ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	bool bFoundFilter = false;
	for (const FCategoryFilter& Filter : Settings->CurrentFilters.Categories)
	{
		bFoundFilter = Filter.CategoryName == String;
		if (bFoundFilter)
		{
			const bool bFineWithSearchString = Settings->bSearchInsideLogs == true ||
				(Settings->bSearchInsideLogs == false && (Settings->CurrentFilters.SearchBoxFilter.Len() == 0 || Filter.CategoryName.Find(Settings->CurrentFilters.SearchBoxFilter) != INDEX_NONE));
			return bFineWithSearchString && Verbosity <= Filter.LogVerbosity && Filter.Enabled;
		}
	}

	return bFoundFilter;
}

bool FCategoryFiltersManager::MatchObjectName(FString String)
{
	const ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	return (Settings->CurrentFilters.SelectedClasses.Num() == 0 || Settings->CurrentFilters.SelectedClasses.Find(String) != INDEX_NONE);
}

bool FCategoryFiltersManager::MatchSearchString(FString String)
{
	const ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	return Settings->CurrentFilters.SearchBoxFilter == String;
}

void FCategoryFiltersManager::SetSearchString(FString InString) 
{ 
	ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->CurrentFilters.SearchBoxFilter = InString; 
}

FString FCategoryFiltersManager::GetSearchString() 
{ 
	return ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->CurrentFilters.SearchBoxFilter; 
}

void FCategoryFiltersManager::SetObjectFilterString(FString InFilterString) 
{ 
	ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->CurrentFilters.ObjectNameFilter = InFilterString; 
}

FString FCategoryFiltersManager::GetObjectFilterString()
{ 
	return ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->CurrentFilters.ObjectNameFilter; 
}

void FCategoryFiltersManager::AddCategory(FString InName, ELogVerbosity::Type InVerbosity)
{
	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	FCategoryFilter Filter;
	Filter.CategoryName = InName;
	Filter.LogVerbosity = InVerbosity;
	Filter.Enabled = true;
	Settings->CurrentFilters.Categories.Add(Filter);
}

void FCategoryFiltersManager::RemoveCategory(FString InName)
{
	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	for (int32 Index = 0; Index < Settings->CurrentFilters.Categories.Num(); ++Index)
	{
		const FCategoryFilter& Filter = Settings->CurrentFilters.Categories[Index];
		if (Filter.CategoryName == InName)
		{
			Settings->CurrentFilters.Categories.RemoveAt(Index);
			break;
		}
	}
}

bool FCategoryFiltersManager::IsValidCategory(FString InName)
{
	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	for (const FCategoryFilter& Filter : Settings->CurrentFilters.Categories)
	{
		if (Filter.CategoryName == InName)
		{
			return true;
		}
	}
	return Settings->CurrentFilters.Categories.Num() == 0;
}

FCategoryFilter& FCategoryFiltersManager::GetCategory(FString InName)
{
	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	for (FCategoryFilter& Filter : Settings->CurrentFilters.Categories)
	{
		if (Filter.CategoryName == InName)
		{
			return Filter;
		}
	}

	static FCategoryFilter NoCategory;
	return NoCategory;
}

void FCategoryFiltersManager::SelectObject(FString ObjectName)
{
	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	Settings->CurrentFilters.SelectedClasses.AddUnique(ObjectName);
}

void FCategoryFiltersManager::RemoveObjectFromSelection(FString ObjectName)
{
	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	Settings->CurrentFilters.SelectedClasses.Remove(ObjectName);
}

const TArray<FString>& FCategoryFiltersManager::GetSelectedObjects()
{
	return ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>()->CurrentFilters.SelectedClasses;
}

void FCategoryFiltersManager::SavePresistentData()
{
	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	if (Settings->bPresistentFilters)
	{
		Settings->PresistentFilters = Settings->CurrentFilters;
	}
	else
	{
		Settings->PresistentFilters = FVisualLoggerFilters();
	}
	Settings->SaveConfig();

}

void FCategoryFiltersManager::ClearPresistentData()
{
	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	if (Settings->bPresistentFilters)
	{
		Settings->PresistentFilters = FVisualLoggerFilters();
	}
}

void FCategoryFiltersManager::LoadPresistentData()
{
	ULogVisualizerSettings* Settings = ULogVisualizerSettings::StaticClass()->GetDefaultObject<ULogVisualizerSettings>();
	if (Settings->bPresistentFilters)
	{
		Settings->CurrentFilters = Settings->PresistentFilters;
	}
	else
	{
		Settings->CurrentFilters = FVisualLoggerFilters();
	}
}