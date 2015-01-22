// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LMCore.h"

/** 
 * Set to 1 to allow selecting lightmap texels by holding down T and left clicking in the editor,
 * And having debug information about that texel tracked during subsequent lighting rebuilds.
 * Be sure to set the define with the same name in Unreal!
 */
#define ALLOW_LIGHTMAP_SAMPLE_DEBUGGING	0

#include "LightmassScene.h"
#include "Mesh.h"
#include "Texture.h"
#include "Material.h"
#include "LightingMesh.h"
#include "BuildOptions.h"
#include "LightmapData.h"
#include "Mappings.h"
#include "Collision.h"
#include "BSP.h"
#include "StaticMesh.h"
#include "FluidSurface.h"
#include "Landscape.h"

namespace Lightmass
{
	extern double GStartupTime;
}

#define WORLD_MAX			2097152.0			/* Maximum size of the world */
#define HALF_WORLD_MAX		( WORLD_MAX*0.5f )	/* Half the maximum size of the world */


