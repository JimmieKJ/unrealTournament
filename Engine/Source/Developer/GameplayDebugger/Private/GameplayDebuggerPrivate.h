// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core.h"
#include "CoreUObject.h"
#include "InputCore.h"
#include "EngineDefines.h"
#include "EngineSettings.h"
#include "EngineStats.h"
#include "EngineLogs.h"
#include "EngineGlobals.h"

#include "Engine/EngineBaseTypes.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/PlayerController.h"
#include "AI/Navigation/RecastNavMesh.h"
#include "GameFramework/Pawn.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"

#include "RenderResource.h"
#include "Shader.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"

//////////////////////////////////////////////////////////////////////////

#include "GameplayDebuggingTypes.h"
#include "GameplayDebugger.h"
#include "GameplayDebuggingReplicator.h"
#include "GameplayDebuggingHUDComponent.h"
#include "GameplayDebuggingControllerComponent.h"
#include "GameplayDebuggingComponent.h"

#if WITH_EDITOR
#	include "UnrealEd.h" //right now it's needed here because of ECoordSystem 
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogGDT, Warning, All);
