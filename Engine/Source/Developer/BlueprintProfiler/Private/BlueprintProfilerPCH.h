// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#ifndef _INC_BLUEPRINTPROFILER
#define _INC_BLUEPRINTPROFILER

#include "CoreUObject.h"
#include "Core.h"
#include "Script.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/Kismet/Public/Profiler/TracePath.h"
#include "Editor/Kismet/Public/Profiler/EventExecution.h"
#include "Editor/Kismet/Public/Profiler/ScriptPerfData.h"
#include "Editor/BlueprintGraph/Public/BlueprintGraphDefinitions.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetEditorUtilities.h"
#include "Editor/GraphEditor/Public/GraphEditorSettings.h"
#include "Editor/UnrealEd/Public/EdGraphUtilities.h"
#include "Private/BlueprintProfilerConnectionDrawingPolicy.h"
#endif // WITH_EDITOR

#include "BlueprintProfilerModule.h"
#include "Public/BlueprintProfiler.h"

DECLARE_STATS_GROUP(TEXT("BlueprintProfiler"), STATGROUP_BlueprintProfiler, STATCAT_Advanced);

#endif // _INC_BLUEPRINTPROFILER
