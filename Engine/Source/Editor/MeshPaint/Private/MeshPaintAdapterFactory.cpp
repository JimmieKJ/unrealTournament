// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MeshPaintAdapterFactory.h"


TArray<TSharedPtr<class IMeshPaintGeometryAdapterFactory>> FMeshPaintAdapterFactory::FactoryList;

TSharedPtr<class IMeshPaintGeometryAdapter> FMeshPaintAdapterFactory::CreateAdapterForMesh(UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex)
{
	TSharedPtr<IMeshPaintGeometryAdapter> Result;

	for (const auto& Factory : FactoryList)
	{
		Result = Factory->Construct(InComponent, InPaintingMeshLODIndex, InUVChannelIndex);

		if (Result.IsValid())
		{
			break;
		}
	}

	return Result;
}

void FMeshPaintAdapterFactory::InitializeAdapterGlobals()
{
	for (const auto& Factory : FactoryList)
	{
		Factory->InitializeAdapterGlobals();
	}
}
