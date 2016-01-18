// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "HierarchicalLODVolume.h"

AHierarchicalLODVolume::AHierarchicalLODVolume(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bColored = true;
	BrushColor.R = 255;
	BrushColor.G = 100;
	BrushColor.B = 255;
	BrushColor.A = 255;
}
