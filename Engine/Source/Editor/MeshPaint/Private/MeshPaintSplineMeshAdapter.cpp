// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MeshPaintSplineMeshAdapter.h"

#include "StaticMeshResources.h"
#include "Components/SplineMeshComponent.h"

//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForSplineMeshes

void FMeshPaintGeometryAdapterForSplineMeshes::InitializeMeshVertices()
{
	// Cache deformed spline mesh vertices for quick lookup during painting / previewing
	USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>(StaticMeshComponent);
	check(SplineMeshComponent);

	const int32 NumVertices = LODModel->PositionVertexBuffer.GetNumVertices();
	MeshVertices.Reset();
	MeshVertices.AddDefaulted(NumVertices);
	for (int32 Index = 0; Index < NumVertices; Index++)
	{
		FVector Position = LODModel->PositionVertexBuffer.VertexPosition(Index);
		const FTransform SliceTransform = SplineMeshComponent->CalcSliceTransform(USplineMeshComponent::GetAxisValue(Position, SplineMeshComponent->ForwardAxis));
		USplineMeshComponent::GetAxisValue(Position, SplineMeshComponent->ForwardAxis) = 0;
		MeshVertices[Index] = SliceTransform.TransformPosition(Position);
	}
}


//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForSplineMeshesFactory

TSharedPtr<IMeshPaintGeometryAdapter> FMeshPaintGeometryAdapterForSplineMeshesFactory::Construct(class UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex) const
{
	if (USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>(InComponent))
	{
		if (SplineMeshComponent->GetStaticMesh() != nullptr)
		{
			TSharedRef<FMeshPaintGeometryAdapterForSplineMeshes> Result = MakeShareable(new FMeshPaintGeometryAdapterForSplineMeshes());
			if (Result->Construct(InComponent, InPaintingMeshLODIndex, InUVChannelIndex))
			{
				return Result;
			}
		}
	}

	return nullptr;
}
