// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTNavMeshRenderingComponent.h"

FPrimitiveSceneProxy* UUTNavMeshRenderingComponent::CreateSceneProxy()
{
#if WITH_RECAST && WITH_EDITOR
	FNavMeshSceneProxy* NewSceneProxy = NULL;
	ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(GetOwner());
	if (IsVisible() && NavMesh != NULL && !NavMesh->IsPendingKillPending())
	{
		FNavMeshSceneProxyData ProxyData;
		ProxyData.Reset();
		// use our own handling of NavMesh->bDrawNavMesh
		{
			bool bSavedDrawNavMesh = NavMesh->bDrawFilledPolys;
			NavMesh->bDrawFilledPolys = false;

			const int32 DetailFlags = ProxyData.GetDetailFlags(NavMesh);
			TArray<int32> EmptyTileSet;
			ProxyData.GatherData(NavMesh, DetailFlags, EmptyTileSet);

			NavMesh->bDrawFilledPolys = bSavedDrawNavMesh;
		}
		if (NavMesh->bDrawFilledPolys)
		{
			GatherTriangleData(&ProxyData);
		}

		NewSceneProxy = new FNavMeshSceneProxy(this, &ProxyData);
		ProxyData.Reset();
#if WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		if (NewSceneProxy)
		{
			NavMeshDebugDrawDelgateManager.InitDelegateHelper(NewSceneProxy);
			NavMeshDebugDrawDelgateManager.ReregisterDebugDrawDelgate();
		}
#endif
	}
	return NewSceneProxy;
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
		FColor NodeColor = It.Key()->DebugDrawColor.ToFColor(false);
		const FNavMeshTriangleList& TriangleData = It.Value();

		FNavMeshSceneProxyData::FDebugMeshData DebugMeshData;
		for (const FVector& Vert : TriangleData.Verts)
		{
			FNavMeshRenderingHelpers::AddVertex(DebugMeshData, Vert + CurrentData->NavMeshDrawOffset, NodeColor);
		}
		for (const FNavMeshTriangleList::FTriangle& Tri : TriangleData.Triangles)
		{
			FNavMeshRenderingHelpers::AddTriangle(DebugMeshData, Tri.Indices[0] + CurrentBaseIndex, Tri.Indices[1] + CurrentBaseIndex, Tri.Indices[2] + CurrentBaseIndex);
		}
		DebugMeshData.ClusterColor = NodeColor;
		CurrentData->MeshBuilders.Add(DebugMeshData);
		CurrentBaseIndex += TriangleData.Verts.Num();
	}
	
	NavData->FinishBatchQuery();
#endif
}