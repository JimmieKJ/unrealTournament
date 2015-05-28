// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NavMeshRenderingComponent.cpp: A component that renders a nav mesh.
 =============================================================================*/

#include "EnginePrivate.h"
#include "DebugRenderSceneProxy.h"
#include "NavigationOctree.h"
#include "AI/Navigation/RecastHelpers.h"
#include "AI/Navigation/RecastNavMeshGenerator.h"
#include "AI/Navigation/NavigationSystem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#endif

#include "NavMeshRenderingHelpers.h"

UNavMeshRenderingComponent::UNavMeshRenderingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	AlwaysLoadOnClient = false;
	AlwaysLoadOnServer = false;
	bSelectable = false;
	bCollectNavigationData = false;
}

bool UNavMeshRenderingComponent::NeedsLoadForServer() const
{
	// This rendering component is not needed in dedicated server builds
	return false;
}

bool UNavMeshRenderingComponent::IsNavigationShowFlagSet(const UWorld* World)
{
	bool bShowNavigation = false;

	FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(World);

#if WITH_EDITOR
	if (GEditor && WorldContext && WorldContext->WorldType != EWorldType::Game)
	{
		bShowNavigation = WorldContext->GameViewport != nullptr && WorldContext->GameViewport->EngineShowFlags.Navigation;
		if (bShowNavigation == false)
		{
			// we have to check all vieports because we can't to distinguish between SIE and PIE at this point.
			for (FEditorViewportClient* CurrentViewport : GEditor->AllViewportClients)
			{
				if (CurrentViewport && CurrentViewport->IsVisible() && CurrentViewport->EngineShowFlags.Navigation)
				{
					bShowNavigation = true;
					break;
				}
			}
		}
	}
	else
#endif //WITH_EDITOR
	{
		bShowNavigation = WorldContext && WorldContext->GameViewport && WorldContext->GameViewport->EngineShowFlags.Navigation;
	}

	return bShowNavigation;
}

void UNavMeshRenderingComponent::TimerFunction()
{
	const bool bShowNavigation = IsNavigationShowFlagSet(GetWorld());

	if (bShowNavigation != !!bCollectNavigationData)
	{
		bCollectNavigationData = bShowNavigation;
		MarkRenderStateDirty();
	}
}

void UNavMeshRenderingComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	// it's a kind of HACK but there is no event or other information that show flag was changed by user => we have to check it periodically
#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->GetTimerManager()->SetTimer(TimerHandle, FTimerDelegate::CreateUObject(this, &UNavMeshRenderingComponent::TimerFunction), 1, true);
	}
	else
#endif //WITH_EDITOR
	{
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateUObject(this, &UNavMeshRenderingComponent::TimerFunction), 1, true);
	}
#endif //WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
}

void UNavMeshRenderingComponent::OnUnregister()
{
#if WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	// it's a kind of HACK but there is no event or other information that show flag was changed by user => we have to check it periodically
#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->GetTimerManager()->ClearTimer(TimerHandle);
	}
	else
#endif //WITH_EDITOR
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
	}
#endif //WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	Super::OnUnregister();
}

FPrimitiveSceneProxy* UNavMeshRenderingComponent::CreateSceneProxy()
{
#if WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	FPrimitiveSceneProxy* SceneProxy = NULL;

	const bool bShowNavigation = IsNavigationShowFlagSet(GetWorld());

	bCollectNavigationData = bShowNavigation;

	if (bCollectNavigationData && IsVisible())
	{
		FNavMeshSceneProxyData ProxyData;
		ProxyData.Reset();
		GatherData(ProxyData);

		SceneProxy = new FRecastRenderingSceneProxy(this, &ProxyData);
		ProxyData.Reset();
	}
	return SceneProxy;
#else
	return NULL;
#endif //WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
}

void UNavMeshRenderingComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

#if WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (SceneProxy != NULL)
	{
		static_cast<FRecastRenderingSceneProxy*>(SceneProxy)->RegisterDebugDrawDelgate();
	}
#endif
}

void UNavMeshRenderingComponent::DestroyRenderState_Concurrent()
{
#if WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if (SceneProxy != NULL)
	{
		static_cast<FRecastRenderingSceneProxy*>(SceneProxy)->UnregisterDebugDrawDelgate();
	}
#endif

	Super::DestroyRenderState_Concurrent();
}

FBoxSphereBounds UNavMeshRenderingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox BoundingBox(0);
#if WITH_RECAST
	ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(GetOwner());
	if (NavMesh)
	{
		BoundingBox = NavMesh->GetNavMeshBounds();
		if (NavMesh->bDrawOctree)
		{
			const UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
			const FNavigationOctree* NavOctree = NavSys ? NavSys->GetNavOctree() : NULL;
			if (NavOctree)
			{
				for (FNavigationOctree::TConstIterator<> NodeIt(*NavOctree); NodeIt.HasPendingNodes(); NodeIt.Advance())
				{
					const FOctreeNodeContext& CorrentContext = NodeIt.GetCurrentContext();
					BoundingBox += CorrentContext.Bounds.GetBox();
				}
			}
		}
	}
#endif
	return FBoxSphereBounds(BoundingBox);
}

void UNavMeshRenderingComponent::GatherData(FNavMeshSceneProxyData& CurrentData) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_NavMesh_GatherDebugDrawingGeometry);
#if WITH_RECAST
	const ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(GetOwner());

	CurrentData.Reset();
	
	if (NavMesh && NavMesh->IsDrawingEnabled())
	{
		CurrentData.bEnableDrawing = true;
		CurrentData.bNeedsNewData = false;

		FHitProxyId HitProxyId = FHitProxyId();
		CurrentData.bDrawPathCollidingGeometry = NavMesh->bDrawPathCollidingGeometry;

		CurrentData.NavMeshDrawOffset.Z = NavMesh->DrawOffset;
		CurrentData.NavMeshGeometry.bGatherPolyEdges = NavMesh->bDrawPolyEdges;
		CurrentData.NavMeshGeometry.bGatherNavMeshEdges = NavMesh->bDrawNavMeshEdges;

		const FNavDataConfig& NavConfig = NavMesh->GetConfig();
		CurrentData.NavMeshColors[RECAST_DEFAULT_AREA] = NavConfig.Color.DWColor() > 0 ? NavConfig.Color : NavMeshRenderColor_RecastMesh;
		for (uint8 i = 0; i < RECAST_DEFAULT_AREA; ++i)
		{
			CurrentData.NavMeshColors[i] = NavMesh->GetAreaIDColor(i);
		}

		// just a little trick to make sure navmeshes with different sized are not drawn with same offset
		CurrentData.NavMeshDrawOffset.Z += NavMesh->GetConfig().AgentRadius / 10.f;

		NavMesh->BeginBatchQuery();

		if (NavMesh->bDrawOctree)
		{
			const UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
			const FNavigationOctree* NavOctree = NavSys ? NavSys->GetNavOctree() : NULL;
			if (NavOctree)
			{
				for (FNavigationOctree::TConstIterator<> NodeIt(*NavOctree); NodeIt.HasPendingNodes(); NodeIt.Advance())
				{
					const FNavigationOctree::FNode& CurrentNode = NodeIt.GetCurrentNode();
					const FOctreeNodeContext& CorrentContext = NodeIt.GetCurrentContext();
					CurrentData.OctreeBounds.Add(CorrentContext.Bounds);

					FOREACH_OCTREE_CHILD_NODE(ChildRef)
					{
						if (CurrentNode.HasChild(ChildRef))
						{
							NodeIt.PushChild(ChildRef);
						}
					}
				}
			}
		}

		NavMesh->GetDebugGeometry(CurrentData.NavMeshGeometry);
		const TArray<FVector>& MeshVerts = CurrentData.NavMeshGeometry.MeshVerts;

		// @fixme, this is going to double up on lots of interior lines
		if (NavMesh->bDrawTriangleEdges)
		{
			for (int32 AreaIdx = 0; AreaIdx < RECAST_MAX_AREAS; ++AreaIdx)
			{
				const TArray<int32>& MeshIndices = CurrentData.NavMeshGeometry.AreaIndices[AreaIdx];
				for (int32 Idx=0; Idx<MeshIndices.Num(); Idx += 3)
				{
					CurrentData.ThickLineItems.Add(FNavMeshSceneProxyData::FDebugThickLine(MeshVerts[MeshIndices[Idx + 0]] + CurrentData.NavMeshDrawOffset, MeshVerts[MeshIndices[Idx + 1]] + CurrentData.NavMeshDrawOffset, NavMeshRenderColor_Recast_TriangleEdges, DefaultEdges_LineThickness));
					CurrentData.ThickLineItems.Add(FNavMeshSceneProxyData::FDebugThickLine(MeshVerts[MeshIndices[Idx + 1]] + CurrentData.NavMeshDrawOffset, MeshVerts[MeshIndices[Idx + 2]] + CurrentData.NavMeshDrawOffset, NavMeshRenderColor_Recast_TriangleEdges, DefaultEdges_LineThickness));
					CurrentData.ThickLineItems.Add(FNavMeshSceneProxyData::FDebugThickLine(MeshVerts[MeshIndices[Idx + 2]] + CurrentData.NavMeshDrawOffset, MeshVerts[MeshIndices[Idx + 0]] + CurrentData.NavMeshDrawOffset, NavMeshRenderColor_Recast_TriangleEdges, DefaultEdges_LineThickness));
				}
			}
		}

		// make lines for tile edges
		if (NavMesh->bDrawPolyEdges)
		{
			const TArray<FVector>& TileEdgeVerts = CurrentData.NavMeshGeometry.PolyEdges;
			for (int32 Idx=0; Idx < TileEdgeVerts.Num(); Idx += 2)
			{
				CurrentData.TileEdgeLines.Add(FDebugRenderSceneProxy::FDebugLine(TileEdgeVerts[Idx] + CurrentData.NavMeshDrawOffset, TileEdgeVerts[Idx + 1] + CurrentData.NavMeshDrawOffset, NavMeshRenderColor_Recast_TileEdges));
			}
		}

		// make lines for navmesh edges
		if (NavMesh->bDrawNavMeshEdges)
		{
			const FColor EdgesColor = DarkenColor(CurrentData.NavMeshColors[RECAST_DEFAULT_AREA]);
			const TArray<FVector>& NavMeshEdgeVerts = CurrentData.NavMeshGeometry.NavMeshEdges;
			for (int32 Idx=0; Idx < NavMeshEdgeVerts.Num(); Idx += 2)
			{
				CurrentData.NavMeshEdgeLines.Add( FDebugRenderSceneProxy::FDebugLine(NavMeshEdgeVerts[Idx] + CurrentData.NavMeshDrawOffset, NavMeshEdgeVerts[Idx+1] + CurrentData.NavMeshDrawOffset, EdgesColor));
			}
		}

		// offset all navigation-link positions
		if (!NavMesh->bDrawClusters)
		{
			for (int32 OffMeshLineIndex = 0; OffMeshLineIndex < CurrentData.NavMeshGeometry.OffMeshLinks.Num(); ++OffMeshLineIndex)
			{
				FRecastDebugGeometry::FOffMeshLink& Link = CurrentData.NavMeshGeometry.OffMeshLinks[OffMeshLineIndex];
				const bool bLinkValid = (Link.ValidEnds & FRecastDebugGeometry::OMLE_Left) && (Link.ValidEnds & FRecastDebugGeometry::OMLE_Right);

				if (NavMesh->bDrawFailedNavLinks || (NavMesh->bDrawNavLinks && bLinkValid))
				{
					const FVector V0 = Link.Left + CurrentData.NavMeshDrawOffset;
					const FVector V1 = Link.Right + CurrentData.NavMeshDrawOffset;
					const FColor LinkColor = ((Link.Direction && Link.ValidEnds) || (Link.ValidEnds & FRecastDebugGeometry::OMLE_Left)) ? DarkenColor(CurrentData.NavMeshColors[Link.AreaID]) : NavMeshRenderColor_OffMeshConnectionInvalid;

					CacheArc(CurrentData.NavLinkLines, V0, V1, 0.4f, 4, LinkColor, LinkLines_LineThickness);

					const FVector VOffset(0, 0, FVector::Dist(V0, V1) * 1.333f);
					CacheArrowHead(CurrentData.NavLinkLines, V1, V0+VOffset, 30.f, LinkColor, LinkLines_LineThickness);
					if (Link.Direction)
					{
						CacheArrowHead(CurrentData.NavLinkLines, V0, V1+VOffset, 30.f, LinkColor, LinkLines_LineThickness);
					}

					// if the connection as a whole is valid check if there are any of ends is invalid
					if (LinkColor != NavMeshRenderColor_OffMeshConnectionInvalid)
					{
						if (Link.Direction && (Link.ValidEnds & FRecastDebugGeometry::OMLE_Left) == 0)
						{
							// left end invalid - mark it
							DrawWireCylinder(CurrentData.NavLinkLines, V0, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), NavMeshRenderColor_OffMeshConnectionInvalid, Link.Radius, NavMesh->AgentMaxStepHeight, 16, 0, DefaultEdges_LineThickness);
						}
						if ((Link.ValidEnds & FRecastDebugGeometry::OMLE_Right) == 0)
						{
							DrawWireCylinder(CurrentData.NavLinkLines, V1, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), NavMeshRenderColor_OffMeshConnectionInvalid, Link.Radius, NavMesh->AgentMaxStepHeight, 16, 0, DefaultEdges_LineThickness);
						}
					}
				}					
			}
		}

		if (NavMesh->bDrawTileLabels || NavMesh->bDrawPolygonLabels || NavMesh->bDrawDefaultPolygonCost || NavMesh->bDrawTileBounds)
		{
			// calculate appropriate points for displaying debug labels
			const int32 TilesCount = NavMesh->GetNavMeshTilesCount();
			CurrentData.DebugLabels.Reserve(TilesCount);

			for (int32 TileIndex = 0; TileIndex < TilesCount; ++TileIndex)
			{
				int32 X, Y, Layer;
				if (NavMesh->GetNavMeshTileXY(TileIndex, X, Y, Layer))
				{
					const FBox TileBoundingBox = NavMesh->GetNavMeshTileBounds(TileIndex);
					FVector TileLabelLocation = TileBoundingBox.GetCenter();
					TileLabelLocation.Z = TileBoundingBox.Max.Z;

					FNavLocation NavLocation(TileLabelLocation);
					if (!NavMesh->ProjectPoint(TileLabelLocation, NavLocation, FVector(NavMesh->TileSizeUU/100, NavMesh->TileSizeUU/100, TileBoundingBox.Max.Z-TileBoundingBox.Min.Z)))
					{
						NavMesh->ProjectPoint(TileLabelLocation, NavLocation, FVector(NavMesh->TileSizeUU/2, NavMesh->TileSizeUU/2, TileBoundingBox.Max.Z-TileBoundingBox.Min.Z));
					}

					if (NavMesh->bDrawTileLabels)
					{
						CurrentData.DebugLabels.Add(FNavMeshSceneProxyData::FDebugText(
							/*Location*/NavLocation.Location + CurrentData.NavMeshDrawOffset
							, /*Text*/FString::Printf(TEXT("(%d,%d:%d)"), X, Y, Layer)
							));
					}

					if (NavMesh->bDrawPolygonLabels || NavMesh->bDrawDefaultPolygonCost)
					{
						TArray<FNavPoly> Polys;
						NavMesh->GetPolysInTile(TileIndex, Polys);

						if (NavMesh->bDrawDefaultPolygonCost)
						{
							float DefaultCosts[RECAST_MAX_AREAS];
							float FixedCosts[RECAST_MAX_AREAS];

							NavMesh->GetDefaultQueryFilter()->GetAllAreaCosts(DefaultCosts, FixedCosts, RECAST_MAX_AREAS);

							for(int k = 0; k < Polys.Num(); ++k)
							{
								uint32 AreaID = NavMesh->GetPolyAreaID(Polys[k].Ref);

								CurrentData.DebugLabels.Add(FNavMeshSceneProxyData::FDebugText(
									/*Location*/Polys[k].Center + CurrentData.NavMeshDrawOffset
									, /*Text*/FString::Printf(TEXT("\\%.3f; %.3f\\"), DefaultCosts[AreaID], FixedCosts[AreaID])
									));
							}
						}
						else
						{
							for(int k = 0; k < Polys.Num(); ++k)
							{
								uint32 NavPolyIndex = 0;
								uint32 NavTileIndex = 0;
								NavMesh->GetPolyTileIndex(Polys[k].Ref, NavPolyIndex, NavTileIndex);

								CurrentData.DebugLabels.Add(FNavMeshSceneProxyData::FDebugText(
									/*Location*/Polys[k].Center + CurrentData.NavMeshDrawOffset
									, /*Text*/FString::Printf(TEXT("[%X:%X]"), NavTileIndex, NavPolyIndex)
									));
							}
						}
					}							

					if (NavMesh->bDrawTileBounds)
					{
						FBox TileBox = NavMesh->GetNavMeshTileBounds(TileIndex);

						float DrawZ = (TileBox.Min.Z + TileBox.Max.Z) * 0.5f;		// @hack average
						FVector LL(TileBox.Min.X, TileBox.Min.Y, DrawZ);
						FVector UR(TileBox.Max.X, TileBox.Max.Y, DrawZ);
						FVector UL(LL.X, UR.Y, DrawZ);
						FVector LR(UR.X, LL.Y, DrawZ);
						CurrentData.ThickLineItems.Add(FNavMeshSceneProxyData::FDebugThickLine(LL, UL, NavMeshRenderColor_TileBounds, DefaultEdges_LineThickness));
						CurrentData.ThickLineItems.Add(FNavMeshSceneProxyData::FDebugThickLine(UL, UR, NavMeshRenderColor_TileBounds, DefaultEdges_LineThickness));
						CurrentData.ThickLineItems.Add(FNavMeshSceneProxyData::FDebugThickLine(UR, LR, NavMeshRenderColor_TileBounds, DefaultEdges_LineThickness));
						CurrentData.ThickLineItems.Add(FNavMeshSceneProxyData::FDebugThickLine(LR, LL, NavMeshRenderColor_TileBounds, DefaultEdges_LineThickness));
					}
				}
			}
		}

		CurrentData.bSkipDistanceCheck = GIsEditor && (GEngine->GetDebugLocalPlayer() == NULL);
		CurrentData.bDrawClusters = NavMesh->bDrawClusters;
		NavMesh->FinishBatchQuery();

		// Draw Mesh
		if (NavMesh->bDrawClusters)
		{
			for (int32 Idx = 0; Idx < CurrentData.NavMeshGeometry.Clusters.Num(); ++Idx)
			{
				const TArray<int32>& MeshIndices = CurrentData.NavMeshGeometry.Clusters[Idx].MeshIndices;

				if (MeshIndices.Num() == 0)
				{
					continue;
				}

				FNavMeshSceneProxyData::FDebugMeshData DebugMeshData;
				DebugMeshData.ClusterColor = GetClusterColor(Idx);
				for (int32 VertIdx=0; VertIdx < MeshVerts.Num(); ++VertIdx)
				{
					AddVertexHelper(DebugMeshData, MeshVerts[VertIdx] + CurrentData.NavMeshDrawOffset, DebugMeshData.ClusterColor);
				}
				for (int32 TriIdx=0; TriIdx < MeshIndices.Num(); TriIdx+=3)
				{
					AddTriangleHelper(DebugMeshData, MeshIndices[TriIdx], MeshIndices[TriIdx+1], MeshIndices[TriIdx+2]);
				}

				CurrentData.MeshBuilders.Add(DebugMeshData);
			}
		}
		else if (NavMesh->bDrawNavMesh)
		{			
			for (int32 AreaType = 0; AreaType < RECAST_MAX_AREAS; ++AreaType)
			{
				const TArray<int32>& MeshIndices = CurrentData.NavMeshGeometry.AreaIndices[AreaType];

				if (MeshIndices.Num() == 0)
				{
					continue;
				}

				FNavMeshSceneProxyData::FDebugMeshData DebugMeshData;
				for (int32 VertIdx=0; VertIdx < MeshVerts.Num(); ++VertIdx)
				{
					AddVertexHelper(DebugMeshData, MeshVerts[VertIdx] + CurrentData.NavMeshDrawOffset, CurrentData.NavMeshColors[AreaType]);
				}
				for (int32 TriIdx=0; TriIdx < MeshIndices.Num(); TriIdx+=3)
				{
					AddTriangleHelper(DebugMeshData, MeshIndices[TriIdx], MeshIndices[TriIdx+1], MeshIndices[TriIdx+2]);
				}

				DebugMeshData.ClusterColor = CurrentData.NavMeshColors[AreaType];
				CurrentData.MeshBuilders.Add(DebugMeshData);
			}
		}

		if (NavMesh->bDrawPathCollidingGeometry)
		{
			// draw all geometry gathered in navoctree
			const FNavigationOctree* NavOctree = NavMesh->GetWorld()->GetNavigationSystem()->GetNavOctree();

			if (NavOctree)
			{
				TArray<FVector> PathCollidingGeomVerts;
				TArray <int32> PathCollidingGeomIndices;
				for (FNavigationOctree::TConstIterator<> It(*NavOctree); It.HasPendingNodes(); It.Advance())
				{
					const FNavigationOctree::FNode& Node = It.GetCurrentNode();
					for (FNavigationOctree::ElementConstIt ElementIt(Node.GetElementIt()); ElementIt; ElementIt++)
					{
						const FNavigationOctreeElement& Element = *ElementIt;
						if (Element.ShouldUseGeometry(NavMesh->GetConfig()) && Element.Data->CollisionData.Num())
						{
							const FRecastGeometryCache CachedGeometry(Element.Data->CollisionData.GetData());
							AppendGeometry(PathCollidingGeomVerts, PathCollidingGeomIndices, CachedGeometry.Verts, CachedGeometry.Header.NumVerts, CachedGeometry.Indices, CachedGeometry.Header.NumFaces);
						}
					}
					FOREACH_OCTREE_CHILD_NODE(ChildRef)
					{
						if (Node.HasChild(ChildRef))
						{
							It.PushChild(ChildRef);
						}
					}
				}
				CurrentData.PathCollidingGeomIndices = PathCollidingGeomIndices;
				for (const auto& Vertex : PathCollidingGeomVerts)
				{
					CurrentData.PathCollidingGeomVerts.Add(FDynamicMeshVertex(Vertex));
				}
			}
		}

		if (CurrentData.NavMeshGeometry.BuiltMeshIndices.Num() > 0)
		{
			FNavMeshSceneProxyData::FDebugMeshData DebugMeshData;
			for (int32 VertIdx=0; VertIdx < MeshVerts.Num(); ++VertIdx)
			{
				AddVertexHelper(DebugMeshData, MeshVerts[VertIdx] + CurrentData.NavMeshDrawOffset, NavMeshRenderColor_RecastTileBeingRebuilt);
			}
			DebugMeshData.Indices.Append(CurrentData.NavMeshGeometry.BuiltMeshIndices);
			DebugMeshData.ClusterColor = NavMeshRenderColor_RecastTileBeingRebuilt;
			CurrentData.MeshBuilders.Add(DebugMeshData);

			// updates should be requested by FRecastNavMeshGenerator::TickAsyncBuild after tiles were refreshed
		}

		if (NavMesh->bDrawClusters)
		{
			for (int i = 0; i < CurrentData.NavMeshGeometry.ClusterLinks.Num(); i++)
			{
				const FRecastDebugGeometry::FClusterLink& CLink = CurrentData.NavMeshGeometry.ClusterLinks[i];
				const FVector V0 = CLink.FromCluster + CurrentData.NavMeshDrawOffset;
				const FVector V1 = CLink.ToCluster + CurrentData.NavMeshDrawOffset + FVector(0,0,20.0f);

				CacheArc(CurrentData.ClusterLinkLines, V0, V1, 0.4f, 4, FColor::Black, ClusterLinkLines_LineThickness);
				const FVector VOffset(0, 0, FVector::Dist(V0, V1) * 1.333f);
				CacheArrowHead(CurrentData.ClusterLinkLines, V1, V0+VOffset, 30.f, FColor::Black, ClusterLinkLines_LineThickness);
			}
		}

		// cache segment links
		if (NavMesh->bDrawNavLinks)
		{
			for (int32 iArea = 0; iArea < RECAST_MAX_AREAS; iArea++)
			{
				const TArray<int32>& Indices = CurrentData.NavMeshGeometry.OffMeshSegmentAreas[iArea];
				FNavMeshSceneProxyData::FDebugMeshData DebugMeshData;
				int32 VertBase = 0;

				for (int32 i = 0; i < Indices.Num(); i++)
				{
					FRecastDebugGeometry::FOffMeshSegment& SegInfo = CurrentData.NavMeshGeometry.OffMeshSegments[Indices[i]];
					const FVector A0 = SegInfo.LeftStart + CurrentData.NavMeshDrawOffset;
					const FVector A1 = SegInfo.LeftEnd + CurrentData.NavMeshDrawOffset;
					const FVector B0 = SegInfo.RightStart + CurrentData.NavMeshDrawOffset;
					const FVector B1 = SegInfo.RightEnd + CurrentData.NavMeshDrawOffset;
					const FVector Edge0 = B0 - A0;
					const FVector Edge1 = B1 - A1;
					const float Len0 = Edge0.Size();
					const float Len1 = Edge1.Size();
					const FColor SegColor = DarkenColor(CurrentData.NavMeshColors[SegInfo.AreaID]);
					const FColor ColA = (SegInfo.ValidEnds & FRecastDebugGeometry::OMLE_Left) ? FColor::White : FColor::Black;
					const FColor ColB = (SegInfo.ValidEnds & FRecastDebugGeometry::OMLE_Right) ? FColor::White : FColor::Black;

					const int32 NumArcPoints = 8;
					const float ArcPtsScale = 1.0f / NumArcPoints;

					FVector Prev0 = EvalArc(A0, Edge0, Len0*0.25f, 0);
					FVector Prev1 = EvalArc(A1, Edge1, Len1*0.25f, 0);
					AddVertexHelper(DebugMeshData, Prev0, ColA);
					AddVertexHelper(DebugMeshData, Prev1, ColA);
					for (int32 j = 1; j <= NumArcPoints; j++)
					{
						const float u = j * ArcPtsScale;
						FVector Pt0 = EvalArc(A0, Edge0, Len0*0.25f, u);
						FVector Pt1 = EvalArc(A1, Edge1, Len1*0.25f, u);

						AddVertexHelper(DebugMeshData, Pt0, (j == NumArcPoints) ? ColB : FColor::White);
						AddVertexHelper(DebugMeshData, Pt1, (j == NumArcPoints) ? ColB : FColor::White);

						AddTriangleHelper(DebugMeshData, VertBase+0, VertBase+2, VertBase+1);
						AddTriangleHelper(DebugMeshData, VertBase+2, VertBase+3, VertBase+1);
						AddTriangleHelper(DebugMeshData, VertBase+0, VertBase+1, VertBase+2);
						AddTriangleHelper(DebugMeshData, VertBase+2, VertBase+1, VertBase+3);

						VertBase += 2;
						Prev0 = Pt0;
						Prev1 = Pt1;
					}
					VertBase += 2;

					DebugMeshData.ClusterColor = SegColor;
				}

				if (DebugMeshData.Indices.Num())
				{
					CurrentData.MeshBuilders.Add(DebugMeshData);
				}
			}
		}

		CurrentData.NavMeshGeometry.PolyEdges.Empty();
		CurrentData.NavMeshGeometry.NavMeshEdges.Empty();
	}
#endif //WITH_RECAST
}
