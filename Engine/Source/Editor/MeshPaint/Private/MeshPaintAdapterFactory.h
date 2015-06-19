// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshPaintModule.h"

class FMeshPaintAdapterFactory
{
public:
	static TArray<TSharedPtr<class IMeshPaintGeometryAdapterFactory>> FactoryList;

public:
	static TSharedPtr<class IMeshPaintGeometryAdapter> CreateAdapterForMesh(UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex);
};