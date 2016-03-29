// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Components/BrushComponent.h"

#include "../Classes/Engine/HLODMeshCullingVolume.h"

AHLODMeshCullingVolume::AHLODMeshCullingVolume(const FObjectInitializer& ObjectInitializer)
:Super(ObjectInitializer)
{
	bNotForClientOrServer = true;

	bColored = true;
	BrushColor = FColor(45, 225, 45);
}

