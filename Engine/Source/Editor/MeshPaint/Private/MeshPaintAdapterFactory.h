// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshPaintModule.h"

class UMeshComponent;

class FMeshPaintAdapterFactory
{
public:
	static TArray<TSharedPtr<class IMeshPaintGeometryAdapterFactory>> FactoryList;

public:
	static TSharedPtr<class IMeshPaintGeometryAdapter> CreateAdapterForMesh(UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex);
	static void InitializeAdapterGlobals();
};
