// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTNavMeshRenderingComponent.h"
#include "Private/NavMeshRenderingHelpers.h"

FPrimitiveSceneProxy* UUTNavMeshRenderingComponent::CreateSceneProxy()
{
#if WITH_RECAST && WITH_EDITOR
	FPrimitiveSceneProxy* SceneProxy = NULL;
	ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(GetOwner());
	if (IsVisible() && NavMesh != NULL)
	{
		FNavMeshSceneProxyData ProxyData;
		ProxyData.Reset();
		// use our own handling of NavMesh->bDrawNavMesh
		{
			bool bSavedDrawNavMesh = NavMesh->bDrawNavMesh;
			NavMesh->bDrawNavMesh = false;
			GatherData(ProxyData);
			NavMesh->bDrawNavMesh = bSavedDrawNavMesh;
		}
		if (NavMesh->bDrawNavMesh)
		{
			GatherTriangleData(&ProxyData);
		}

		SceneProxy = new FRecastRenderingSceneProxy(this, &ProxyData);
		ProxyData.Reset();
	}
	return SceneProxy;
#else
	return NULL;
#endif
}

void UUTNavMeshRenderingComponent::GatherTriangleData(FNavMeshSceneProxyData* CurrentData) const
{
#if WITH_EDITOR
	AUTRecastNavMesh* NavData = Cast<AUTRecastNavMesh>(GetOwner());
	NavData->BeginBatchQuery();

	TMap<const UUTPathNode*, FNavMeshTriangleList> TriangleMap;
	NavData->GetNodeTriangleMap(TriangleMap);
	// the vertices all go in one buffer in the scene proxy, so we need to keep track of the total verts added
	int32 CurrentBaseIndex = 0;
	for (TMap<const UUTPathNode*, FNavMeshTriangleList>::TConstIterator It(TriangleMap); It; ++It)
	{
		FColor NodeColor = It.Key()->DebugDrawColor;
		const FNavMeshTriangleList& TriangleData = It.Value();

		FNavMeshSceneProxyData::FDebugMeshData DebugMeshData;
		for (const FVector& Vert : TriangleData.Verts)
		{
			AddVertexHelper(DebugMeshData, Vert + CurrentData->NavMeshDrawOffset, NodeColor);
		}
		for (const FNavMeshTriangleList::FTriangle& Tri : TriangleData.Triangles)
		{
			AddTriangleHelper(DebugMeshData, Tri.Indices[0] + CurrentBaseIndex, Tri.Indices[1] + CurrentBaseIndex, Tri.Indices[2] + CurrentBaseIndex);
		}
		DebugMeshData.ClusterColor = NodeColor;
		CurrentData->MeshBuilders.Add(DebugMeshData);
		CurrentBaseIndex += TriangleData.Verts.Num();
	}
	
	NavData->FinishBatchQuery();
#endif
}