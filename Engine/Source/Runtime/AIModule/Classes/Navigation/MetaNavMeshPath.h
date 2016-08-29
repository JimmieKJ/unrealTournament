// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/RecastNavMesh.h"

enum class EMetaPathUpdateReason : uint8 
{
	PathFinished,
	MoveTick,
};

/** 
 * FMetaNavMeshPath allows creating hierarchical or guided navmesh paths
 *  
 * Waypoints array defines list of locations that will be used to create actual FNavMeshPath data during path following.
 * On path following start and upon reaching waypoint, path will request update from owning navmesh with following parameters:
 * - start location set to agent location
 * - end location set to target waypoint (or goal actor for last one)
 * - goal actor set only for last section
 *
 * Since path updates itself for moving agent it really shouldn't be reused by others.
 */
struct AIMODULE_API FMetaNavMeshPath : public FNavMeshPath
{
	typedef FNavMeshPath Super;
	static const FNavPathType Type;

	FMetaNavMeshPath();
	FMetaNavMeshPath(const TArray<FVector>& InWaypoints, const ANavigationData& NavData);
	FMetaNavMeshPath(const TArray<FVector>& InWaypoints, const AController& Owner);

	/** initialize path for path following */
	virtual void Initialize(const FVector& AgentLocation);

	/** try switching to next waypoint */
	virtual bool ConditionalMoveToNextSection(const FVector& AgentLocation, EMetaPathUpdateReason Reason);

	/** updates underlying navmesh path for current target waypoint */
	virtual bool UpdatePath(const FVector& AgentLocation);

	/** copy properties of other meta path */
	virtual void CopyFrom(const FMetaNavMeshPath& Other);

	/** returns true if path at last waypoint */
	bool IsLastSection() const { return (TargetWaypointIdx == (Waypoints.Num() - 1)) && (Waypoints.Num() > 0); }

	/** returns index of current target waypoint */
	int32 GetTargetWaypointIndex() const { return TargetWaypointIdx; }

	/** returns number of waypoints */
	int32 GetNumWaypoints() const { return Waypoints.Num(); }

	/** returns waypoint array */
	const TArray<FVector>& GetWaypoints() const { return Waypoints; }

	/** tries to set waypoints, fails when path is ready being followed */
	bool SetWaypoints(const TArray<FVector>& InWaypoints);

	/** returns radius for switching to next waypoint during path following */
	float GetWaypointSwitchRadius() const { return WaypointSwitchRadius; }

	/** sets radius for switching to next waypoint during path following */
	void SetWaypointSwitchRadius(float InSwitchRadius) { WaypointSwitchRadius = InSwitchRadius; }

	/** returns approximate length of path, ignores parameters */
	virtual float GetLengthFromPosition(FVector SegmentStart, uint32 NextPathPointIndex) const override;
	
	/** returns approximate length of path, ignores parameter */
	virtual float GetCostFromIndex(int32 PathPointIndex) const override;

#if ENABLE_VISUAL_LOG
	virtual void DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const override;
#endif
	virtual void DebugDraw(const ANavigationData* NavData, FColor PathColor, UCanvas* Canvas, bool bPersistent, const uint32 NextPathPointIndex = 0) const override;

protected:

	/** list of waypoints, including start and end of path */
	TArray<FVector> Waypoints;

	/** sum of 3D distance along waypoints, used for approximating length of path */
	float ApproximateLength;

	/** update navmesh path when this close to target waypoint */
	float WaypointSwitchRadius;

	/** current target of path following */
	int32 TargetWaypointIdx;

	/** stored goal actor */
	TWeakObjectPtr<AActor> PathGoal;

	/** stored goal actor's tether distance */
	float PathGoalTetherDistance;

	/** switch to next waypoint  */
	bool MoveToNextSection(const FVector& AgentLocation);
};
