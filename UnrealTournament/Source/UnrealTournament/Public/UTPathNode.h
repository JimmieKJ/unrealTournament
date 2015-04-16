// Nodes overlaying the navmesh to provide richer information about points of interest and advanced reachability (e.g. superjump links)
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AI/Navigation/NavigationTypes.h"

#include "UTPathNode.generated.h"

#define BLOCKED_PATH_COST 10000000

enum EReachFlags
{
	R_JUMP = 0x01,
};

USTRUCT(BlueprintType)
struct FCapsuleSize
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Capsule)
	int32 Radius;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Capsule)
	int32 Height;

	FCapsuleSize() = default;
	FCapsuleSize(int32 InRadius, int32 InHeight)
		: Radius(InRadius), Height(InHeight)
	{}
	FCapsuleSize(const FCapsuleSize& Other) = default;

	bool operator== (const FCapsuleSize& Other) const
	{
		return Radius == Other.Radius && Height == Other.Height;
	}

	FVector GetExtent() const
	{
		return FVector(Radius, Radius, Height);
	}
};

/** holds a position optionally relative to a component
* unfortunately FBasedPosition only works with RootComponent so it's not useful
*/
USTRUCT(BlueprintType)
struct FComponentBasedPosition
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasedPosition)
	TWeakObjectPtr<USceneComponent> Base;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = BasedPosition)
		FVector Position;

	mutable FVector CachedBaseLocation;
	mutable FRotator CachedBaseRotation;
	mutable FVector CachedTransPosition;

	FComponentBasedPosition()
		: Base(NULL), Position(ForceInit), CachedBaseLocation(ForceInit), CachedBaseRotation(ForceInit), CachedTransPosition(ForceInit)
	{}
	explicit FComponentBasedPosition(TWeakObjectPtr<USceneComponent> InBase, const FVector& InPosition)
	{
		Set(InBase, InPosition);
	}
	explicit FComponentBasedPosition(const FVector& InPosition)
	{
		Set(NULL, InPosition);
	}

	// Retrieve world location of this position
	FVector Get() const
	{
		if (Base.IsValid())
		{
			const FVector BaseLocation = Base->GetComponentLocation();
			const FRotator BaseRotation = Base->GetComponentRotation();

			// If base hasn't changed location/rotation use cached transformed position
			if (CachedBaseLocation != BaseLocation || CachedBaseRotation != BaseRotation)
			{
				CachedBaseLocation = BaseLocation;
				CachedBaseRotation = BaseRotation;
				CachedTransPosition = BaseLocation + FRotationMatrix(BaseRotation).TransformPosition(Position);
			}

			return CachedTransPosition;
		}
		else if (Base.IsStale())
		{
			return CachedTransPosition;
		}
		else
		{
			return Position;
		}
	}
	void Set(TWeakObjectPtr<USceneComponent> InBase, const FVector& InPosition)
	{
		if (InPosition.IsNearlyZero())
		{
			Base = NULL;
			Position = FVector::ZeroVector;
		}
		else
		{
			Base = (InBase.IsValid() && InBase->Mobility != EComponentMobility::Static) ? InBase : NULL;
			if (Base != NULL)
			{
				const FVector BaseLocation = Base->GetComponentLocation();
				const FRotator BaseRotation = Base->GetComponentRotation();

				CachedBaseLocation = BaseLocation;
				CachedBaseRotation = BaseRotation;
				CachedTransPosition = InPosition;
				Position = FTransform(BaseRotation).InverseTransformPosition(InPosition - BaseLocation);
			}
			else
			{
				Position = InPosition;
			}
		}
	}
	void Clear()
	{
		Base = NULL;
		Position = FVector::ZeroVector;
	}
};

static_assert(NavNodeRef(-1) == uint64(-1) && sizeof(NavNodeRef) == sizeof(uint64), "expecting NavNodeRef to be uint64");

class UUTPathNode;

USTRUCT(BlueprintType)
struct FUTPathLink
{
	GENERATED_USTRUCT_BODY()

	/** source path node */
	UPROPERTY(BlueprintReadOnly, Category = PathLink)
	TWeakObjectPtr<const UUTPathNode> Start;
	/** poly on source that is closest to EndPoly
	 * note that it does not necessarily share a navmesh edge with EndPoly if this is a link that's not simple walkable (jump, teleport, etc)
	 */
	UPROPERTY()
	uint64 StartEdgePoly;
	/** endpoint path node */
	UPROPERTY(BlueprintReadOnly, Category = PathLink)
	TWeakObjectPtr<const UUTPathNode> End;
	/** poly on End's side of the edge between the two nodes */
	UPROPERTY()
	uint64 EndPoly;
	/** additional polys that are guaranteed reachable from Start if EndPoly is
	 * these polys are NOT used for distance calculations, only for determining ideal endpoint when actually moving along this link
	 * this is intended to optimize by limiting the number of path connections when one node has a number of viable direct paths to another
	 * (primarily jump paths)
	 */
	UPROPERTY()
	TArray<uint64> AdditionalEndPolys;
	// optional class describing this path connection; if NULL it is assumed to be a standard walkable path with no special properties or traversal method
	UPROPERTY(BlueprintReadOnly, Category = PathLink)
	TWeakObjectPtr<class UUTReachSpec> Spec;
	/** maximum agent radius allowed */
	UPROPERTY(BlueprintReadOnly, Category = PathLink)
	int32 CollisionRadius;
	/** maximum agent height allowed */
	UPROPERTY(BlueprintReadOnly, Category = PathLink)
	int32 CollisionHeight;
	/** traversal flags (e.g. jumping required) */
	UPROPERTY(BlueprintReadOnly, Category = PathLink)
	uint16 ReachFlags;
	/** path distance from each poly in Start to EndPoly
	* this array mirrors Start->Polys
	*/
	UPROPERTY()
	TArray<int32> Distances;

	/** returns whether this path link is filled in (has actual path data) since e.g. UTBot keeps a copy for what path it is currently traversing that may be zeroed if it's travelling direct to target */
	inline bool IsSet()
	{
		return Start != NULL && End != NULL;
	}

	inline bool Supports(int32 TestRadius, int32 TestHeight, int32 MoveFlags) const
	{
		return (TestRadius <= CollisionRadius && TestHeight <= CollisionHeight && (MoveFlags & ReachFlags) == ReachFlags);
	}

	// NOTE: Asker may be NULL
	int32 CostFor(APawn* Asker, const FNavAgentProperties& AgentProps, NavNodeRef StartPoly, const class AUTRecastNavMesh* NavMesh) const;

	bool GetMovePoints(const FVector& StartLoc, APawn* Asker, const FNavAgentProperties& AgentProps, const struct FRouteCacheItem& Target, const TArray<FRouteCacheItem>& FullRoute, const class AUTRecastNavMesh* NavMesh, TArray<FComponentBasedPosition>& MovePoints) const;
	/** implementation of GetMovePoints() for paths with a jump at the end, split out because some UTReachSpec paths also use it */
	bool GetJumpMovePoints(const FVector& StartLoc, APawn* Asker, const FNavAgentProperties& AgentProps, const struct FRouteCacheItem& Target, const TArray<FRouteCacheItem>& FullRoute, const class AUTRecastNavMesh* NavMesh, TArray<FComponentBasedPosition>& MovePoints) const;

	/** returns color to identify the path type for debug path drawing */
	FLinearColor GetPathColor() const;

	FUTPathLink()
		: Start(NULL), StartEdgePoly(INVALID_NAVNODEREF), End(NULL), EndPoly(INVALID_NAVNODEREF), Spec(NULL), CollisionRadius(0), CollisionHeight(0), ReachFlags(0)
	{}
	FUTPathLink(const UUTPathNode* InStart, const NavNodeRef InStartEdgePoly, const UUTPathNode* InEnd, const NavNodeRef InEndPoly, UUTReachSpec* InSpec, int32 InRadius, int32 InHeight, uint32 InFlags)
		: Start(InStart), StartEdgePoly(InStartEdgePoly), End(InEnd), EndPoly(InEndPoly), Spec(InSpec), CollisionRadius(InRadius), CollisionHeight(InHeight), ReachFlags(InFlags)
	{}
	FUTPathLink(const FUTPathLink& Other) = default;
	FUTPathLink& operator= (const FUTPathLink& Other)
	{
		new(this) FUTPathLink(Other);
		return *this;
	}
};

UCLASS(NotPlaceable)
class UNREALTOURNAMENT_API UUTPathNode : public UObject
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	/** color for debug drawing to show this node and its owned polys */
	FLinearColor DebugDrawColor;
#endif

	/** appoximate center point of the tiles this node is contained by
	* this point is guaranteed to be a walkable point on the navmesh
	*/
	UPROPERTY()
	FVector Location;
	/** smallest poly edge size (i.e. largest collision capsule that can traverse all polygons and edges in this node) */
	UPROPERTY(BlueprintReadOnly, Category = PathNode)
	FCapsuleSize MinPolyEdgeSize;
	/** navmesh polygon IDs that encompass this node and have compatible reachability attributes */
	UPROPERTY()
	TArray<uint64> Polys;
	/** links to other paths */
	UPROPERTY(BlueprintReadOnly, Category = PathNode)
	TArray<FUTPathLink> Paths;
	/** game objects and other Actor points of interest that are within the Tiles */
	UPROPERTY(BlueprintReadOnly, Category = PathNode)
	TArray< TWeakObjectPtr<AActor> > POIs;

	/** number of kills that have occurred while the killer is standing in this node's area */
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = AIMapData)
	int32 NearbyKills;
	/** number of deaths that have occurred while the victim is standing in this node's area */
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = AIMapData)
	int32 NearbyDeaths;
	/** number of times a bot has attempted to use this path node as a hiding spot (e.g. CTF flag carrier) */
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = AIMapData)
	int32 HideAttempts;
	/** average amount of time a bot hid on this node before being detected by an enemy */
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = AIMapData)
	float AvgHideDuration;

	/** returns index to best link in Paths for Asker to move from this node to Target, or INDEX_NONE if no link is found that can be used
	 * note that there may be multiple links to the other node with different traversability properties; the one with shortest Distance is used
	 */
	int32 GetBestLinkTo(NavNodeRef StartPoly, const struct FRouteCacheItem& Target, APawn* Asker, const FNavAgentProperties& AgentProps, const AUTRecastNavMesh* NavMesh) const;
};