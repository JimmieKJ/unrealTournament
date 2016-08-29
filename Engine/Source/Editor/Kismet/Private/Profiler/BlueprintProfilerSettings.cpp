// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "BlueprintProfilerSettings.h"
#include "ScriptPerfData.h"

float UBlueprintProfilerSettings::CustomEventPerformanceThresholdDefaultValue = 1.f;
float UBlueprintProfilerSettings::CustomAveragePerformanceThresholdDefaultValue = 0.2f;
float UBlueprintProfilerSettings::CustomInclusivePerformanceThresholdDefaultValue = 0.25f;
float UBlueprintProfilerSettings::CustomMaxPerformanceThresholdDefaultValue = 0.5f;

UBlueprintProfilerSettings::UBlueprintProfilerSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bDisplayByInstance(true)
	, bScopeToDebugInstance(false)
	, bGraphFilter(false)
	, bDisplayPure(true)
	, bDisplayInheritedEvents(true)
	, bAverageBlueprintStats(true)
	, GraphNodeHeatMapDisplayMode(EBlueprintProfilerHeatMapDisplayMode::None)
	, WireHeatMapDisplayMode(EBlueprintProfilerHeatMapDisplayMode::None)
	, HeatLevelMetricsType(EBlueprintProfilerHeatLevelMetricsType::ClassRelative)
	, CustomEventPerformanceThreshold(CustomEventPerformanceThresholdDefaultValue)
	, CustomAveragePerformanceThreshold(CustomAveragePerformanceThresholdDefaultValue)
	, CustomInclusivePerformanceThreshold(CustomInclusivePerformanceThresholdDefaultValue)
	, CustomMaxPerformanceThreshold(CustomMaxPerformanceThresholdDefaultValue)
{
	FScriptPerfData::EnableBlueprintStatAverage(bAverageBlueprintStats);
}