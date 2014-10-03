// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "TickableEditorObject.h"
#endif

#include "UTReachSpec.h"
#include "AI/Navigation/NavigationTypes.h"
#include "UTPathNode.h"

#include "UTRecastNavMesh.generated.h"

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
public:
	/** polygon to target to reach this route point
	 * for nodes, this is the first poly on the node that connects to the previous node
	 * for actors, this is the polygon the Actor was on when the route was generated
	 */
	NavNodeRef TargetPoly;

	inline bool IsValid() const
	{
		return TargetPoly != INVALID_NAVNODEREF && !Node.IsStale() && !Actor.IsStale();
	}

	inline void Clear()
	{
		*this = FRouteCacheItem();
	}

	inline FVector GetLocation() const
	{
		return Actor.IsValid() ? Actor->GetActorLocation() : Location;
	}

	FRouteCacheItem()
		: Node(NULL), Actor(NULL), Location(FVector::ZeroVector), TargetPoly(INVALID_NAVNODEREF)
	{}
	FRouteCacheItem(TWeakObjectPtr<UUTPathNode> InNode, const FVector& InLoc, NavNodeRef InTargetPoly)
		: Node(InNode), Actor(NULL), Location(InLoc), TargetPoly(InTargetPoly)
	{}
	FRouteCacheItem(TWeakObjectPtr<AActor> InActor, const FVector& InLoc, NavNodeRef InTargetPoly)
		: Node(NULL), Actor(InActor), Location(InLoc), TargetPoly(InTargetPoly)
	{}
	FRouteCacheItem(const FVector& InLoc, NavNodeRef InTargetPoly)
		: Node(NULL), Actor(NULL), Location(InLoc), TargetPoly(InTargetPoly)
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
	explicit FSingleEndpointEval(FVector InGoalLoc)
		: GoalActor(NULL), GoalLoc(InGoalLoc)
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

	/** extension to the default 2D Raycast() function with a simple Z bounding box check for poly centers
	 * the default implementation can false negative when there is a walkable polygon directly under the trace even if in 3D the trace would hit a polygon border
	 * this method gets around this by doing a simple Z check, which in the case of slopes may return false positives (hits)
	 * but in cases where it is better to be conservative than optimistic this is a preferrable option
	 */
	bool RaycastWithZCheck(const FVector& RayStart, const FVector& RayEnd, float ZExtent, FVector& HitLocation) const;

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
	 * @param PolyRoute (optional) - the polygons to traverse on the navmesh to hit all the desired nodes. Not specifying this when not needed improves performance somewhat. (for example, if you intend to re-run the search regularly throughout the path, you don't need the whole detailed path returned)
	 * @return whether a valid path was found
	 */
	virtual bool FindBestPath(APawn* Asker, const FNavAgentProperties& AgentProps, FUTNodeEvaluator& NodeEval, const FVector& StartLoc, float& Weight, bool bAllowDetours, TArray<FRouteCacheItem>& NodeRoute, TArray<NavNodeRef>* PolyRoute = NULL);

	/** calculate effective traveling distance between two polys
	 * returns direct distance if reachable by straight line or no navmesh path exists, otherwise does navmesh pathfinding and returns path distance
	 * this function is designed for calculating UTPathLink distances between known accessible nodes during path building and isn't intended for gameplay
	 */
	int32 CalcPolyDistance(NavNodeRef StartPoly, NavNodeRef EndPoly);

	/** returns list of polygons from source to target */
	virtual bool FindPolyPath(FVector StartLoc, const FNavAgentProperties& AgentProps, const FRouteCacheItem& Target, TArray<NavNodeRef>& PolyRoute, bool bSkipCurrentPoly = true) const;

	/** returns a string-pulled list of move points to go from StartLoc to Target */
	virtual bool GetMovePoints(const FVector& StartLoc, APawn* Asker, const FNavAgentProperties& AgentProps, const FRouteCacheItem& Target, const TArray<FRouteCacheItem>& FullRoute, TArray<FComponentBasedPosition>& MovePoints, FUTPathLink& NodeLink, float* TotalDistance = NULL) const;

	/** takes the passed in node list and string pulls a valid smooth set of points to traverse that path */
	virtual bool DoStringPulling(const FVector& StartLoc, const TArray<NavNodeRef>& PolyRoute, const FNavAgentProperties& AgentProps, TArray<FComponentBasedPosition>& MovePoints) const;

	/** returns if the agent has reached the specified goal, taking into account any special reachability concerns (i.e. if touching is required use collision instead of navigation query, etc)
	 * ASKER is required here!
	 */
	virtual bool HasReachedTarget(APawn* Asker, const FNavAgentProperties& AgentProps, const FRouteCacheItem& Target, const FUTPathLink& CurrentPath) const;

	/** HACK: workaround for navmesh not supporting moving objects
	* and thus when on lifts players will be temporarily off the navmesh
	* look for lift and try to figure out what poly the lift is taking the Pawn to
	* hopefully will become unnecessary in a future engine version
	*/
	NavNodeRef FindLiftPoly(APawn* Asker, const FNavAgentProperties& AgentProps) const;
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
	FCapsuleSize GetSteppedEdgeSize(const struct dtPoly* PolyData, const struct dtMeshTile* TileData, const struct dtLink& Link) const;

	/** set min edge/poly sizes for the given path node, generally matching the navmesh poly data with one of the values in SizeSteps */
	virtual void SetNodeSize(UUTPathNode* Node);

	virtual void DeletePaths();

	/** returns if the given distance is traversable only by jumping and/or falling
	 * note: returns false if walk reachable; use Raycast() to check that
	 */
	virtual bool OnlyJumpReachable(APawn* Scout, FVector Start, const FVector& End, NavNodeRef StartPoly = INVALID_NAVNODEREF, NavNodeRef EndPoly = INVALID_NAVNODEREF, float MaxJumpZ = -1.0f, float* RequiredJumpZ = NULL, float* MaxFallSpeed = NULL) const;

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
	TSubobjectPtr<class UUTNavGraphRenderingComponent> NodeRenderer;
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

		PathNodes.Remove(NULL); // really shouldn't happen but no need to crash for it
		for (UUTPathNode* Node : PathNodes)
		{
			for (NavNodeRef PolyRef : Node->Polys)
			{
				PolyToNode.Add(PolyRef, Node);
			}
		}
	}

	virtual void SetConfig(const FNavDataConfig& Src) override
	{
		// we have set the proper values already, ignore this redundant and convoluted stuff
	}

#if WITH_EDITOR
	class FUTNavMeshEditorTick* EditorTick;
	~AUTRecastNavMesh();
#endif

#if WITH_NAVIGATION_GENERATOR
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
	virtual void PreSave() override
	{
		Super::PreSave();
		
		// make sure special links are done
		BuildSpecialLinks(MAX_int32);
	}
#endif // WITH_NAVIGATION_GENERATOR
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