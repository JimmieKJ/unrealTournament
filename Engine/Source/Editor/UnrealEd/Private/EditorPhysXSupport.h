// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorPhysXSupport.h: Editor version of the engine's PhysXSupport.h
=============================================================================*/

#pragma once

#if WITH_PHYSX

#pragma warning( push )
#pragma warning( disable : 4946 ) // reinterpret_cast used between related classes

PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS

#if USING_CODE_ANALYSIS
	#pragma warning( push )
	#pragma warning( disable : ALL_CODE_ANALYSIS_WARNINGS )
#endif	// USING_CODE_ANALYSIS

#pragma pack(push,8)

#include "Px.h"
#include "PxPhysicsAPI.h"
#include "PxRenderBuffer.h"
#include "PxExtensionsAPI.h"
#include "PxVisualDebuggerExt.h"
//#include "PxDefaultCpuDispatcher.h"

// vehicle related header files
//#include "PxVehicleSDK.h"
//#include "PxVehicleUtils.h"

// utils
#include "PxGeometryQuery.h"
#include "PxMeshQuery.h"
#include "PxTriangle.h"

// APEX
#if WITH_APEX
// Framework
#include "NxApex.h"

// Modules
#include "NxModuleDestructible.h"
#include "NxModuleClothing.h"

// Assets
#include "NxDestructibleAsset.h"
#include "NxClothingAsset.h"

// Actors
#include "NxDestructibleActor.h"
#include "NxClothingActor.h"

// Utilities
#include "NxParamUtils.h"

#endif // #if WITH_APEX

#pragma pack(pop)

#if USING_CODE_ANALYSIS
	#pragma warning( pop )
#endif	// USING_CODE_ANALYSIS

#pragma warning( pop )

PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS

using namespace physx;

#endif // WITH_PHYSX
