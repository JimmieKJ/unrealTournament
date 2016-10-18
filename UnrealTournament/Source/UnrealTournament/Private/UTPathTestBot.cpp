// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTPathTestBot.h"
#include "UTReachSpec_HighJump.h"
#include "UTReachSpec_WallDodge.h"
#include "UTReachSpec_Lift.h"

void AUTPathTestBot::ExecuteWhatToDoNext()
{
	Skill = 7.0f;
	Personality.MovementAbility = 1.0f;
	Personality.Jumpiness = 1.0f;
	if (GetUTChar() != nullptr)
	{
		GetUTChar()->Health = GetUTChar()->SuperHealthMax;
	}

	if (TestList.Num() > 0)
	{
		UUTPathNode* Anchor = NavData->GetNodeFromPoly(NavData->FindAnchorPoly(GetPawn()->GetNavAgentLocation(), GetPawn(), GetPawn()->GetNavAgentPropertiesRef()));

		if ( !TestList.IsValidIndex(CurrentTestIndex) || Anchor == NULL || (TestList[CurrentTestIndex].Src.Node != Anchor && TestList[CurrentTestIndex].Dest.Node != Anchor) ||
			(LastPawnLoc - GetPawn()->GetActorLocation()).IsNearlyZero() || NavData->HasReachedTarget(GetPawn(), GetPawn()->GetNavAgentPropertiesRef(), TestList[CurrentTestIndex].Dest) )
		{
			// move to next test
			CurrentTestIndex = TestList.IsValidIndex(CurrentTestIndex + 1) ? (CurrentTestIndex + 1) : 0;

			GetPawn()->TeleportTo(TestList[CurrentTestIndex].Src.GetLocation(GetPawn()), (TestList[CurrentTestIndex].Dest.GetLocation(GetPawn()) - TestList[CurrentTestIndex].Src.GetLocation(GetPawn())).GetSafeNormal2D().Rotation());
		}

		SetMoveTarget(TestList[CurrentTestIndex].Dest);
		StartWaitForMove();
		LastPawnLoc = GetPawn()->GetActorLocation();
	}
}

void AUTPathTestBot::GatherTestList(bool bHighJumps, bool bWallDodges, bool bLifts, bool bLiftJumps)
{
	TestList.Empty();
	CurrentTestIndex = INDEX_NONE;

	NavData = GetUTNavData(GetWorld());
	if (NavData != NULL)
	{
		const TArray<const UUTPathNode*>& NodeList = NavData->GetAllNodes();
		for (const UUTPathNode* Node : NodeList)
		{
			for (const FUTPathLink& Link : Node->Paths)
			{
				if ( (bHighJumps && Cast<UUTReachSpec_HighJump>(Link.Spec.Get()) != NULL) ||
					(bWallDodges && Cast<UUTReachSpec_WallDodge>(Link.Spec.Get()) != NULL) )
				{
					new(TestList) FPathTestEntry(FRouteCacheItem(Link.Start.Get(), NavData->GetPolySurfaceCenter(Link.StartEdgePoly), Link.StartEdgePoly), FRouteCacheItem(Link.End.Get(), NavData->GetPolySurfaceCenter(Link.EndPoly), Link.EndPoly));
				}
				else if (bLifts || bLiftJumps)
				{
					UUTReachSpec_Lift* LiftSpec = Cast<UUTReachSpec_Lift>(Link.Spec.Get());
					if (LiftSpec != NULL && bLiftJumps == bool(Link.ReachFlags & R_JUMP))
					{
						new(TestList) FPathTestEntry(FRouteCacheItem(Link.Start.Get(), NavData->GetPolySurfaceCenter(Link.StartEdgePoly), Link.StartEdgePoly), FRouteCacheItem(Link.End.Get(), NavData->GetPolySurfaceCenter(Link.EndPoly), Link.EndPoly));
					}
				}
			}
		}
	}
}