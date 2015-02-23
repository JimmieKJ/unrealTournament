// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "TickableEditorObject.h"
#endif

#include "UTReachSpec.h"
#include "AI/Navigation/NavigationTypes.h"
#include "UTPathNode.h"

#include "UTRecastNavMesh.generated.h"

USTRUCT(BlueprintType)
struct FLine
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Line)
	FVector A;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Line)
	FVector B;

	FLine()
	{}
	FLine(const FVector& InA, const FVector& InB)
		: A(InA), B(InB)
	{}
	inline FVector GetCenter() const
	{
		return (A + B) * 0.5f;
	}
};

USTRUCT(BlueprintType)
struct FRouteCacheItem
{
	GENERATED_USTRUCT_BODY()

	/** path node on the route (optional) */
	UPROPERTY(BlueprintReadOnly, Category = Route)
	TWeakObjectPtr<UUTPathNode> Node;
	/** Actor at this point on the route (optional) */
	UPROPERTY(BlueprintReadOnly, Category = Route)
	TWeakObjectPtr<AActor> Actor;
protected:
	/** location for this point on the route (at the time the route was generated; if the item is a moving Actor this is not updated) */
	UPROPERTY()
	FVector Location;
	/** set if this is a direct target with no navmesh bindings (AI moving to this will go in a straight line from its current location) */
	UPROPERTY()
	bool bDirectTarget;
public:
	/** polygon to target to reach this route point
	 * for nodes, this is the first poly on the node that connects to the previous node
	 * for actors, this is the polygon the Actor was on when the route was generated
	 */
	NavNodeRef TargetPoly;

	inline bool IsValid() const
	{
		return (bDirectTarget || TargetPoly != INVALID_NAVNODEREF) && !Node.IsStale() && !Actor.IsStale();
	}
	inline bool IsDirectTarget() const
	{
		return bDirectTarget && IsValid();
	}

	inline void Clear()
	{
		*this = FRouteCacheItem();
	}

	FVector GetLocation(APawn* Asker) const
	{
		if (Actor.IsValid())
		{
			return Actor->GetActorLocation();
		}
		else if (TargetPoly != INVALID_NAVNODEREF && Asker != NULL)
		{
			// adjust navmesh surface location for height of target
			return Location + FVector(0.0f, 0.0f, Asker->GetSimpleCollisionHalfHeight());
		}
		else
		{
			return Location;
		}
	}

	FRouteCacheItem()
		: Node(NULL), Actor(NULL), Location(FVector::ZeroVector), bDirectTarget(false), TargetPoly(INVALID_NAVNODEREF)
	{}
	FRouteCacheItem(TWeakObjectPtr<UUTPathNode> InNode, const FVector& InLoc, NavNodeRef InTargetPoly)
		: Node(InNode), Actor(NULL), Location(InLoc), bDirectTarget(false), TargetPoly(InTargetPoly)
	{}
	FRouteCacheItem(TWeakObjectPtr<AActor> InActor, const FVector& InLoc, NavNodeRef InTargetPoly)
		: Node(NULL), Actor(InActor), Location(InLoc), bDirectTarget(InTargetPoly == INVALID_NAVNODEREF), TargetPoly(InTargetPoly)
	{}
	explicit FRouteCacheItem(const FVector& InLoc, NavNodeRef InTargetPoly = INVALID_NAVNODEREF)
		: Node(NULL), Actor(NULL), Location(InLoc), bDirectTarget(InTargetPoly == INVALID_NAVNODEREF), TargetPoly(InTargetPoly)
	{}
	explicit FRouteCacheItem(TWeakObjectPtr<AActor> InActor)
		: Node(NULL), Actor(InActor), Location(InActor.IsValid() ? InActor->GetActorLocation() : FVector::ZeroVector), bDirectTarget(InActor.IsValid()), TargetPoly(INVALID_NAVNODEREF)
	{}


	bool operator== (const FRouteCacheItem& Other) const
	{
		return (Other.Node == Node && Other.Actor == Actor && Other.Location == Location && Other.TargetPoly == TargetPoly);
	}
};

/** node evaluation structure for pathfinding routines */
struct UNREALTOURNAMENT_API FUTNodeEvaluator
{
	/** called prior to pathing using the passed in navigation network
	 * use to cache node/poly locations for endpoints and such
	 * @return whether pathing can continue (e.g. might return false if desired target(s) are off the mesh)
	 */
	virtual bool InitForPathfinding(APawn* Asker, const FNavAgentProperties& AgentProps, AUTRecastNavMesh* NavData)
	{
		return true;
	}
	/** evaluates each node traversed
	 * @param Asker - Pawn doing the search. May be NULL
	 * @param AgentProps - size and capabilities for reachability
	 * @param Node - node being evaluated
	 * @param EntryLoc - path entry point into Node; i.e. where the agent would be when it entered Node's area if it traversed the current path
	 * @param TotalDistance - total path distance so far
	 * @return weighting as a path endpoint
	 */
	virtual float Eval(APawn* Asker, const FNavAgentProperties& AgentProps, const UUTPathNode* Node, const FVector& EntryLoc, int32 TotalDistance) = 0;

	/** adds optional evaluator specific cost to the given path link; return BLOCKED_PATH_COST to prevent a path from being used even if it would otherwise be valid */
	virtual uint32 GetTransientCost(const FUTPathLink& Link, APawn* Asker, const FNavAgentProperties& AgentProps, NavNodeRef StartPoly, int32 TotalDistance)
	{
		return 0;
	}

	/** optional function that allows the evaluator to add a point to move to at the end of the route, i.e. the final target
	 * since the core functionality only adds PathNodes
	 * only called if pathing succeeds (some node returned a value from Eval() that exceeds the minimum requirement passed into FindBestPath())
	 */
	virtual bool GetRouteGoal(AActor*& OutGoal, FVector& OutGoalLoc) const
	{
		return false;
	}
};

/** basic node evaluator for single endpoint */
struct UNREALTOURNAMENT_API FSingleEndpointEval : public FUTNodeEvaluator
{
	AActor* GoalActor;
	FVector GoalLoc;
	UUTPathNode* GoalNode;

	virtual bool InitForPathfinding(APawn* Asker, const FNavAgentProperties& AgentProps, AUTRecastNavMesh* NavData);
	virtual float Eval(APawn* Asker, const FNavAgentProperties& AgentProps, const UUTPathNode* Node, const FVector& EntryLoc, int32 TotalDistance);
	virtual bool GetRouteGoal(AActor*& OutGoal, FVector& OutGoalLoc) const override
	{
		OutGoal = GoalActor;
		OutGoalLoc = GoalLoc;
		return true;
	}

	explicit FSingleEndpointEval(AActor* InGoalActor)
		: GoalActor(InGoalActor), GoalLoc(InGoalActor->GetActorLocation())
	{}
	explicit FSingleEndpointEval(const FVector& InGoalLoc)
		: GoalActor(NULL), GoalLoc(InGoalLoc)
	{}
};

struct UNREALTOURNAMENT_API FSingleEndpointEvalWeighted : public FSingleEndpointEval
{
	/** map of additional node costs to bias the path taken to the target */
	TMap< TWeakObjectPtr<UUTPathNode>, uint32 > ExtraCosts;

	virtual uint32 GetTransientCost(const FUTPathLink& Link, APawn* Asker, const FNavAgentProperties& AgentProps, NavNodeRef StartPoly, int32 TotalDistance)
	{
		return ExtraCosts.FindRef(Link.End);
	}

	explicit FSingleEndpointEvalWeighted(AActor* InGoalActor)
		: FSingleEndpointEval(InGoalActor)
	{}
	explicit FSingleEndpointEvalWeighted(const FVector& InGoalLoc)
		: FSingleEndpointEval(InGoalLoc)
	{}
};

/** ends pathing when it encounters one of any number of target nodes */
struct UNREALTOURNAMENT_API FMultiPathNodeEval : public FUTNodeEvaluator
{
	TSet<const UUTPathNode*> Goals;

	virtual bool InitForPathfinding(APawn* Asker, const FNavAgentProperties& AgentProps, AUTRecastNavMesh* NavData) override
	{
		return Goals.Num() > 0;
	}
	virtual float Eval(APawn* Asker, const FNavAgentProperties& AgentProps, const UUTPathNode* Node, const FVector& EntryLoc, int32 TotalDistance) override
	{
		return Goals.Contains(Node) ? 10.0f : 0.0f;
	}

	FMultiPathNodeEval() = default;
	explicit FMultiPathNodeEval(const TSet<const UUTPathNode*>& InGoals)
		: Goals(InGoals)
	{}
	explicit FMultiPathNodeEval(const TArray<const UUTPathNode*>& InGoals)
		: Goals(InGoals)
	{}
};

UCLASS()
class AUTRecastNavMesh : public ARecastNavMesh
{
	GENERATED_UCLASS_BODY()

	friend struct FUTPathNodeRenderProxy;
	friend struct FUTPathLinkRenderProxy;

	/** nodes and their links are clamped to the highest of these sizes their internal poly edges pass
	 * this is used to merge as many polys into fewer nodes as possible and should be set to all potential agent sizes
	 */
	UPROPERTY(EditDefaultsOnly, Category = Generation)
	TArray<FCapsuleSize> SizeSteps;
	UPROPERTY(EditAnywhere, Category = Generation)
	float JumpTestThreshold2D;
	/** character class to pull default movement properties from */
	UPROPERTY(EditDefaultsOnly, Category = Generation)
	TSubclassOf<ACharacter> ScoutClass;
	/** default effective jump Z speed; i.e. combination of all 'standard' jump moves
	 * all jump-capable agents are assumed to be able to jump with at least this much speed
	 * if not set, uses ScoutClass's JumpZ directly
	 */
	UPROPERTY(EditDefaultsOnly, Category = Generation)
	float DefaultEffectiveJumpZ;

	/** convenience redirect to ARecastNavMesh::GetPolyCenter() that returns the center instead of out parameter
	 * returns ZeroVector if PolyID is invalid
	 */
	FVector GetPolyCenter(NavNodeRef PolyID) const
	{
		FVector Result;
		return (Super::GetPolyCenter(PolyID, Result) ? Result : FVector::ZeroVector);
	}

	FORCEINLINE bool GetPolyCenter(NavNodeRef PolyID, FVector& OutCenter) const
	{
		return Super::GetPolyCenter(PolyID, OutCenter);
	}

	/** returns the poly XY center projected onto the surface of the mesh (GetPolyCenter() just returns an average of the verts)
	 * this function is better when using the results as a movement or pathing target
	 */
	FVector GetPolySurfaceCenter(NavNodeRef PolyID) const;

	/** return Z coordinate of the given polygon at the specified XY location
	 * if the specified location is not on the polygon, returns the Z at the polygon's center
	 * if the PolyID is invalid, returns 0.0f
	 */
	float GetPolyZAtLoc(NavNodeRef PolyID, const FVector2D& Loc2D) const;

	/** return size for standard human traversable paths (used by special paths to set the appropriate size) */
	virtual FCapsuleSize GetHumanPathSize() const;
	inline FCapsuleSize GetMaxPathSize() const
	{
		FCapsuleSize Max(0, 0);
		for (int32 i = 0; i < SizeSteps.Num(); i++)
		{
			Max.Radius = FMath::Max<int32>(Max.Radius, SizeSteps[i].Radius);
			Max.Height = FMath::Max<int32>(Max.Height, SizeSteps[i].Height);
		}
		return Max;
	}

	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;

	virtual FString GetMapLearningDataFilename() const;
	/** loads AI learning data for the current map, if any */
	virtual void LoadMapLearningData();
	/** saves AI learning data for the current map */
	virtual void SaveMapLearningData();

	// add or remove an Actor from the list of POIs
	// some pathfinding functions (inventory searches, for example) use this list to efficiently find possible endpoints
	virtual void AddToNavigation(AActor* NewPOI);
	virtual void RemoveFromNavigation(AActor* OldPOI);

	/** find pathnode corresponding to target's position on the navmesh (if any) */
	virtual UUTPathNode* FindNearestNode(const FVector& TestLoc, const FVector& Extent) const;

	/** returns PathNode that owns the given poly */
	inline UUTPathNode* GetNodeFromPoly(NavNodeRef PolyRef) const
	{
		return PolyToNode.FindRef(PolyRef);
	}

	/** extension to the default 2D Raycast() function that has a sanity check against the most common Z-axis false negative
	 * the default implementation can false negative when there is a walkable polygon directly under the trace even if in 3D the trace would hit a polygon border
	 * this method gets around this by attempting to match the end location to an explicit poly and checks that the trace traversed it
	 */
	bool RaycastWithZCheck(const FVector& RayStart, const FVector& RayEnd, FVector* HitLocation = NULL, NavNodeRef* LastPoly = NULL) const;

	/** returns an array of walls (edges with no connection to a neighbor) for the passed in poly */
	TArray<FLine> GetPolyWalls(NavNodeRef PolyRef) const;

	/** returns the extent that should be used for the given POI (non-agent path target) when determining what poly and/or PathNode it is on
	 * this is needed because some POIs don't have collision and therefore don't have reasonable bounds to derive an extent from
	 * passing NULL is valid (returns minimum extent that any POI should use)
	 */
	virtual FVector GetPOIExtent(AActor* POI) const;

	/** calculate reachability parameters and flags for pathfinding */
	static void CalcReachParams(APawn* Asker, const FNavAgentProperties& AgentProps, int32& Radius, int32& Height, int32& MaxFallSpeed, uint32& MoveFlags);

	/** find best path to desired target (or one of many targets) on the node network using NodeEval function to evaluate nodes, then use that to build a poly route over the navmesh
	 *
	 * @param Asker - Pawn doing the search. Can be NULL, not used by base functionality. Some NodeEval functions use this to evaluate potential endpoints (e.g. inventory search)
	 * @param AgentProps - size and capabilities for reachability
	 * @param NodeEval - evaluator for each node traversed. Search is stopped immediately as successful if this object's Eval() returns >= 1.0f, otherwise path to highest rated node is returned
	 * @param StartLoc - start location for search. Pathing fails if this is not on the navmesh.
	 * @param Weight (in/out) - input is minimum weight (as returned by NodeEval) for a node to be chosen; output is the weight of the found node (if any)
	 * @param bAllowDetours - whether to allow post processing the final path with redirects to nearby inventory items (ignored if Asker is NULL)
	 * @param NodeRoute - the route found over the node network
	 * @return whether a valid path was found
	 */
	virtual bool FindBestPath(APawn* Asker, const FNavAgentProperties& AgentProps, FUTNodeEvaluator& NodeEval, const FVector& StartLoc, float& Weight, bool bAllowDetours, TArray<FRouteCacheItem>& NodeRoute);

	/** calculate effective traveling distance between two polys
	 * returns direct distance if reachable by straight line or no navmesh path exists, otherwise does navmesh pathfinding and returns path distance
	 * this function is designed for calculating UTPathLink distances between known accessible nodes during path building and isn't intended for gameplay
	 */
	int32 CalcPolyDistance(NavNodeRef StartPoly, NavNodeRef EndPoly);

	/** trace test potential jump arcs for obstructions (does not walk test, align to edge, etc; see OnlyJumpReachable()) */
	virtual bool JumpTraceTest(FVector Start, const FVector& End, NavNodeRef StartPoly, NavNodeRef EndPoly, FCollisionShape ScoutShape, float XYSpeed, float GravityZ, float BaseJumpZ, float MaxJumpZ = -1.0f, float* RequiredJumpZ = NULL, float* MaxFallSpeed = NULL) const;

	/** returns list of polygons from source to target */
	virtual bool FindPolyPath(FVector StartLoc, const FNavAgentProperties& AgentProps, const FRouteCacheItem& Target, TArray<NavNodeRef>& PolyRoute, bool bSkipCurrentPoly = true) const;

	/** returns a string-pulled list of move points to go from StartLoc to Target */
	virtual bool GetMovePoints(const FVector& StartLoc, APawn* Asker, const FNavAgentProperties& AgentProps, const FRouteCacheItem& Target, const TArray<FRouteCacheItem>& FullRoute, TArray<FComponentBasedPosition>& MovePoints, FUTPathLink& NodeLink, float* TotalDistance = NULL) const;

	/** takes the passed in node list and string pulls a valid smooth set of points to traverse that path */
	virtual bool DoStringPulling(const FVector& StartLoc, const TArray<NavNodeRef>& PolyRoute, const FNavAgentProperties& AgentProps, TArray<FComponentBasedPosition>& MovePoints) const;

	/** returns if the agent has reached the specified goal, taking into account any special reachability concerns (i.e. if touching is required use collision instead of navigation query, etc)
	 * ASKER is required here!
	 */
	virtual bool HasReachedTarget(APawn* Asker, const FNavAgentProperties& AgentProps, const FRouteCacheItem& Target) const;

	/** returns poly that the given target should be "on" for purposes of pathing, i.e. the source location for path finding and following
	* Asker may be NULL; if specified and it has a valid LastReachedMoveTarget then that takes precedence over a poly search to minimize errors where a Pawn reaches a target but then doesn't use it as the next start location
	*/
	virtual NavNodeRef FindAnchorPoly(const FVector& TestLoc, APawn* Asker, const FNavAgentProperties& AgentProps) const;

	/** HACK: workaround for navmesh not supporting moving objects
	* and thus when on lifts players will be temporarily off the navmesh
	* look for lift and try to figure out what poly the lift is taking the Pawn to
	* hopefully will become unnecessary in a future engine version
	*/
	NavNodeRef FindLiftPoly(APawn* Asker, const FNavAgentProperties& AgentProps) const;

	/** returns a list of polys adjacent to that of the passed in location
	 * if bOnlyWalkable is false, includes the far side of edges that require additional movement (jump, teleport, special move, etc)
	 */
	virtual void FindAdjacentPolys(APawn* Asker, const FNavAgentProperties& AgentProps, NavNodeRef StartPoly, bool bOnlyWalkable, TArray<NavNodeRef>& Polys) const;
protected:
	/** graph of path nodes overlaying the mesh */
	UPROPERTY()
	TArray<UUTPathNode*> PathNodes;
	/** maps tile ID to PathNode pointer */
	TMap<NavNodeRef, UUTPathNode*> PolyToNode;
	/** all reach specs inside PathNodes/Links for easy GC */
	UPROPERTY()
	TArray<UUTReachSpec*> AllReachSpecs;
	/** transient POI to Node table to optimize AddToNavigation()/RemoveFromNavigation() */
	TMap<TWeakObjectPtr<AActor>, UUTPathNode*> POIToNode;

	/** get size of poly edge link clamped to one of the SizeSteps
	 * inputs are all assumed valid
	 */
	FCapsuleSize GetSteppedEdgeSize(NavNodeRef PolyRef, const struct dtPoly* PolyData, const struct dtMeshTile* TileData, const struct dtLink& Link) const;

	/** set min edge/poly sizes for the given path node, generally matching the navmesh poly data with one of the values in SizeSteps */
	virtual void SetNodeSize(UUTPathNode* Node);

	virtual void DeletePaths();

	/** returns if the given distance is traversable only by jumping and/or falling
	 * note: returns false if walk reachable; use Raycast() to check that
	 */
	virtual bool OnlyJumpReachable(APawn* Scout, FVector Start, const FVector& End, NavNodeRef StartPoly = INVALID_NAVNODEREF, NavNodeRef EndPoly = INVALID_NAVNODEREF, float MaxJumpZ = -1.0f, float* RequiredJumpZ = NULL, float* MaxFallSpeed = NULL) const;

	const class dtQueryFilter* GetDefaultDetourFilter() const;

	// hide base functionality we don't want being used
	// if you are UT aware you should use the UT functions that use the node graph with more robust traversal options
private:
	FORCEINLINE FPathFindingResult FindPath(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query) const
	{
		checkSlow(false);
		return FPathFindingResult(ENavigationQueryResult::Invalid);
	}
	FORCEINLINE FPathFindingResult FindHierarchicalPath(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query) const
	{
		checkSlow(false);
		return FPathFindingResult(ENavigationQueryResult::Invalid);
	}
	FORCEINLINE bool TestPath(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query, int32* NumVisitedNodes) const
	{
		checkSlow(false);
		return false;
	}
	FORCEINLINE bool TestHierarchicalPath(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query, int32* NumVisitedNodes) const
	{
		checkSlow(false);
		return false;
	}

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	class UUTNavGraphRenderingComponent* NodeRenderer;
#endif

public:
	inline const TArray<const UUTPathNode*>& GetAllNodes() const
	{
		return *(const TArray<const UUTPathNode*>*)&PathNodes;
	}

	virtual bool ShouldTickIfViewportsOnly() const override
	{
		return true;
	}

	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
	{
		Super::ApplyWorldOffset(InOffset, bWorldShift);

		for (UUTPathNode* Node : PathNodes)
		{
			Node->Location += InOffset;
		}
	}

	virtual void PostLoad()
	{
		Super::PostLoad();

		// work around nav data selection code that keeps breaking us
		NavDataConfig.AgentRadius = GetClass()->GetDefaultObject<AUTRecastNavMesh>()->AgentRadius;
		NavDataConfig.AgentHeight = GetClass()->GetDefaultObject<AUTRecastNavMesh>()->AgentHeight;
		NavDataConfig.AgentStepHeight = GetClass()->GetDefaultObject<AUTRecastNavMesh>()->AgentMaxStepHeight;

		// HACK: UNavigationSystem's preloading of nav classes doesn't work because UT module wasn't loaded yet
		UNavigationSystem::StaticClass()->GetDefaultObject()->PostInitProperties();

		PathNodes.Remove(NULL); // really shouldn't happen but no need to crash for it
		for (UUTPathNode* Node : PathNodes)
		{
			for (NavNodeRef PolyRef : Node->Polys)
			{
				PolyToNode.Add(PolyRef, Node);
			}
		}
	}

#if WITH_EDITOR
	class FUTNavMeshEditorTick* EditorTick;
	~AUTRecastNavMesh();
#endif

private:
	bool bIsBuilding;
	bool bUserRequestedBuild;
protected:
	/** building special links (jumps, translocator, etc) are done over time when rebuilding while editing; this is the current node or INDEX_NONE if done/not started */
	int32 SpecialLinkBuildNodeIndex;
public:
	/** builds the navigation nodes overlaying the navmesh data */
	virtual void BuildNodeNetwork();
	/** builds nonstandard (jumping, teleporting, etc) links between nodes */
	virtual void BuildSpecialLinks(int32 NumToProcess);

	virtual void TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;
	virtual void Tick(float DeltaTime) override;
	virtual void PreSave() override;

	virtual bool NeedsRebuild() const
	{
		return (Super::NeedsRebuild() || PathNodes.Num() == 0);
	}
};

inline AUTRecastNavMesh* GetUTNavData(UWorld* World)
{
	if (World->GetNavigationSystem() == NULL)
	{
		// workaround because engine doesn't want to create on clients by default
		World->SetNavigationSystem(NewObject<UNavigationSystem>(World, GEngine->NavigationSystemClass));
	}
	return Cast<AUTRecastNavMesh>(World->GetNavigationSystem()->GetMainNavData(FNavigationSystem::ECreateIfEmpty::DontCreate));
}

#if WITH_EDITOR
/** used to force our tick code in the editor */
class FUTNavMeshEditorTick : public FTickableEditorObject
{
public:
	TWeakObjectPtr<class AUTRecastNavMesh> Owner;
	/** used to avoid double tick if editor is actually ticking the mesh actor */
	bool bWasTicked;

	FUTNavMeshEditorTick(AUTRecastNavMesh* InOwner)
		: Owner(InOwner), bWasTicked(false)
	{}

	virtual void Tick(float DeltaTime) override
	{
		if (bWasTicked)
		{
			bWasTicked = false;
		}
		else if (Owner.IsValid())
		{
			Owner->Tick(DeltaTime);
		}
	}

	virtual bool IsTickable() const override
	{
		return true;
	}

	virtual TStatId GetStatId() const override
	{
		return TStatId();
	}
};
#endif