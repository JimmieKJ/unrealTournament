// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#ifndef __UnrealEd_h__
#define __UnrealEd_h__

#ifdef __cplusplus // Xcode needs that to use this file as a precompiled header for indexing

#if WITH_EDITOR

#include "EngineDefines.h"

#include "SlateBasics.h"
#include "EditorStyle.h"

#include "EditorComponents.h"
#include "EditorReimportHandler.h"
#include "TexAlignTools.h"

#include "TickableEditorObject.h"

#include "UnrealEdClasses.h"

#include "Editor.h"

#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"

#include "EditorModeRegistry.h"
#include "EditorModes.h"

#include "MRUList.h"
#include "Projects.h"


//#include "../Private/GeomFitUtils.h"

extern UNREALED_API class UUnrealEdEngine* GUnrealEd;

#include "UnrealEdMisc.h"
#include "EditorDirectories.h"
#include "Utils.h"
#include "FileHelpers.h"
#include "EditorModeInterpolation.h"
#include "PhysicsManipulationMode.h"
#include "PhysicsAssetUtils.h"

#include "ParticleDefinitions.h"

#include "Dialogs/Dialogs.h"
#include "Viewports.h"

#endif // WITH_EDITOR

UNREALED_API int32 EditorInit( class IEngineLoop& EngineLoop );
UNREALED_API void EditorExit();

#include "UnrealEdMessages.h"

#include "EditorAnalytics.h"

#endif // __cplusplus

#endif	// __UnrealEd_h__
