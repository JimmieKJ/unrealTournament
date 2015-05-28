// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
// Includes all necessary PhysX headers

#pragma once

#if WITH_PHYSX

MSVC_PRAGMA(warning(push))
MSVC_PRAGMA(warning(disable : 4946)) // reinterpret_cast used between related classes

PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS

#if USING_CODE_ANALYSIS
	MSVC_PRAGMA(warning(push))
	MSVC_PRAGMA(warning(disable : ALL_CODE_ANALYSIS_WARNINGS))
#endif	// USING_CODE_ANALYSIS

#pragma pack(push,8)

#include "Px.h"
#include "PxPhysicsAPI.h"
#include "PxRenderBuffer.h"
#include "PxExtensionsAPI.h"
#include "PxCollectionExt.h"
#include "PxVisualDebuggerExt.h"

// vehicle related header files
#include "PxVehicleSDK.h"
#include "PxVehicleNoDrive.h"
#include "PxVehicleDrive4W.h"
//#include "PxVehicleSuspLimitConstraintShader.h"
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
#include "NxDestructibleAsset.h"
#include "NxDestructibleActor.h"

#if WITH_APEX_CLOTHING
#include "NxModuleClothing.h"
#include "NxClothingAsset.h"
#include "NxClothingActor.h"
#include "NxClothingCollision.h"
#endif

#if WITH_APEX_LEGACY
#include "NxModuleLegacy.h"
#endif

// Utilities
#include "NxParamUtils.h"

#endif // #if WITH_APEX

#pragma pack(pop)

#if USING_CODE_ANALYSIS
	MSVC_PRAGMA(warning(pop))
#endif	// USING_CODE_ANALYSIS

MSVC_PRAGMA(warning(pop))

PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS

using namespace physx;

#endif // WITH_PHYSX