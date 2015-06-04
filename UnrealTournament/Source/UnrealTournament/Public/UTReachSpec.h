// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTPathNode.h"

#include "UTReachSpec.generated.h"

UCLASS(NotPlaceable, Abstract, CustomConstructor)
class UNREALTOURNAMENT_API UUTReachSpec : public UObject
{
	GENERATED_UCLASS_BODY()

	UUTReachSpec(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		PathColor = FLinearColor(0.0f, 1.0f, 0.0f);
	}

	/** path color as property for specs that only need one */
	FLinearColor PathColor;

	/** return color to draw the path in for debug/editor views */
	virtual FLinearColor GetPathColor() const
	{
		return PathColor;
	}

	/** get Actor to use as MoveTarget when moving along this path (generally used when completing this path involves a moving object or something that must be touched */
	virtual TWeakObjectPtr<AActor> GetMoveTargetActor() const
	{
		return NULL;
	}

	/** return traversal cost in UU or BLOCKED_PATH_COST to prevent the pah from being used */
	virtual int32 CostFor(int32 DefaultCost, const FUTPathLink& OwnerLink, APawn* Asker, const FNavAgentProperties& AgentProps, NavNodeRef StartPoly, const class AUTRecastNavMesh* NavMesh)
	{
		return DefaultCost;
	}

	/** return whether AI should pause before continuing move along this path (e.g. wait for elevator to reach the right place) */
	virtual bool WaitForMove(const FUTPathLink& OwnerLink, APawn* Asker, const FComponentBasedPosition& MovePos, const FRouteCacheItem& Target) const
	{
		return false;
	}

	/** if not using a direct move, gives the ReachSpec the chance to override the points that the AI should move through to get from its current start to End (assumed reachable)
	 * can be used to make sure the AI hits a required point, such as a trigger, a specific start spot for a jump or special move, etc
	 * return false to use the default shortest poly route behavior
	 */
	virtual bool GetMovePoints(const FUTPathLink& OwnerLink, const FVector& StartLoc, APawn* Asker, const FNavAgentProperties& AgentProps, const struct FRouteCacheItem& Target, const TArray<FRouteCacheItem>& FullRoute, const class AUTRecastNavMesh* NavMesh, TArray<FComponentBasedPosition>& MovePoints) const
	{
		return false;
	}
};