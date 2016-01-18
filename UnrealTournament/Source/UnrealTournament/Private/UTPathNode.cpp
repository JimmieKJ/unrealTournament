// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTPathNode.h"
#include "UTReachSpec.h"
#include "UTRecastNavMesh.h"

int32 FUTPathLink::CostFor(APawn* Asker, const FNavAgentProperties& AgentProps, NavNodeRef StartPoly, const AUTRecastNavMesh* NavMesh) const
{
	checkSlow(Start.IsValid());
	checkSlow(Start->Polys.Find(StartPoly) != INDEX_NONE);
	checkSlow(Distances.Num() >= Start->Polys.Num());

	int32 Result;

	int32 Index = Start->Polys.Find(StartPoly);
	if (Distances[Index] > 0)
	{
		Result = Distances[Index];
	}
	else
	{
		// for distances that haven't been precached, use line distance
		FVector Center1 = FVector::ZeroVector;
		FVector Center2 = FVector::ZeroVector;
		if (!NavMesh->GetPolyCenter(StartPoly, Center1) || !NavMesh->GetPolyCenter(EndPoly, Center2))
		{
			Result = 1;
		}
		else
		{
			Result = FMath::TruncToInt((Center2 - Center1).Size());
		}
	}
	return (Spec.IsValid() ? Spec->CostFor(Result, *this, Asker, AgentProps, StartPoly, NavMesh) : Result);
}

bool FUTPathLink::GetMovePoints(const FVector& StartLoc, APawn* Asker, const FNavAgentProperties& AgentProps, const FRouteCacheItem& Target, const TArray<FRouteCacheItem>& FullRoute, const AUTRecastNavMesh* NavMesh, TArray<FComponentBasedPosition>& MovePoints) const
{
	if (Spec.IsValid())
	{
		return Spec->GetMovePoints(*this, StartLoc, Asker, AgentProps, Target, FullRoute, NavMesh, MovePoints);
	}
	else if (ReachFlags & R_SWIM)
	{
		if (Start->PhysicsVolume == NULL || !Start->PhysicsVolume->bWaterVolume)
		{
			// use standard code to get in the water
			return (ReachFlags & R_JUMP) ? GetJumpMovePoints(StartLoc, Asker, AgentProps, Target, FullRoute, NavMesh, MovePoints) : false;
		}
		else
		{
			NavNodeRef StartPoly = NavMesh->FindAnchorPoly(StartLoc, Asker, AgentProps);
			// first calculate route as if we were moving along the surface of the mesh
			TArray<NavNodeRef> PolyRoute;
			const FVector MeshSurfaceStart = NavMesh->GetPolyCenter(StartPoly);
			TArray<FComponentBasedPosition> OrigMovePoints;
			if (!NavMesh->FindPolyPath(MeshSurfaceStart, AgentProps, FRouteCacheItem(NavMesh->GetPolyCenter(StartEdgePoly), StartEdgePoly), PolyRoute, false) || PolyRoute.Num() == 0 || !NavMesh->DoStringPulling(MeshSurfaceStart, PolyRoute, AgentProps, OrigMovePoints))
			{
				// just try swimming straight there
				MovePoints.Add(FComponentBasedPosition(Target.GetLocation(Asker)));
				return true;
			}
			else
			{
				bool bAddedTarget = false;
				if (ReachFlags == R_SWIM && Target.Actor == NULL && !Target.IsDirectTarget())
				{
					OrigMovePoints.Add(FComponentBasedPosition(Target.GetLocation(Asker)));
					bAddedTarget = true;
				}

				// attempt to maintain the AI's current depth in the water
				for (const FComponentBasedPosition& Pos : OrigMovePoints)
				{
					if (Pos.Base != NULL)
					{
						// use unmodified because it could move around
						MovePoints.Add(Pos);
					}
					else
					{
						FVector TestLoc = Pos.Get();
						TestLoc.Z = StartLoc.Z;
						const FVector TraceStart = (MovePoints.Num() > 0) ? MovePoints.Last().Get() : StartLoc;
						const FCollisionShape AgentShape = FCollisionShape::MakeCapsule(AgentProps.AgentRadius, AgentProps.AgentHeight * 0.5f);
						if (Asker->GetWorld()->SweepTestByChannel(TraceStart, TestLoc, FQuat::Identity, ECC_Pawn, AgentShape))
						{
							// try halfway
							TestLoc.Z = (StartLoc.Z + Pos.Get().Z) * 0.5f;
							if (Asker->GetWorld()->SweepTestByChannel(TraceStart, TestLoc, FQuat::Identity, ECC_Pawn, AgentShape))
							{
								// have to use mesh surface
								MovePoints.Add(Pos);
							}
							else
							{
								MovePoints.Add(FComponentBasedPosition(TestLoc));
							}
						}
						else
						{
							MovePoints.Add(FComponentBasedPosition(TestLoc));
						}
					}
				}
				if (!bAddedTarget)
				{
					MovePoints.Add(FComponentBasedPosition(Target.GetLocation(Asker)));
				}
				return true;
			}
		}
	}
	else if (ReachFlags & R_JUMP)
	{
		return GetJumpMovePoints(StartLoc, Asker, AgentProps, Target, FullRoute, NavMesh, MovePoints);
	}
	else
	{
		return false;
	}
}

bool FUTPathLink::GetJumpMovePoints(const FVector& StartLoc, APawn* Asker, const FNavAgentProperties& AgentProps, const struct FRouteCacheItem& Target, const TArray<FRouteCacheItem>& FullRoute, const class AUTRecastNavMesh* NavMesh, TArray<FComponentBasedPosition>& MovePoints) const
{
	TArray<NavNodeRef> PolyRoute;
	if (NavMesh->FindPolyPath(StartLoc, AgentProps, FRouteCacheItem(NavMesh->GetPolySurfaceCenter(StartEdgePoly), StartEdgePoly), PolyRoute, false) && PolyRoute.Num() > 0 && NavMesh->DoStringPulling(StartLoc, PolyRoute, AgentProps, MovePoints))
	{
		FVector DirectLoc = Target.GetLocation(Asker);
		// if there's a future point in the route try adjusting move location to one of AdditionalEndPolys
		int32 Index = FullRoute.Find(Target);
		if (Index != INDEX_NONE && Index + 1 < FullRoute.Num() && AdditionalEndPolys.Contains(FullRoute[Index + 1].TargetPoly))
		{
			DirectLoc = NavMesh->GetPolySurfaceCenter(FullRoute[Index + 1].TargetPoly) + FVector(0.0f, 0.0f, AgentProps.AgentHeight * 0.5f);
		}
		// find poly wall that has the best angle to the target
		// add a move point for it if going straight to dest from poly center doesn't intersect that wall
		TArray<FLine> Walls = NavMesh->GetPolyWalls(StartEdgePoly);
		if (Walls.Num() > 0)
		{
			FVector PolyCenter = NavMesh->GetPolySurfaceCenter(StartEdgePoly);
			int32 BestIndex = INDEX_NONE;
			float BestAngle = -2.0f;
			for (int32 i = 0; i < Walls.Num(); i++)
			{
				FVector WallCenter = (Walls[i].A + Walls[i].B) * 0.5f;
				float TestAngle = (DirectLoc - WallCenter).GetSafeNormal() | (WallCenter - PolyCenter).GetSafeNormal();
				if (TestAngle > BestAngle)
				{
					BestIndex = i;
					BestAngle = TestAngle;
				}
			}
			// check for intersection against the wall
			FBox TestBox(0);
			TestBox += Walls[BestIndex].A;
			TestBox += Walls[BestIndex].B;
			TestBox.Min.Z = FMath::Min3<float>(TestBox.Min.Z, DirectLoc.Z, PolyCenter.Z);
			TestBox.Max.Z = FMath::Max3<float>(TestBox.Max.Z, DirectLoc.Z, PolyCenter.Z);
			FVector HitLoc;
			FVector HitNormal;
			float HitTime;
			if (!FMath::LineExtentBoxIntersection(TestBox, DirectLoc, PolyCenter, FVector::ZeroVector, HitLoc, HitNormal, HitTime))
			{
				FVector WallCenter = (Walls[BestIndex].A + Walls[BestIndex].B) * 0.5f;
				WallCenter.Z = NavMesh->GetPolyZAtLoc(StartEdgePoly, FVector2D(WallCenter));
				MovePoints.Add(FComponentBasedPosition(WallCenter + FVector(0.0f, 0.0f, AgentProps.AgentHeight * 0.5f)));
			}
		}
		MovePoints.Add(FComponentBasedPosition(DirectLoc));
		return true;
	}
	else
	{
		return false;
	}
}

FLinearColor FUTPathLink::GetPathColor() const
{
	if (Spec.IsValid())
	{
		return Spec->GetPathColor();
	}
	else if (ReachFlags & R_SWIM)
	{
		return FLinearColor(0.5f, 0.5f, 1.0f);
	}
	else if (ReachFlags & R_JUMP)
	{
		return FLinearColor(0.5f, 0.0f, 1.0f);
	}
	else if (CollisionHeight < GetDefault<AUTRecastNavMesh>()->GetHumanPathSize().Height)
	{
		return FLinearColor(0.0f, 0.2f, 0.0f);
	}
	else
	{
		return FLinearColor(0.0f, 1.0f, 0.0f);
	}
}

UUTPathNode::UUTPathNode(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	DebugDrawColor = FColor::MakeRandomColor();
#endif
}

int32 UUTPathNode::GetBestLinkTo(NavNodeRef StartPoly, const struct FRouteCacheItem& Target, APawn* Asker, const FNavAgentProperties& AgentProps, const AUTRecastNavMesh* NavMesh) const
{
	if (!Polys.Contains(StartPoly))
	{
		return INDEX_NONE;
	}
	else
	{
		int32 Radius, Height, MaxFallSpeed;
		uint32 MoveFlags;
		AUTRecastNavMesh::CalcReachParams(Asker, AgentProps, Radius, Height, MaxFallSpeed, MoveFlags);

		int32 BestDist = MAX_int32;
		int32 BestIndex = INDEX_NONE;
		for (int32 i = 0; i < Paths.Num(); i++)
		{
			if (Paths[i].End.IsValid() && Paths[i].End == Target.Node && Paths[i].EndPoly == Target.TargetPoly && Paths[i].Supports(Radius, Height, MoveFlags))
			{
				int32 Dist = Paths[i].CostFor(Asker, AgentProps, StartPoly, NavMesh);
				if (Dist < BestDist)
				{
					BestIndex = i;
					BestDist = Dist;
				}
			}
		}
		return BestIndex;
	}
}