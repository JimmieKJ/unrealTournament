// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintProfilerSettings.generated.h"

/** Heat map display mode */
UENUM()
enum class EBlueprintProfilerHeatMapDisplayMode : uint8
{
	None = 0,
	Inclusive,
	Average,
	MaxTiming,
	Total,
	HottestPath,
	PinToPin
};

/** Heat map level metrics type */
UENUM()
enum class EBlueprintProfilerHeatLevelMetricsType : uint8
{
	ClassRelative,
	FrameRelative,
	CustomThresholds
};

UCLASS(config=EditorPerProjectUserSettings)
class KISMET_API UBlueprintProfilerSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** Display statistics by instance rather than by blueprint */
	UPROPERTY(config)
	bool bDisplayByInstance;

	/** Restrict the displayed instances to match the blueprint debug filter */
	UPROPERTY(config)
	bool bScopeToDebugInstance;

	/** Only Display events that are applicable to the current graph */
	UPROPERTY(config)
	bool bGraphFilter;

	/** Display timings from pure nodes */
	UPROPERTY(config)
	bool bDisplayPure;

	/** Display events for parent blueprints */
	UPROPERTY(config)
	bool bDisplayInheritedEvents;

	/** Blueprint Statistics are Averaged or Summed By Instance */
	UPROPERTY(config)
	bool bAverageBlueprintStats;

	/** Current graph node heat map display mode */
	UPROPERTY(config)
	EBlueprintProfilerHeatMapDisplayMode GraphNodeHeatMapDisplayMode;

	/** Current wire heat map display mode */
	UPROPERTY(config)
	EBlueprintProfilerHeatMapDisplayMode WireHeatMapDisplayMode;

	/** Current heat level metrics type */
	UPROPERTY(config)
	EBlueprintProfilerHeatLevelMetricsType HeatLevelMetricsType;

	/** Custom performance threshold for event timings in ms. */
	UPROPERTY(config)
	float CustomEventPerformanceThreshold;

	/** Custom performance threshold for average timings in ms. */
	UPROPERTY(config)
	float CustomAveragePerformanceThreshold;

	/** Custom performance threshold for inclusive timings in ms. */
	UPROPERTY(config)
	float CustomInclusivePerformanceThreshold;

	/** Custom performance threshold for max timings in ms. */
	UPROPERTY(config)
	float CustomMaxPerformanceThreshold;

	/** Default custom threshold values */
	/** @TODO - maybe we don't have a need for these anymore? they're currently supporting the "reset to default" feature in the UI - not sure we have that in the "normal" editor settings UI */
	static float CustomEventPerformanceThresholdDefaultValue;
	static float CustomAveragePerformanceThresholdDefaultValue;
	static float CustomInclusivePerformanceThresholdDefaultValue;
	static float CustomMaxPerformanceThresholdDefaultValue;
};
