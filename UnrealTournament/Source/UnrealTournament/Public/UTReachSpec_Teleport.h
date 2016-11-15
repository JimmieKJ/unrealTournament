// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTReachSpec.h"
#include "UTRecastNavMesh.h"

#include "UTReachSpec_Teleport.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTReachSpec_Teleport : public UUTReachSpec
{
	GENERATED_UCLASS_BODY()

	/** the teleporter Actor that needs to be touched to teleport */
	UPROPERTY()
	TWeakObjectPtr<AActor> Teleporter;

	UUTReachSpec_Teleport(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
		PathColor = FLinearColor(0.0f, 0.0f, 1.0f);
	}

	virtual TWeakObjectPtr<AActor> GetDestActor() const override
	{
		return Teleporter;
	}

	virtual int32 CostFor(int32 DefaultCost, const FUTPathLink& OwnerLink, APawn* Asker, const FNavAgentProperties& AgentProps, const FUTReachParams& ReachParams, AController* RequestOwner, NavNodeRef StartPoly, const class AUTRecastNavMesh* NavMesh) override
	{
		// TODO: check if teleporter is active?
		return Teleporter.IsValid() ? DefaultCost : BLOCKED_PATH_COST;
	}

	virtual bool GetMovePoints(const FUTPathLink& OwnerLink, const FVector& StartLoc, APawn* Asker, const FNavAgentProperties& AgentProps, const struct FRouteCacheItem& Target, const TArray<FRouteCacheItem>& FullRoute, const class AUTRecastNavMesh* NavMesh, TArray<FComponentBasedPosition>& MovePoints) const
	{
		TArray<NavNodeRef> PolyRoute;
		NavMesh->FindPolyPath(StartLoc, AgentProps, FRouteCacheItem(NavMesh->GetPolyCenter(OwnerLink.StartEdgePoly), OwnerLink.StartEdgePoly), PolyRoute, false);
		NavMesh->DoStringPulling(StartLoc, PolyRoute, AgentProps, MovePoints);
		if (Teleporter.IsValid())
		{
			MovePoints.Add(FComponentBasedPosition(Teleporter->GetRootComponent(), Teleporter->GetActorLocation()));
		}
		return true;
	}
};