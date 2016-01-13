// renders the pathnode layer on top of the navmesh (intended for editor use)
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PrimitiveSceneProxy.h"
#include "DebugRenderSceneProxy.h"
#include "UTRecastNavMesh.h"

#include "UTNavGraphRenderingComponent.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTNavGraphRenderingComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
};

struct UNREALTOURNAMENT_API FUTPathLinkRenderProxy
{
	/** poly center of connecting poly in Start PathNode */
	FVector StartLocation;
	/** poly center of connecting poly in End PathNode */
	FVector EndLocation;
	int32 CollisionRadius;
	int32 CollisionHeight;
	FLinearColor PathColor;
	uint16 ReachFlags;
	/** optional ReachSpec that can render the path differently
	 * TODO: is this safe?
	 */
	TWeakObjectPtr<UUTReachSpec> Spec;

	FUTPathLinkRenderProxy(const FUTPathLink& RealLink, const AUTRecastNavMesh* NavData);
};
struct UNREALTOURNAMENT_API FUTPathNodeRenderProxy
{
	/** center of each polygon that makes up the node's area */
	TArray<FVector> PolyCenters;
	/** location of POIs */
	TArray<FVector> POILocations;
	/** links to other paths */
	TArray<FUTPathLinkRenderProxy> Paths;
	/** drawing color for poly centers */
	FLinearColor PolyColor;

	FUTPathNodeRenderProxy(const UUTPathNode* RealNode, const AUTRecastNavMesh* NavData);
};


#if !UE_SERVER

class UNREALTOURNAMENT_API FNavGraphSceneProxy : public FDebugRenderSceneProxy
{
public:
	FNavGraphSceneProxy(UUTNavGraphRenderingComponent* InComponent);

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const
	{
		const bool bVisible = View->Family->EngineShowFlags.Navigation != 0;
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = bVisible && IsShown(View);
		Result.bDynamicRelevance = true;
		Result.bNormalTranslucencyRelevance = bVisible && IsShown(View);
		return Result;
	}

	virtual uint32 GetMemoryFootprint() const
	{
		uint32 Size = sizeof(*this);
		Size += sizeof(FUTPathNodeRenderProxy) * PathNodes.Num();
		for (const FUTPathNodeRenderProxy& Node : PathNodes)
		{
			Size += sizeof(FVector) * Node.PolyCenters.Num();
			Size += sizeof(FVector) * Node.POILocations.Num();
			Size += sizeof(FUTPathLinkRenderProxy) * Node.Paths.Num();
		}
		return Size;
	}
protected:
	/** mirror of nodes from owner */
	TArray<FUTPathNodeRenderProxy> PathNodes;
	/** whether to draw individual polygon information */
	bool bDrawPolyEdges;
	/** path drawing flags copied from AUTRecastNavMesh */
	bool bDrawWalkPaths;
	bool bDrawStandardJumpPaths;
	bool bDrawSpecialPaths;
};

#endif