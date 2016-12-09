// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Profiler/BlueprintProfilerSettings.h"

UBlueprintProfilerSettings::UBlueprintProfilerSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bDisplayByInstance(true)
	, bScopeToDebugInstance(false)
	, bGraphFilter(false)
	, bDisplayPure(true)
	, bDisplayInheritedEvents(true)
	, GraphNodeHeatMapDisplayMode(EBlueprintProfilerHeatMapDisplayMode::Average)
	, WireHeatMapDisplayMode(EBlueprintProfilerHeatMapDisplayMode::None)
	, HeatLevelMetricsType(EBlueprintProfilerHeatLevelMetricsType::ClassRelative)
	// Copied out from CustomStatisticThresholdValues.
	, CustomEventPerformanceThreshold(1.f)
	, CustomAveragePerformanceThreshold(0.2f)
	, CustomInclusivePerformanceThreshold(0.25f)
	, CustomMaxPerformanceThreshold(0.5f)
	, bThresholdsModified(false)
{
}
