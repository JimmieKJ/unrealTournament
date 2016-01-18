// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTRecastNavMesh.h"
#include "UTPathNode.h"
#include "UTLiftExit.h"
#include "UTReachSpec_Lift.h"
#include "MessageLog.h"
#include "UObjectToken.h"

AUTLiftExit::AUTLiftExit(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	Icon = ObjectInitializer.CreateOptionalDefaultSubobject<UBillboardComponent>(this, FName(TEXT("Icon")));
	if (Icon != NULL)
	{
		Icon->bHiddenInGame = true;
		RootComponent = Icon;
	}
}

bool AUTLiftExit::AddLiftPathsShared(const FVector& ExitLoc, AUTLift* TheLift, bool bRequireLiftJump, bool bOnlyExitPath, AUTRecastNavMesh* NavData)
{
	FVector NavExtent = NavData->GetPOIExtent(NULL);
	NavNodeRef MyPoly = NavData->UTFindNearestPoly(ExitLoc, NavExtent);
	UUTPathNode* MyNode = NavData->GetNodeFromPoly(MyPoly);
	TArray<FVector> Stops = TheLift->GetStops();
	if (Stops.Num() > 0 && MyNode != NULL)
	{
		// find standing position on lift
		FHitResult LiftHit;
		FVector LiftCenter = TheLift->GetComponentsBoundingBox().GetCenter();
		if (TheLift->ActorLineTraceSingle(LiftHit, LiftCenter + FVector(0.0f, 0.0f, 10000.0f), LiftCenter - FVector(0.0f, 0.0f, 10000.0f), ECC_Pawn, FCollisionQueryParams()))
		{
			const FVector LiftCenterOffset = LiftHit.Location + FVector(0.0f, 0.0f, NavData->AgentHeight * 0.5f) - TheLift->GetActorLocation();

			float BestDist = FLT_MAX;
			FVector BestStop = FVector::ZeroVector;
			// find stop closest to lift exit
			for (const FVector& NextStop : Stops)
			{
				float Dist = (NextStop - ExitLoc).Size();
				if (Dist < BestDist)
				{
					BestStop = NextStop;
					BestDist = Dist;
				}
			}

			// if lift jump, check for obstruction if bot tries to jump directly to exit
			bool bSkipInitialAirControl = false;
			if (bRequireLiftJump)
			{
				const FVector BestLiftCenter = BestStop + LiftCenterOffset;
				FHitResult Hit;
				bSkipInitialAirControl = TheLift->GetWorld()->LineTraceSingleByChannel(Hit, BestLiftCenter, BestLiftCenter + (ExitLoc - BestLiftCenter) * 0.7f, ECC_Pawn, FCollisionQueryParams()) && Hit.Normal.Z < 0.0f;
			}

			NavNodeRef LiftPoly = NavData->UTFindNearestPoly(BestStop + LiftCenterOffset, NavExtent * FVector(1.0f, 1.0f, 1.5f));
			UUTPathNode* LiftNode = NavData->GetNodeFromPoly(LiftPoly);
			if (MyPoly != INVALID_NAVNODEREF && LiftPoly != INVALID_NAVNODEREF && LiftNode != NULL)
			{
				FCapsuleSize PathSize = NavData->GetHumanPathSize();
				int32 ExtraDist = (BestStop != TheLift->GetActorLocation()) ? FMath::TruncToInt((BestStop - TheLift->GetActorLocation()).Size()) : 0; // TODO: calculate lift travel time?
				// create specs to lift
				if (!bOnlyExitPath)
				{
					UUTReachSpec_Lift* LiftSpec = NewObject<UUTReachSpec_Lift>(MyNode);
					LiftSpec->Lift = TheLift;
					LiftSpec->LiftExitLoc = ExitLoc;
					LiftSpec->LiftCenter = BestStop + LiftCenterOffset;
					LiftSpec->bEntryPath = true;
					FUTPathLink* NewLink = new(MyNode->Paths) FUTPathLink(MyNode, MyPoly, LiftNode, LiftPoly, LiftSpec, PathSize.Radius, PathSize.Height, 0);
					for (NavNodeRef StartPoly : MyNode->Polys)
					{
						NewLink->Distances.Add(NavData->CalcPolyDistance(StartPoly, MyPoly) + FMath::TruncToInt((LiftSpec->LiftCenter - ExitLoc).Size()) + ExtraDist);
					}
				}
				// create specs from all lift stops to me
				// TODO: when lifts properly have their own movable nav mesh then this will only need to be one path
				for (const FVector& NextStop : Stops)
				{
					NavNodeRef StopPoly = NavData->UTFindNearestPoly(NextStop + LiftCenterOffset, NavExtent);
					UUTPathNode* StopNode = NavData->GetNodeFromPoly(StopPoly);
					if (StopPoly != INVALID_NAVNODEREF && StopNode != NULL)
					{
						UUTReachSpec_Lift* LiftSpec = NewObject<UUTReachSpec_Lift>(StopNode);
						LiftSpec->Lift = TheLift;
						LiftSpec->LiftExitLoc = ExitLoc;
						LiftSpec->LiftCenter = NextStop + LiftCenterOffset;
						LiftSpec->bEntryPath = false;
						LiftSpec->bSkipInitialAirControl = bSkipInitialAirControl;
						FUTPathLink* NewLink = new(StopNode->Paths) FUTPathLink(StopNode, StopPoly, MyNode, MyPoly, LiftSpec, PathSize.Radius, PathSize.Height, bRequireLiftJump ? R_JUMP : 0);
						for (NavNodeRef StartPoly : StopNode->Polys)
						{
							NewLink->Distances.Add(NavData->CalcPolyDistance(StartPoly, StopPoly) + FMath::TruncToInt((LiftSpec->LiftCenter - ExitLoc).Size()) + ExtraDist);
						}
					}
				}

				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

void AUTLiftExit::AddSpecialPaths(UUTPathNode* MyNode, AUTRecastNavMesh* NavData)
{
	if (MyLift != NULL)
	{
		if (!AddLiftPathsShared(GetActorLocation(), MyLift, bLiftJump, bOnlyExit, NavData))
		{
			FMessageLog(TEXT("MapCheck")).Warning()->AddToken(FUObjectToken::Create(this))->AddToken(FTextToken::Create(NSLOCTEXT("LiftExit", "NoPaths", "Failed to generate lift paths. Check that the LiftExit is in a valid position.")));
			FMessageLog(TEXT("MapCheck")).Open();
		}
	}
}
