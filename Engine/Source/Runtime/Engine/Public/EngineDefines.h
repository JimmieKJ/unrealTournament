// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
// 
// Public defines form the Engine

#pragma once

/*-----------------------------------------------------------------------------
	Configuration defines
-----------------------------------------------------------------------------*/

/** 
 *   Whether or not compiling with PhysX
 */
#ifndef WITH_PHYSX
	#define WITH_PHYSX 1
#endif

/** 
 *   Whether or not compiling with APEX extensions to PhysX
 */
#ifndef WITH_APEX
	#define WITH_APEX (1 && WITH_PHYSX)
#endif

/** 
 *   Whether or not compiling with Vehicle extensions to PhysX
 */
#ifndef WITH_VEHICLE
	#define WITH_VEHICLE (1 && WITH_PHYSX)
#endif

#ifndef WITH_PHYSICS_COOKING
	#define WITH_PHYSICS_COOKING (WITH_EDITOR || WITH_APEX)		//APEX currently relies on cooking even at runtime
#endif

#if WITH_APEX
#ifndef WITH_SUBSTEPPING
	#define WITH_SUBSTEPPING 1
#endif

#ifndef WITH_APEX_CLOTHING
	#define WITH_APEX_CLOTHING	(1 && WITH_APEX)
#endif // WITH_APEX_CLOTHING

#ifndef WITH_APEX_LEGACY
	#define WITH_APEX_LEGACY	1
#endif // WITH_APEX_LEGACY

#endif // WITH_APEX

#if WITH_APEX_CLOTHING
#ifndef WITH_CLOTH_COLLISION_DETECTION
	#define WITH_CLOTH_COLLISION_DETECTION (1 && WITH_APEX_CLOTHING)
#endif//WITH_CLOTH_COLLISION_DETECTION
#endif //WITH_APEX_CLOTHING

#ifndef ENABLE_VISUAL_LOG
#	define ENABLE_VISUAL_LOG (1 && !NO_LOGGING && !USING_CODE_ANALYSIS && !(UE_BUILD_SHIPPING || UE_BUILD_TEST))
#endif

#ifndef WITH_FIXED_AREA_ENTERING_COST
	#define WITH_FIXED_AREA_ENTERING_COST 1
#endif // WITH_FIXED_AREA_ENTERING_COST

// If set, recast will use async workers for rebuilding tiles in runtime
// All access to tile data must be guarded with critical sections
#ifndef RECAST_ASYNC_REBUILDING
	#define RECAST_ASYNC_REBUILDING	1
#endif

/*-----------------------------------------------------------------------------
	Size of the world.
-----------------------------------------------------------------------------*/

#define WORLD_MAX					2097152.0				/* Maximum size of the world */
#define HALF_WORLD_MAX				(WORLD_MAX * 0.5)		/* Half the maximum size of the world */
#define HALF_WORLD_MAX1				(HALF_WORLD_MAX - 1.0)	/* Half the maximum size of the world minus one */

#define DEFAULT_ORTHOZOOM			10000.0					/* Default 2D viewport zoom */