// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTNavGraphRenderingComponent.h"
#include "UTRecastNavMesh.h"
#include "Runtime/Engine/Private/AI/Navigation/PImplRecastNavMesh.h"
#include "Runtime/Navmesh/Public/Detour/DetourNavMesh.h"
#include "Runtime/Engine/Private/AI/Navigation/RecastHelpers.h"

UUTNavGraphRenderingComponent::UUTNavGraphRenderingComponent(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	AlwaysLoadOnClient = false;
	AlwaysLoadOnServer = false;
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
: Location(FVector::ZeroVector), PolyColor(FColor::MakeRandomColor())
{
	const dtNavMesh* InternalMesh = NavData->GetRecastNavMeshImpl()->GetRecastMesh();

	if (RealNode->Polys.Num() > 0)
	{
		Location = RealNode->Location;
		for (NavNodeRef PolyRef : RealNode->Polys)
		{
			PolyCenters.Add(NavData->GetPolyCenter(PolyRef));
		}
	}
	for (TWeakObjectPtr<AActor> POI : RealNode->POIs)
	{
		POILocations.Add(POI->GetActorLocation());
	}
	for (const FUTPathLink& Link : RealNode->Paths)
	{
		new(Paths) FUTPathLinkRenderProxy(Link, NavData);
	}
}

FUTPathLinkRenderProxy::FUTPathLinkRenderProxy(const FUTPathLink& RealLink, const AUTRecastNavMesh* NavData)
: EndLocation(FVector::ZeroVector), CollisionRadius(RealLink.CollisionRadius), CollisionHeight(RealLink.CollisionHeight), PathColor(RealLink.GetPathColor())
{
	if (RealLink.End != NULL)
	{
		EndLocation = RealLink.End->Location;
	}
}

FNavGraphSceneProxy::FNavGraphSceneProxy(UUTNavGraphRenderingComponent* InComponent)
: FDebugRenderSceneProxy(InComponent), bDrawPolyEdges(false)
{
	AUTRecastNavMesh* NavData = Cast<AUTRecastNavMesh>(InComponent->GetOwner());
	if (NavData != NULL)
	{
		bDrawPolyEdges = NavData->bDrawPolyEdges;
		for (const UUTPathNode* Node : NavData->GetAllNodes())
		{
			FUTPathNodeRenderProxy* Proxy = new(PathNodes) FUTPathNodeRenderProxy(Node, NavData);
		}
	}
}

void FNavGraphSceneProxy::DrawDynamicElements(FPrimitiveDrawInterface* PDI, const FSceneView* View)
{
	for (const FUTPathNodeRenderProxy& Node : PathNodes)
	{
		PDI->DrawPoint(Node.Location + FVector(0.0f, 0.0f, 32.0f), bDrawPolyEdges ? Node.PolyColor : FLinearColor(0.0f, 1.0f, 0.0f), 32.0f, 0);
		for (const FUTPathLinkRenderProxy& Link : Node.Paths)
		{
			// TODO: color based on collision size?
			PDI->DrawLine(Node.Location, Link.EndLocation, Link.PathColor, 0, 5.0f);
		}
		if (bDrawPolyEdges)
		{
			for (const FVector& PolyLoc : Node.PolyCenters)
			{
				PDI->DrawPoint(PolyLoc + FVector(0.0f, 0.0f, 12.0f), Node.PolyColor, 12.0f, 0);
			}
		}
	}
	FDebugRenderSceneProxy::DrawDynamicElements(PDI, View);
}

#endif