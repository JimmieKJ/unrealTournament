// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MeshPaintPrivatePCH.h"
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
