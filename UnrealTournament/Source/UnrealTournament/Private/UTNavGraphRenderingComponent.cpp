// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTNavGraphRenderingComponent.h"
#include "UTRecastNavMesh.h"
#include "Runtime/Engine/Public/AI/Navigation/PImplRecastNavMesh.h"
#include "Runtime/Navmesh/Public/Detour/DetourNavMesh.h"
#include "Runtime/Engine/Public/AI/Navigation/RecastHelpers.h"

UUTNavGraphRenderingComponent::UUTNavGraphRenderingComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	bIsEditorOnly = true;
	bSelectable = false;
}

FPrimitiveSceneProxy* UUTNavGraphRenderingComponent::CreateSceneProxy()
{
#if !UE_SERVER
	return new FNavGraphSceneProxy(this);
#else
	return nullptr;
#endif
}

FBoxSphereBounds UUTNavGraphRenderingComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	FBox BoundingBox(0);
	AUTRecastNavMesh* NavMesh = Cast<AUTRecastNavMesh>(GetOwner());
	if (NavMesh != NULL)
	{
		BoundingBox = NavMesh->GetNavMeshBounds();
	}
	return FBoxSphereBounds(BoundingBox);
}

#if !UE_SERVER

FUTPathNodeRenderProxy::FUTPathNodeRenderProxy(const UUTPathNode* RealNode, const AUTRecastNavMesh* NavData)
: 
#if WITH_EDITORONLY_DATA
PolyColor(RealNode->DebugDrawColor)
#else
PolyColor(FColor::MakeRandomColor())
#endif
{
	const dtNavMesh* InternalMesh = NavData->GetRecastNavMeshImpl()->GetRecastMesh();

	if (RealNode->Polys.Num() > 0)
	{
		for (NavNodeRef PolyRef : RealNode->Polys)
		{
			PolyCenters.Add(NavData->GetPolySurfaceCenter(PolyRef));
		}
	}
	for (TWeakObjectPtr<AActor> POI : RealNode->POIs)
	{
		if (POI.IsValid())
		{
			POILocations.Add(POI->GetActorLocation());
		}
	}
	for (const FUTPathLink& Link : RealNode->Paths)
	{
		new(Paths) FUTPathLinkRenderProxy(Link, NavData);
	}
}

FUTPathLinkRenderProxy::FUTPathLinkRenderProxy(const FUTPathLink& RealLink, const AUTRecastNavMesh* NavData)
: StartLocation(NavData->GetPolySurfaceCenter(RealLink.StartEdgePoly)), EndLocation(NavData->GetPolySurfaceCenter(RealLink.EndPoly)), CollisionRadius(RealLink.CollisionRadius), CollisionHeight(RealLink.CollisionHeight), PathColor(RealLink.GetPathColor()),
	ReachFlags(RealLink.ReachFlags), Spec(RealLink.Spec)
{
}

FNavGraphSceneProxy::FNavGraphSceneProxy(UUTNavGraphRenderingComponent* InComponent)
: FDebugRenderSceneProxy(InComponent), bDrawPolyEdges(false)
{
	AUTRecastNavMesh* NavData = Cast<AUTRecastNavMesh>(InComponent->GetOwner());
	if (NavData != NULL)
	{
		bDrawPolyEdges = NavData->bDrawPolyEdges;
		bDrawWalkPaths = NavData->bDrawWalkPaths;
		bDrawStandardJumpPaths = NavData->bDrawStandardJumpPaths;
		bDrawSpecialPaths = NavData->bDrawSpecialPaths;
		for (const UUTPathNode* Node : NavData->GetAllNodes())
		{
			FUTPathNodeRenderProxy* Proxy = new(PathNodes) FUTPathNodeRenderProxy(Node, NavData);
		}
	}
}


void FNavGraphSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	const float NodePointSize = 32.0f;
	const float PolyPointSize = 12.0f;
	const FVector PolyPointOffset(0.0f, 0.0f, 15.0f);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

			for (const FUTPathNodeRenderProxy& Node : PathNodes)
			{
				for (const FUTPathLinkRenderProxy& Link : Node.Paths)
				{
					bool bDraw = true;
					if (!Link.Spec.IsValid())
					{
						bDraw = (Link.ReachFlags & R_JUMP) ? bDrawStandardJumpPaths : bDrawWalkPaths;
					}
					else
					{
						bDraw = bDrawSpecialPaths;
					}
					if (bDraw)
					{
						// push line off start and end so that the line color doesn't cover up the poly point color (if enabled)
						const FVector PathOffset = (Link.EndLocation - Link.StartLocation).GetSafeNormal() * (PolyPointSize * 0.5f);
						PDI->DrawLine(Link.StartLocation + PolyPointOffset + PathOffset + FVector(0.0f, 0.0f, 25.0f), Link.EndLocation + PolyPointOffset - PathOffset, Link.PathColor, 0, 5.0f);
					}
				}
				if (bDrawPolyEdges)
				{
					for (const FVector& PolyLoc : Node.PolyCenters)
					{
						PDI->DrawPoint(PolyLoc + PolyPointOffset, Node.PolyColor, PolyPointSize, 0);
					}
				}
			}
		}
	}
}

#endif