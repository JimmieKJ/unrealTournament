// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Volume.h"
#include "HLODMeshCullingVolume.generated.h"

/** A volume that can be added to an HLOD cluster to remove triangles from source meshes before generating HLOD mesh (this only works with the non-Simplygon mesh merging path) */
UCLASS(Experimental, MinimalAPI)
class AHLODMeshCullingVolume : public AVolume
{
	GENERATED_UCLASS_BODY()

public:

};
