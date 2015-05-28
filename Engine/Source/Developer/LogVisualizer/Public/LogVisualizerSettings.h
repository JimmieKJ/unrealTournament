// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
GameplayDebuggerSettings.h: Declares the UGameplayDebuggerSettings class.
=============================================================================*/
#pragma once


#include "LogVisualizerSettings.generated.h"

USTRUCT()
struct FCategoryFilter
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(config)
	FString CategoryName;

	UPROPERTY(config)
	int32 LogVerbosity;

	UPROPERTY(config)
	bool Enabled;
};

USTRUCT()
struct FVisualLoggerFilters
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(config)
	FString SearchBoxFilter;
	
	UPROPERTY(config)
	FString ObjectNameFilter;
	
	UPROPERTY(config)
	TArray<FCategoryFilter> Categories;
	
	UPROPERTY(config)
	TArray<FString> SelectedClasses;
};

struct FCategoryFiltersManager;

UCLASS(config = EditorPerProjectUserSettings)
class LOGVISUALIZER_API ULogVisualizerSettings : public UObject
{
	GENERATED_UCLASS_BODY()
	friend struct FCategoryFiltersManager;

public:
	DECLARE_EVENT_OneParam(ULogVisualizerSettings, FSettingChangedEvent, FName /*PropertyName*/);
	FSettingChangedEvent& OnSettingChanged() { return SettingChangedEvent; }

	/**Whether to show trivial logs, i.e. the ones with only one entry.*/
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger")
	bool bIgnoreTrivialLogs;

	/**Threshold for trivial Logs*/
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger", meta = (EditCondition = "bIgnoreTrivialLogs", ClampMin = "0", ClampMax = "10", UIMin = "0", UIMax = "10"))
	int32 TrivialLogsThreshold;

	/**Whether to show the recent data or not. Property disabled for now.*/
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger")
	bool bStickToRecentData;

	/**Whether to reset current data or not for each new session.*/
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger")
	bool bResetDataWithNewSession;

	/**Whether to show histogram labels inside graph or outside. Property disabled for now.*/
	UPROPERTY(VisibleAnywhere, config, Category = "VisualLogger")
	bool bShowHistogramLabelsOutside;

	/** Camera distance used to setup location during reaction on log item double click */
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger", meta = (ClampMin = "10", ClampMax = "1000", UIMin = "10", UIMax = "1000"))
	float DefaultCameraDistance;

	/**Whether to search/filter categories or to get text vlogs into account too */
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger")
	bool bSearchInsideLogs;

	/** Background color for 2d graphs visualization */
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger")
	FColor GraphsBackgroundColor;

	/**Whether to store all filter settings on exit*/
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger")
	bool bPresistentFilters;

	/** Whether to extreme values on graph (data has to be provided for extreme values) */
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger")
	bool bDrawExtremesOnGraphs;

	/** Whether to use PlayersOnly during Pause or not */
	UPROPERTY(EditAnywhere, config, Category = "VisualLogger")
	bool bUsePlayersOnlyForPause;

	// UObject overrides
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	FVisualLoggerFilters CurrentFilters;
	
	UPROPERTY(config)
	FVisualLoggerFilters PresistentFilters;

private:

	// Holds an event delegate that is executed when a setting has changed.
	FSettingChangedEvent SettingChangedEvent;

};

struct FCategoryFiltersManager
{
	static FCategoryFiltersManager& Get() { return StaticManager; }

	bool MatchCategoryFilters(FString String, ELogVerbosity::Type Verbosity = ELogVerbosity::All);
	bool MatchObjectName(FString String);
	bool MatchSearchString(FString String);

	void SetSearchString(FString InString);
	FString GetSearchString();

	void SetObjectFilterString(FString InFilterString);
	FString GetObjectFilterString();

	void AddCategory(FString InName, ELogVerbosity::Type InVerbosity);
	void RemoveCategory(FString InName);
	bool IsValidCategory(FString InName);
	FCategoryFilter& GetCategory(FString InName);

	void SelectObject(FString ObjectName);
	void RemoveObjectFromSelection(FString ObjectName);
	const TArray<FString>& GetSelectedObjects();

	void SavePresistentData();
	void ClearPresistentData();
	void LoadPresistentData();

protected:
	static FCategoryFiltersManager StaticManager;
};
