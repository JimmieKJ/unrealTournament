// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "SplineMeshSceneProxy.h"

FSplineMeshSceneProxy::FSplineMeshSceneProxy(USplineMeshComponent* InComponent) :
	FStaticMeshSceneProxy(InComponent)
{
	bSupportsDistanceFieldRepresentation = false;

	// make sure all the materials are okay to be rendered as a spline mesh
	for (FStaticMeshSceneProxy::FLODInfo& LODInfo : LODs)
	{
		for (FStaticMeshSceneProxy::FLODInfo::FSectionInfo& Section : LODInfo.Sections)
		{
			if (!Section.Material->CheckMaterialUsage(MATUSAGE_SplineMesh))
			{
				Section.Material = UMaterial::GetDefaultMaterial(MD_Surface);
			}
		}
	}

	// Copy spline params from component
	SplineParams = InComponent->SplineParams;
	SplineUpDir = InComponent->SplineUpDir;
	bSmoothInterpRollScale = InComponent->bSmoothInterpRollScale;
	ForwardAxis = InComponent->ForwardAxis;

	// Fill in info about the mesh
	if (FMath::IsNearlyEqual(InComponent->SplineBoundaryMin, InComponent->SplineBoundaryMax))
	{
		FBoxSphereBounds StaticMeshBounds = StaticMesh->GetBounds();
		SplineMeshScaleZ = 0.5f / USplineMeshComponent::GetAxisValue(StaticMeshBounds.BoxExtent, ForwardAxis); // 1/(2 * Extent)
		SplineMeshMinZ = USplineMeshComponent::GetAxisValue(StaticMeshBounds.Origin, ForwardAxis) * SplineMeshScaleZ - 0.5f;
	}
	else
	{
		SplineMeshScaleZ = 1.0f / (InComponent->SplineBoundaryMax - InComponent->SplineBoundaryMin);
		SplineMeshMinZ = InComponent->SplineBoundaryMin * SplineMeshScaleZ;
	}

	LODResources.Reset(InComponent->StaticMesh->RenderData->LODResources.Num());

	for (int32 LODIndex = 0; LODIndex < LODs.Num(); LODIndex++)
	{
		FSplineMeshVertexFactory* VertexFactory = new FSplineMeshVertexFactory(this);

		LODResources.Add(VertexFactory);

		InitResources(InComponent, LODIndex);
	}
}

FSplineMeshSceneProxy::~FSplineMeshSceneProxy()
{
	ReleaseResources();

	for (FSplineMeshSceneProxy::FLODResources& LODResource : LODResources)
	{
		delete LODResource.VertexFactory;
	}
	LODResources.Empty();
}

bool FSplineMeshSceneProxy::GetShadowMeshElement(int32 LODIndex, int32 BatchIndex, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch) const
{
	//checkf(LODIndex == 0, TEXT("Getting spline static mesh element with invalid LOD [%d]"), LODIndex);

	if (FStaticMeshSceneProxy::GetShadowMeshElement(LODIndex, BatchIndex, InDepthPriorityGroup, OutMeshBatch))
	{
		OutMeshBatch.VertexFactory = LODResources[LODIndex].VertexFactory;
		OutMeshBatch.ReverseCulling ^= (SplineParams.StartScale.X < 0) ^ (SplineParams.StartScale.Y < 0);
		return true;
	}
	return false;
}

bool FSplineMeshSceneProxy::GetMeshElement(int32 LODIndex, int32 BatchIndex, int32 SectionIndex, uint8 InDepthPriorityGroup, const bool bUseSelectedMaterial, const bool bUseHoveredMaterial, FMeshBatch& OutMeshBatch) const
{
	//checkf(LODIndex == 0 /*&& SectionIndex == 0*/, TEXT("Getting spline static mesh element with invalid params [%d, %d]"), LODIndex, SectionIndex);

	if (FStaticMeshSceneProxy::GetMeshElement(LODIndex, BatchIndex, SectionIndex, InDepthPriorityGroup, bUseSelectedMaterial, bUseHoveredMaterial, OutMeshBatch))
	{
		OutMeshBatch.VertexFactory = LODResources[LODIndex].VertexFactory;
		OutMeshBatch.ReverseCulling ^= (SplineParams.StartScale.X < 0) ^ (SplineParams.StartScale.Y < 0);
		return true;
	}
	return false;
}

bool FSplineMeshSceneProxy::GetWireframeMeshElement(int32 LODIndex, int32 BatchIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch) const
{
	//checkf(LODIndex == 0, TEXT("Getting spline static mesh element with invalid LOD [%d]"), LODIndex);

	if (FStaticMeshSceneProxy::GetWireframeMeshElement(LODIndex, BatchIndex, WireframeRenderProxy, InDepthPriorityGroup, OutMeshBatch))
	{
		OutMeshBatch.VertexFactory = LODResources[LODIndex].VertexFactory;
		OutMeshBatch.ReverseCulling ^= (SplineParams.StartScale.X < 0) ^ (SplineParams.StartScale.Y < 0);
		return true;
	}
	return false;
}