// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Engine/BookMark.h"
#include "StaticMeshResources.h"
#include "EditorSupportDelegates.h"
#include "MouseDeltaTracker.h"
#include "ScopedTransaction.h"
#include "SurfaceIterators.h"
#include "SoundDefinitions.h"
#include "LevelEditor.h"
#include "Toolkits/ToolkitManager.h"
#include "EditorLevelUtils.h"
#include "DynamicMeshBuilder.h"

#include "ActorEditorUtils.h"
#include "EditorStyle.h"
#include "ComponentVisualizer.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Polys.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/LevelStreaming.h"

DEFINE_LOG_CATEGORY(LogEditorModes);

// Builtin editor mode constants
const FEditorModeID FBuiltinEditorModes::EM_None = NAME_None;
const FEditorModeID FBuiltinEditorModes::EM_Default(TEXT("EM_Default"));
const FEditorModeID FBuiltinEditorModes::EM_Placement(TEXT("PLACEMENT"));
const FEditorModeID FBuiltinEditorModes::EM_Bsp(TEXT("BSP"));
const FEditorModeID FBuiltinEditorModes::EM_Geometry(TEXT("EM_Geometry"));
const FEditorModeID FBuiltinEditorModes::EM_InterpEdit(TEXT("EM_InterpEdit"));
const FEditorModeID FBuiltinEditorModes::EM_Texture(TEXT("EM_Texture"));
const FEditorModeID FBuiltinEditorModes::EM_MeshPaint(TEXT("EM_MeshPaint"));
const FEditorModeID FBuiltinEditorModes::EM_Landscape(TEXT("EM_Landscape"));
const FEditorModeID FBuiltinEditorModes::EM_Foliage(TEXT("EM_Foliage"));
const FEditorModeID FBuiltinEditorModes::EM_Level(TEXT("EM_Level"));
const FEditorModeID FBuiltinEditorModes::EM_StreamingLevel(TEXT("EM_StreamingLevel"));
const FEditorModeID FBuiltinEditorModes::EM_Physics(TEXT("EM_Physics"));
const FEditorModeID FBuiltinEditorModes::EM_ActorPicker(TEXT("EM_ActorPicker"));

/*------------------------------------------------------------------------------
	Default.
------------------------------------------------------------------------------*/

FEdModeDefault::FEdModeDefault()
{
	bDrawGrid = false;
	bDrawPivot = false;
	bDrawBaseInfo = false;
	bDrawWorldBox = false;
	bDrawKillZ = false;
}

