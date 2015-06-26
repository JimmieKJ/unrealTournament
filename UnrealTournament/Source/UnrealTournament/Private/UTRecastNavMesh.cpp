// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTRecastNavMesh.h"
#include "UTNavGraphRenderingComponent.h"
#include "RecastNavMeshGenerator.h"
#include "Runtime/Engine/Public/AI/Navigation/PImplRecastNavMesh.h"
#include "Runtime/Navmesh/Public/Detour/DetourNavMesh.h"
#include "Runtime/Navmesh/Public/Detour/DetourNavMeshQuery.h"
#include "Runtime/Navmesh/Public/DetourCrowd/DetourPathCorridor.h"
#include "Runtime/Engine/Public/AI/Navigation/RecastHelpers.h"
#include "UTPathBuilderInterface.h"
#include "UTDroppedPickup.h"
#include "UTReachSpec_HighJump.h"
#include "UTTeleporter.h"
#include "UTNavMeshRenderingComponent.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#if WITH_EDITOR
#include "EditorBuildUtils.h"
#endif
#if !UE_SERVER && WITH_EDITOR
#include "SNotificationList.h"
#include "NotificationManager.h"
#endif

void DrawDebugRoute(UWorld* World, APawn* QueryPawn, const TArray<FRouteCacheItem>& Route)
{
	for (const FRouteCacheItem& RoutePoint : Route)
	{
		DrawDebugSphere(World, RoutePoint.GetLocation(QueryPawn), 16.0f, 8, FColor(0, 255, 0));
	}
}

UUTPathBuilderInterface::UUTPathBuilderInterface(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{}

AUTRecastNavMesh::AUTRecastNavMesh(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
#if WITH_EDITOR
	if (GIsEditor && !IsTemplate())
	{
		EditorTick = new FUTNavMeshEditorTick(this);
	}
#if WITH_EDITORONLY_DATA
	NodeRenderer = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UUTNavGraphRenderingComponent>(this, FName(TEXT("NodeRenderer")));
#endif
#endif

	bDrawPolyEdges = true;
	bDrawWalkPaths = true;
	bDrawStandardJumpPaths = true;
	bDrawSpecialPaths = true;

	SpecialLinkBuildNodeIndex = INDEX_NONE;

	SizeSteps.Add(FCapsuleSize(42, 92));
	SizeSteps.Add(FCapsuleSize(42, 55));
	JumpTestThreshold2D = 2048.0f;
	ScoutClass = AUTCharacter::StaticClass();
}

#if WITH_EDITOR
AUTRecastNavMesh::~AUTRecastNavMesh()
{
	if (EditorTick != NULL)
	{
		delete EditorTick;
	}
}
#endif

UPrimitiveComponent* AUTRecastNavMesh::ConstructRenderingComponent()
{
	return NewObject<UUTNavMeshRenderingComponent>(this, TEXT("NavMeshRenderer"));
}

const dtQueryFilter* AUTRecastNavMesh::GetDefaultDetourFilter() const
{
	return ((const FRecastQueryFilter*)GetDefaultQueryFilterImpl())->GetAsDetourQueryFilter();
}

FCapsuleSize AUTRecastNavMesh::GetSteppedEdgeSize(NavNodeRef PolyRef, const struct dtPoly* PolyData, const struct dtMeshTile* TileData, const struct dtLink& Link) const
{
	const dtNavMesh* InternalMesh = GetRecastNavMeshImpl()->GetRecastMesh();
	dtNavMeshQuery& InternalQuery = GetRecastNavMeshImpl()->SharedNavQuery;

	int32 EdgeRadius = 0;
	float* Vert1 = &TileData->verts[PolyData->verts[Link.edge] * 3];
	float* Vert2 = &TileData->verts[PolyData->verts[(Link.edge + 1) % PolyData->vertCount] * 3];
	FVector UnrealVert1 = Recast2UnrealPoint(Vert1);
	FVector UnrealVert2 = Recast2UnrealPoint(Vert2);
	FCapsuleSize ActualSize;
	// the navmesh pushes out the edges by AgentRadius, so that amount should be guaranteed walkable
	// we then subtract 1 for float imprecision and because it doesn't hurt to be conservative when it comes to reachability
	ActualSize.Radius = FMath::TruncToInt((UnrealVert1 - UnrealVert2).Size() * 0.5f + AgentRadius - 1.0f);
	// recast does NOT calculate walkable height for polys; it expects you to use entirely separate navmeshes for different agent sizes
	// so we will need to calculate height manually
	float AgentHalfHeight = AgentHeight * 0.5f;
	ActualSize.Height = FMath::TruncToInt(AgentHalfHeight);
	// spot test a capsule at the center of both linked polys
	// this is not ideal but a simple sweep generates too many false hits and we have neither the functionality nor performance for a walking simulation
	// TODO: an alternative possibility if this isn't good enough is to extrude the polys upwards by the test height and overlap check that
	int32 FailedHeight = MAX_int32;
	for (const FCapsuleSize TestSize : SizeSteps)
	{
		if (TestSize.Height > ActualSize.Height && TestSize.Height < FailedHeight)
		{
			bool bFailed = false;
			for (int32 i = 0; i < 2; i++)
			{
				FVector PolyCenter = (i == 0) ? GetPolyCenter(PolyRef) : GetPolyCenter(Link.ref);
				// note: poly center is not necessarily a valid surface height
				float PolyHeight = PolyCenter.Z;
				{
					FVector RecastCenter = Unreal2RecastPoint(PolyCenter);
					if (dtStatusSucceed(InternalQuery.getPolyHeight((i == 0) ? PolyRef : Link.ref, (float*)&RecastCenter, &PolyHeight)))
					{
						PolyCenter.Z = PolyHeight;
					}
				}
				// find floor
				FHitResult Hit;
				FCollisionShape TestCapsule = FCollisionShape::MakeCapsule(AgentRadius, 1.0f);
				if (GetWorld()->SweepSingleByChannel(Hit, PolyCenter + FVector(0.0f, 0.0f, AgentHalfHeight * 0.75f), PolyCenter - FVector(0.0f, 0.0f, AgentHalfHeight), FQuat::Identity, ECC_Pawn, TestCapsule, FCollisionQueryParams()))
				{
					TestCapsule.Capsule.HalfHeight = TestSize.Height;
					// make sure capsule fits here
					FVector StartLoc = Hit.Location + FVector(0.0f, 0.0f, TestSize.Height + 1.0f);
					if (GetWorld()->OverlapBlockingTestByChannel(StartLoc, FQuat::Identity, ECC_Pawn, TestCapsule, FCollisionQueryParams()))
					{
						bFailed = true;
						break;
					}
				}
			}
			if (bFailed)
			{
				FailedHeight = FMath::Min<int32>(FailedHeight, TestSize.Height);
			}
			else
			{
				ActualSize.Height = TestSize.Height;
			}
		}
	}

	FCapsuleSize SteppedSize(0, 0);
	for (int32 i = 0; i < SizeSteps.Num(); i++)
	{
		if (ActualSize.Radius >= SizeSteps[i].Radius)
		{
			SteppedSize.Radius = FMath::Max<int32>(SteppedSize.Radius, SizeSteps[i].Radius);
		}
		if (ActualSize.Height >= SizeSteps[i].Height)
		{
			SteppedSize.Height = FMath::Max<int32>(SteppedSize.Height, SizeSteps[i].Height);
		}
	}

	return SteppedSize;
}

void AUTRecastNavMesh::SetNodeSize(UUTPathNode* Node)
{
	Node->MinPolyEdgeSize = FCapsuleSize(0, 0);
	bool bFirstEdge = true;
	const dtNavMesh* InternalMesh = GetRecastNavMeshImpl()->GetRecastMesh();
	for (NavNodeRef PolyRef : Node->Polys)
	{
		const dtPoly* PolyData = NULL;
		const dtMeshTile* TileData = NULL;
		InternalMesh->getTileAndPolyByRef(PolyRef, &TileData, &PolyData);
		if (PolyData != NULL && TileData != NULL) // well, should be impossible to be false, but it's the editor, so...
		{
			uint32 i = PolyData->firstLink;
			while (i != DT_NULL_LINK)
			{
				bool bOffMeshLink = (i >= uint32(TileData->header->maxLinkCount));
				const dtLink& Link = InternalMesh->getLink(TileData, i);
				i = Link.next;
				if (InternalMesh->isValidPolyRef(Link.ref) && !bOffMeshLink)
				{
					FCapsuleSize NewSize = GetSteppedEdgeSize(PolyRef, PolyData, TileData, Link);
					if (bFirstEdge)
					{
						Node->MinPolyEdgeSize = NewSize;
						bFirstEdge = false;
					}
					else
					{
						Node->MinPolyEdgeSize.Radius = FMath::Min<int32>(NewSize.Radius, Node->MinPolyEdgeSize.Radius);
						Node->MinPolyEdgeSize.Height = FMath::Min<int32>(NewSize.Height, Node->MinPolyEdgeSize.Height);
					}
				}
			}
		}
	}				
}

bool AUTRecastNavMesh::JumpTraceTest(FVector Start, const FVector& End, NavNodeRef StartPoly, NavNodeRef EndPoly, FCollisionShape ScoutShape, float XYSpeed, float GravityZ, float BaseJumpZ, float MaxJumpZ, float* RequiredJumpZ, float* MaxFallSpeed) const
{
	// TODO: pass this in?
	const FVector MantleStepUp = FVector(0.0f, 0.0f, GetDefault<AUTCharacter>()->UTCharacterMovement->LandingStepUp);

	const float TimeStep = ScoutShape.GetExtent().X / XYSpeed;
	const FVector TotalDiff = End - Start;
	float XYTime = TotalDiff.Size2D() / XYSpeed;
	float DefaultJumpZ = BaseJumpZ;
	float DesiredJumpZ = TotalDiff.Z / XYTime - 0.5 * GravityZ * XYTime;
	if (MaxJumpZ >= 0.0f)
	{
		DefaultJumpZ = FMath::Min<float>(DefaultJumpZ, MaxJumpZ);
		DesiredJumpZ = FMath::Min<float>(DesiredJumpZ, MaxJumpZ);
	}
	bool bLastJumpBlocked = false;
	for (int32 i = 0; i < ((DesiredJumpZ > DefaultJumpZ) ? 4 : 2); i++)
	{
		float JumpZ = 0.0f;
		if (i == 1)
		{
			JumpZ = DefaultJumpZ;
		}
		else if (i == 2)
		{
			JumpZ = DesiredJumpZ;
		}
		else if (i == 3)
		{
			// extra test using extra jumpZ if we might have failed due to needing a little extra to get over an obstacle
			// (particularly relevant for jumps that go almost straight up)
			if (bLastJumpBlocked || DesiredJumpZ >= MaxJumpZ)
			{
				continue;
			}
			JumpZ = FMath::Min<float>(DesiredJumpZ + 400.0f, MaxJumpZ); // TODO: arbitrary number that made test cases work; probably should be a factor of capsule size?
		}
		if (RequiredJumpZ != NULL)
		{
			*RequiredJumpZ = JumpZ;
		}
		// extra step-up implemented in UTCharacterMovement
		// TODO: for pathing support for non-UTCharacters (i.e. vehicles) this feature may need a reach flag
		bool bTriedMantle = JumpZ <= 0.0f;

		FVector CurrentLoc = Start;
		float ZSpeed = JumpZ;
		float TimeRemaining = 10.0f; // large number for sanity
		bLastJumpBlocked = false;
		while (TimeRemaining > 0.0f)
		{
			FBox TestBox(0);
			TestBox += CurrentLoc + ScoutShape.GetExtent();
			TestBox += CurrentLoc - ScoutShape.GetExtent();
			if (FMath::PointBoxIntersection(End, TestBox))
			{
				// made it!
				if (MaxFallSpeed != NULL)
				{
					*MaxFallSpeed = ZSpeed;
				}
				return true;
			}
			if (CurrentLoc.Z - End.Z < -ScoutShape.GetCapsuleHalfHeight() && ZSpeed < 0.0f)
			{
				// missed, fell below target
				break;
			}

			if (Cast<AKillZVolume>(FindPhysicsVolume(GetWorld(), CurrentLoc, FCollisionShape::MakeSphere(0.0f))) != NULL) // zero for perf, could change if accuracy becomes an issue
			{
				// jump crosses into kill volume
				break;
			}

			FVector Diff = End - CurrentLoc;
			FVector NewVelocity = Diff.GetSafeNormal2D() * FMath::Min<float>(Diff.Size2D() / TimeStep, XYSpeed);
			NewVelocity.Z = ZSpeed;
			ZSpeed += GravityZ * TimeStep;
			FVector NewLoc = CurrentLoc + NewVelocity * TimeStep;
			FHitResult Hit;
			if (GetWorld()->SweepSingleByChannel(Hit, CurrentLoc, NewLoc, FQuat::Identity, ECC_Pawn, ScoutShape, FCollisionQueryParams()))
			{
				// check for mantle boost
				if (!bTriedMantle && NewVelocity.Z <= -GravityZ * TimeStep && NewVelocity.Z > -300.0f)
				{
					bTriedMantle = true;
					FVector TestStart = CurrentLoc;
					if (Hit.bStartPenetrating)
					{
						TestStart -= Diff.GetSafeNormal2D();
					}
					if (!GetWorld()->SweepTestByChannel(TestStart, TestStart + MantleStepUp, FQuat::Identity, ECC_Pawn, ScoutShape, FCollisionQueryParams()))
					{
						FHitResult MantleHit;
						if (!GetWorld()->SweepSingleByChannel(MantleHit, TestStart + MantleStepUp, NewLoc + MantleStepUp, FQuat::Identity, ECC_Pawn, ScoutShape, FCollisionQueryParams()))
						{
							Hit.Time = 1.0f;
							Hit.Location = NewLoc + MantleStepUp;
						}
					}
				}
				if (Hit.Time > KINDA_SMALL_NUMBER && CurrentLoc != Hit.Location)
				{
					// we got some movement so take it
					CurrentLoc = Hit.Location;
				}
				if (Hit.Time < 1.0f)
				{
					// try Z only
					CurrentLoc -= Diff.GetSafeNormal2D(); // avoid float precision penetration issues
					if ((NewVelocity.X == 0.0f && NewVelocity.Y == 0.0f) || GetWorld()->SweepSingleByChannel(Hit, CurrentLoc, FVector(CurrentLoc.X, CurrentLoc.Y, NewLoc.Z), FQuat::Identity, ECC_Pawn, ScoutShape, FCollisionQueryParams()))
					{
						if (NewVelocity.Z > 0.0f && Hit.Normal.Z < -0.99f)
						{
							// bumped head on ceiling
							ZSpeed = FMath::Min<float>(ZSpeed, 0.0f);
						}
						else
						{
							// note: the downward offset for FindNearestPoly() is needed to get consistent results when slightly off the navmesh and on a slanted surface
							if (EndPoly != INVALID_NAVNODEREF && EndPoly == FindNearestPoly(CurrentLoc - FVector(0.0f, 0.0f, ScoutShape.GetExtent().Z * 0.5f), ScoutShape.GetExtent()))
							{
								// make sure really on poly, since check is a bounding box
								FVector ClosestPt = FVector::ZeroVector;
								if (GetClosestPointOnPoly(EndPoly, CurrentLoc, ClosestPt) && (CurrentLoc - ClosestPt).Size2D() < ScoutShape.GetCapsuleRadius() * 1.1f)
								{
									// if we made it to the poly we got close enough
									if (MaxFallSpeed != NULL)
									{
										*MaxFallSpeed = ZSpeed;
									}
									return true;
								}
							}
							// give up, nowhere to go
							bLastJumpBlocked = true;
							break;
						}
					}
					else
					{
						CurrentLoc.Z = NewLoc.Z;
					}
				}
			}
			else
			{
				CurrentLoc = NewLoc;
			}
			TimeRemaining -= TimeStep;
		}
	}

	return false;
}

bool AUTRecastNavMesh::OnlyJumpReachable(APawn* Scout, FVector Start, const FVector& End, NavNodeRef StartPoly, NavNodeRef EndPoly, float MaxJumpZ, float* RequiredJumpZ, float* MaxFallSpeed) const
{
	const FVector OriginalStart = Start;
	ACharacter* Char = Cast<ACharacter>(Scout);
	if (Char == NULL || Char->GetCharacterMovement() == NULL || Char->GetCapsuleComponent() == NULL)
	{
		// TODO: what about jumping vehicles?
		return false;
	}
	else if ( !GetWorld()->FindTeleportSpot(Char, Start, (End - Start).GetSafeNormal2D().Rotation())
		// TODO: this is a bit of a workaround for navmesh generating walkable polys inside some solid objects, like blocking volumes
		//		reject jump sources that require significant XY movement to find open space, since it might have teleported us on the other side of an obstruction that should block a jump
			|| (Start - OriginalStart).Size2D() > Char->GetCapsuleComponent()->GetUnscaledCapsuleRadius() * 2.5f )
	{
		// can't fit at start location
		return false;
	}
	else
	{
		Start.Z += 0.5f; // FindTeleportSpot() will return Z locations on flat floors that fail an XY-only trace, probably due to float precision fails

		FCollisionShape ScoutShape = FCollisionShape::MakeCapsule(Char->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), Char->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
		float GravityZ = GetWorld()->GetDefaultGravityZ(); // FIXME: query physics volumes (always, or just at start?)

		if (StartPoly == INVALID_NAVNODEREF)
		{
			StartPoly = FindNearestPoly(Start, ScoutShape.GetExtent());
		}
		if (EndPoly == INVALID_NAVNODEREF)
		{
			EndPoly = FindNearestPoly(End, ScoutShape.GetExtent());
		}
	
		// move start position to edge of walkable surface
		{
			FVector MoveSlice = (End - Start).GetSafeNormal2D() * ScoutShape.GetCapsuleRadius();
			FHitResult Hit;
			bool bAnyHit = false;
			while (!GetWorld()->SweepSingleByChannel(Hit, Start, Start + FVector(0.0f, 0.0f, AgentMaxStepHeight), FQuat::Identity, ECC_Pawn, ScoutShape, FCollisionQueryParams()) &&
					!GetWorld()->SweepSingleByChannel(Hit, Start + FVector(0.0f, 0.0f, AgentMaxStepHeight), Start + MoveSlice + FVector(0.0f, 0.0f, AgentMaxStepHeight), FQuat::Identity, ECC_Pawn, ScoutShape, FCollisionQueryParams()) &&
					GetWorld()->SweepSingleByChannel(Hit, Start + MoveSlice + FVector(0.0f, 0.0f, AgentMaxStepHeight), Start + MoveSlice - FVector(0.0f, 0.0f, 50.0f), FQuat::Identity, ECC_Pawn, ScoutShape, FCollisionQueryParams()) &&
					!Hit.bStartPenetrating )
			{
				bAnyHit = true;
				Start = Hit.Location + Hit.Normal;
				if ((End - Start).Size2D() <= MoveSlice.Size2D())
				{
					// either reached target without jumping, or stuck above/below it
					return false;
				}
				NavNodeRef NewStartPoly = FindNearestPoly(Start, ScoutShape.GetExtent());
				FVector ClosestPt = FVector::ZeroVector;
				// FindNearestPoly() uses a bounding box check and so is inaccurate. Make sure we're really in another poly and not just close. Important for small/short jumps.
				if (NewStartPoly != INVALID_NAVNODEREF && NewStartPoly != StartPoly && GetClosestPointOnPoly(NewStartPoly, Start, ClosestPt) && (Start - ClosestPt).Size2D() < ScoutShape.GetCapsuleRadius())
				{
					// made it to another walk reachable poly, test jump from there instead
					return false;
				}
			}
			if (bAnyHit && !GetWorld()->SweepTestByChannel(Start, Start + MoveSlice * 0.1f, FQuat::Identity, ECC_Pawn, ScoutShape, FCollisionQueryParams()))
			{
				// this slight extra nudge is meant to make sure minor floating point discrepancies don't cause the zero JumpZ fall test to fail unnecessarily
				Start += MoveSlice * 0.1f;
			}
		}

		return JumpTraceTest(Start, End, StartPoly, EndPoly, ScoutShape, Char->GetCharacterMovement()->MaxWalkSpeed, GravityZ, (DefaultEffectiveJumpZ != 0.0f) ? DefaultEffectiveJumpZ : (Char->GetCharacterMovement()->JumpZVelocity * 0.95f), MaxJumpZ, RequiredJumpZ, MaxFallSpeed);
	}
}

/** returns true if the specified location is in a pain zone/volume */
static bool IsInPain(UWorld* World, const FVector& TestLoc)
{
	TArray<FOverlapResult> Hits;
	World->OverlapMultiByChannel(Hits, TestLoc, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(0.f), FComponentQueryParams());
	
	for (const FOverlapResult& Result : Hits)
	{
		if (Cast<APainCausingVolume>(Result.GetActor()) != NULL)
		{
			return true;
		}
	}

	return false;
}

int32 AUTRecastNavMesh::CalcPolyDistance(NavNodeRef StartPoly, NavNodeRef EndPoly)
{
	dtNavMeshQuery& InternalQuery = GetRecastNavMeshImpl()->SharedNavQuery;

	float Distance = 0.0f;

	FVector Start = Unreal2RecastPoint(GetPolyCenter(StartPoly));
	FVector End = Unreal2RecastPoint(GetPolyCenter(EndPoly));
	float RecastStart[3] = { Start.X, Start.Y, Start.Z };
	float RecastEnd[3] = { End.X, End.Y, End.Z };
	float HitNormal[3];
	float HitTime = 0.0f;
	if (dtStatusSucceed(InternalQuery.raycast(StartPoly, RecastStart, RecastEnd, GetDefaultDetourFilter(), &HitTime, HitNormal, NULL, NULL, 0)) && HitTime >= 1.0f)
	{
		// raycast succeeded, so use line distance
		Distance = (End - Start).Size();
	}
	else
	{
		// use pathing distance instead
		int32 MaxNodes = 100;
		NavNodeRef* Path = (NavNodeRef*)FMemory_Alloca(MaxNodes * sizeof(NavNodeRef));
		int32 NodeCount = 0;
		dtQueryResult PathData;
		dtStatus PathResult = InternalQuery.findPath(StartPoly, EndPoly, RecastStart, RecastEnd, GetDefaultDetourFilter(), PathData, &Distance);
		if (!dtStatusSucceed(PathResult) || PathResult & DT_PARTIAL_RESULT)
		{
			// should never happen
			UE_LOG(UT, Warning, TEXT("FindPath failure precalculating node link distances"));
			Distance = (End - Start).Size();
		}
	}

	return FMath::TruncToInt(Distance);
}

FVector AUTRecastNavMesh::GetPolySurfaceCenter(NavNodeRef PolyID) const
{
	FVector PolyCenter;
	if (!GetPolyCenter(PolyID, PolyCenter))
	{
		return FVector::ZeroVector;
	}
	else
	{
		float PolyHeight = PolyCenter.Z;
		{
			// this gets called early enough that superclass may not have initialized the query object
			GetRecastNavMeshImpl()->SharedNavQuery.init(GetRecastNavMeshImpl()->DetourNavMesh, RECAST_MAX_SEARCH_NODES);

			dtNavMeshQuery& InternalQuery = GetRecastNavMeshImpl()->SharedNavQuery;
			FVector RecastCenter = Unreal2RecastPoint(PolyCenter);
			if (dtStatusSucceed(InternalQuery.getPolyHeight(PolyID, (float*)&RecastCenter, &PolyHeight)))
			{
				PolyCenter.Z = PolyHeight;
			}
		}
		return PolyCenter;
	}
}

FCapsuleSize AUTRecastNavMesh::GetHumanPathSize() const
{
	if (ScoutClass != NULL)
	{
		FCapsuleSize ActualSize(FMath::TruncToInt(ScoutClass.GetDefaultObject()->GetCapsuleComponent()->GetUnscaledCapsuleRadius()), FMath::TruncToInt(ScoutClass.GetDefaultObject()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));

		FCapsuleSize SteppedSize(0, 0);
		FCapsuleSize MinStepSize(0, 0);
		for (int32 i = 0; i < SizeSteps.Num(); i++)
		{
			if (ActualSize.Radius >= SizeSteps[i].Radius)
			{
				SteppedSize.Radius = FMath::Max<int32>(SteppedSize.Radius, SizeSteps[i].Radius);
			}
			if (ActualSize.Height >= SizeSteps[i].Height)
			{
				SteppedSize.Height = FMath::Max<int32>(SteppedSize.Height, SizeSteps[i].Height);
			}
			MinStepSize.Radius = (MinStepSize.Radius == 0) ? SizeSteps[i].Radius : FMath::Min<int32>(MinStepSize.Radius, SizeSteps[i].Radius);
			MinStepSize.Height = (MinStepSize.Height == 0) ? SizeSteps[i].Height : FMath::Min<int32>(MinStepSize.Height, SizeSteps[i].Height);
		}
		// make sure we ended with a valid value in SizeSteps
		if (SteppedSize.Radius == 0)
		{
			SteppedSize.Radius = MinStepSize.Radius;
		}
		if (SteppedSize.Height == 0)
		{
			SteppedSize.Height = MinStepSize.Height;
		}
		return SteppedSize;
	}
	else
	{
		return FCapsuleSize(FMath::TruncToInt(AgentRadius), FMath::TruncToInt(AgentHeight * 0.5f));
	}
}

FVector AUTRecastNavMesh::GetPOIExtent(AActor* POI) const
{
	// enforce a minimum extent for checks
	// this handles cases where the POI doesn't define any colliding primitives (i.e. just a point in space that AI should be aware of)
	FCapsuleSize HumanSize = GetHumanPathSize();
	FVector MinPOIExtent(HumanSize.Radius, HumanSize.Radius, HumanSize.Height);
	if (POI == NULL)
	{
		return MinPOIExtent;
	}
	else
	{
		FVector POIExtent = POI->GetSimpleCollisionCylinderExtent();
		POIExtent.X = FMath::Max<float>(POIExtent.X, MinPOIExtent.X);
		POIExtent.Y = POIExtent.X;
		POIExtent.Z = FMath::Max<float>(POIExtent.Z, MinPOIExtent.Z);
		return POIExtent;
	}
}

void AUTRecastNavMesh::BuildNodeNetwork()
{
#if WITH_EDITORONLY_DATA
	LastNodeBuildDuration = 0.0;
	FSecondsCounter TimeCounter(LastNodeBuildDuration);
#endif

	FMessageLog MapCheckLog(TEXT("MapCheck"));

	struct FQueryMarker
	{
		AUTRecastNavMesh* Mesh;

		FQueryMarker(AUTRecastNavMesh* InMesh)
			: Mesh(InMesh)
		{
			Mesh->BeginBatchQuery();
		}
		~FQueryMarker()
		{
			Mesh->FinishBatchQuery();
		}
	} QueryMark(this);

	DeletePaths();

	const dtNavMesh* InternalMesh = GetRecastNavMeshImpl()->GetRecastMesh();
	dtNavMeshQuery& InternalQuery = GetRecastNavMeshImpl()->SharedNavQuery;

	// make sure generation params are in the list
	SizeSteps.AddUnique(FCapsuleSize(FMath::TruncToInt(AgentRadius), FMath::TruncToInt(AgentMaxHeight)));

	// list of IUTPathBuilderInterface implementing Actors that don't want to be added as POIs but still want path building callbacks
	TArray<IUTPathBuilderInterface*> NonPOIBuilders;

	for (FActorIterator It(GetWorld()); It; ++It)
	{
		IUTPathBuilderInterface* Builder = Cast<IUTPathBuilderInterface>(*It);
		if (Builder != NULL)
		{
			if (!Builder->IsPOI())
			{
				NonPOIBuilders.Add(Builder);
			}
			else
			{
				if (Builder->IsDestinationOnly())
				{
					// we need to make sure to grab all encompassing polys into the new PathNode as it will have special properties
					// and Recast may have split the polys inside the extent of the POI
					const FVector UnrealCenter = It->GetActorLocation();
					const FVector RecastCenter = Unreal2RecastPoint(UnrealCenter);
					FVector RecastExtent = GetPOIExtent(*It) * FVector(0.9f, 0.9f, 1.0f); // hack: trying to minimize grabbing a small portion of adjacent large polys
					RecastExtent = FVector(RecastExtent[0], RecastExtent[2], RecastExtent[1]);
					TArray<NavNodeRef> FinalPolys;
					{
						NavNodeRef ResultPolys[10];
						int32 NumPolys = 0;
						InternalQuery.queryPolygons((float*)&RecastCenter, (float*)&RecastExtent, GetDefaultDetourFilter(), ResultPolys, &NumPolys, ARRAY_COUNT(ResultPolys));
						FinalPolys.Reserve(NumPolys);
						// remove polygons that do not truly intersect the bounding cylinder, since queryPolygons() is an AABB test
						for (int32 i = 0; i < NumPolys; i++)
						{
							FVector ClosestPt = FVector::ZeroVector;
							if (GetClosestPointOnPoly(ResultPolys[i], UnrealCenter, ClosestPt) && (UnrealCenter - ClosestPt).Size2D() < RecastExtent.X)
							{
								FinalPolys.Add(ResultPolys[i]);
							}
						}
					}
					if (FinalPolys.Num() == 0)
					{
						MapCheckLog.Warning()->AddToken(FUObjectToken::Create(*It))->AddToken(FTextToken::Create(NSLOCTEXT("UTRecastNavMesh", "UnlinkedPOI", "Navigation relevant Actor couldn't be linked to the navmesh. Check that it is in a valid position and close enough to the ground.")));
					}
					else
					{
						UUTPathNode* Node = NewObject<UUTPathNode>(this);
						Node->bDestinationOnly = true;
						Node->POIs.Add(*It);
						PathNodes.Add(Node);
						for (NavNodeRef Poly : FinalPolys)
						{
							UUTPathNode* OldNode = PolyToNode.FindRef(Poly);
							if (OldNode != Node) // can happen if prior merge moved further entries from FinalPolys
							{
								if (OldNode != NULL)
								{
									// merge
									Node->Polys += OldNode->Polys;
									for (NavNodeRef OtherPoly : OldNode->Polys)
									{
										PolyToNode.Add(OtherPoly, Node);
									}
									OldNode->Polys.Empty();
									Node->POIs += OldNode->POIs;
									OldNode->POIs.Empty();
									PathNodes.Remove(OldNode);
								}
								else
								{
									PolyToNode.Add(Poly, Node);
									Node->Polys.Add(Poly);
								}
							}
						}
						SetNodeSize(Node);
					}
				}
				else
				{
					NavNodeRef Poly = FindNearestPoly(It->GetActorLocation(), GetPOIExtent(*It));
					if (Poly != INVALID_NAVNODEREF)
					{
						// find node for this tile or create it
						UUTPathNode* Node = PolyToNode.FindRef(Poly);
						if (Node == NULL)
						{
							Node = NewObject<UUTPathNode>(this);
							PathNodes.Add(Node);
							PolyToNode.Add(Poly, Node);
							Node->Polys.Add(Poly);
							SetNodeSize(Node);
						}
						Node->POIs.Add(*It);
					}
					else
					{
						MapCheckLog.Warning()->AddToken(FUObjectToken::Create(*It))->AddToken(FTextToken::Create(NSLOCTEXT("UTRecastNavMesh", "UnlinkedPOI", "Navigation relevant Actor couldn't be linked to the navmesh. Check that it is in a valid position and close enough to the ground.")));
					}
				}
			}
		}
	}

	// expand the nodes into adjacent tiles
	// if compatible reachability, merge into the node's tiles list, otherwise generate a new node and path link
	// do this until we have accounted for the entire standard mesh
	for (bool bNodesAdded = true; bNodesAdded;)
	{
		bNodesAdded = false;
		for (bool bAnyNodeExpanded = true; bAnyNodeExpanded;)
		{
			bAnyNodeExpanded = false;
			for (UUTPathNode* Node : PathNodes)
			{
				if (Node->bDestinationOnly)
				{
					continue;
				}
				checkSlow(Node->Polys.Num() > 0);
				// mirrored array because we will expand the array as we're operating but only want to do one level of expansion per loop
				TArray<NavNodeRef> Polys = Node->Polys;
				for (NavNodeRef PolyRef : Polys)
				{
					const dtPoly* PolyData = NULL;
					const dtMeshTile* TileData = NULL;
					InternalMesh->getTileAndPolyByRef(PolyRef, &TileData, &PolyData);
					if (PolyData != NULL && TileData != NULL) // well, should be impossible to be false, but it's the editor, so...
					{
						uint32 i = PolyData->firstLink;
						while (i != DT_NULL_LINK)
						{
							bool bOffMeshLink = (i >= uint32(TileData->header->maxLinkCount));
							const dtLink& Link = InternalMesh->getLink(TileData, i);
							i = Link.next;
							if (InternalMesh->isValidPolyRef(Link.ref))
							{
								FCapsuleSize OtherSize = GetSteppedEdgeSize(PolyRef, PolyData, TileData, Link);
								// TODO: any other checks needed?
								// TODO: max distance between nodes? Maybe based on pct of mesh size?
								if (!bOffMeshLink && OtherSize == Node->MinPolyEdgeSize && /*TileData->header->walkableClimb == DestTile->header->walkableClimb &&*/ !PolyToNode.Contains(Link.ref))
								{
									ensure(!Node->Polys.Contains(Link.ref)); // should have ended up below if this is the case
									Node->Polys.Add(Link.ref);
									PolyToNode.Add(Link.ref, Node);
									bAnyNodeExpanded = true;
								}
								else
								{
									// create a path link to the neighboring tile
									UUTPathNode* DestNode = PolyToNode.FindRef(Link.ref);
									if (DestNode != NULL && DestNode != Node)
									{
										bool bFound = false;
										for (const FUTPathLink& UTPath : Node->Paths)
										{
											if (UTPath.End == DestNode)
											{
												if (UTPath.EndPoly != Link.ref)
												{
													// if there are multiple polys connecting these two nodes, split the destination node so there is only one connection point between any two nodes
													// this is important for pathfinding as otherwise the correct path to use depends on later points which would make pathing very complex
													UUTPathNode* NewNode = NewObject<UUTPathNode>(this);
													NewNode->MinPolyEdgeSize = DestNode->MinPolyEdgeSize;
													DestNode->Polys.Remove(Link.ref);
													NewNode->Polys.Add(Link.ref);
													PolyToNode.Add(Link.ref, NewNode);
													// TODO: should we claim more than one poly from DestNode?

													// move over POIs that are inside the split polygons
													for (int32 POIIndex = 0; POIIndex < DestNode->POIs.Num(); POIIndex++)
													{
														if (NewNode->Polys.Contains(FindNearestPoly(DestNode->POIs[POIIndex]->GetActorLocation(), GetPOIExtent(DestNode->POIs[POIIndex].Get()))))
														{
															NewNode->POIs.Add(DestNode->POIs[POIIndex]);
															DestNode->POIs.RemoveAt(POIIndex--);
														}
													}
													// if there was a path from Dest -> source, redirect it to Dest -> NewNode
													for (int32 LinkIndex = 0; LinkIndex < DestNode->Paths.Num(); LinkIndex++)
													{
														if (DestNode->Paths[LinkIndex].End == Node && DestNode->Paths[LinkIndex].EndPoly == PolyRef)
														{
															// find poly in DestNode that links to NewNode
															NavNodeRef NewSrcPoly = INVALID_NAVNODEREF;
															for (NavNodeRef DestPolyRef : DestNode->Polys)
															{
																const dtPoly* TestPolyData = NULL;
																const dtMeshTile* TestTileData = NULL;
																InternalMesh->getTileAndPolyByRef(PolyRef, &TestTileData, &TestPolyData);
																if (PolyData != NULL && TileData != NULL)
																{
																	uint32 j = PolyData->firstLink;
																	while (j != DT_NULL_LINK)
																	{
																		const dtLink& TestLink = InternalMesh->getLink(TileData, j);
																		j = TestLink.next;
																		if (TestLink.ref == Link.ref)
																		{
																			NewSrcPoly = DestPolyRef;
																			break;
																		}
																	}
																}
																if (NewSrcPoly != INVALID_NAVNODEREF)
																{
																	break;
																}
															}
															if (NewSrcPoly != INVALID_NAVNODEREF)
															{
																DestNode->Paths[LinkIndex] = FUTPathLink(DestNode, NewSrcPoly, NewNode, Link.ref, NULL, FMath::Min<int32>(NewNode->MinPolyEdgeSize.Radius, DestNode->MinPolyEdgeSize.Radius), FMath::Min<int32>(NewNode->MinPolyEdgeSize.Height, DestNode->MinPolyEdgeSize.Height), 0);
															}
															else
															{
																// shouldn't happen
																UE_LOG(UT, Warning, TEXT("NODE BUILDER: Link error trying to split PathNodes"));
															}
															break;
														}
													}
													// redirect other nodes' paths that point to polys that have been redirected
													for (UUTPathNode* OtherNode : PathNodes)
													{
														for (FUTPathLink& OtherLink : OtherNode->Paths)
														{
															if (NewNode->Polys.Contains(OtherLink.EndPoly))
															{
																OtherLink.End = NewNode;
															}
														}
													}

													DestNode = NewNode;
													PathNodes.Add(NewNode);
												}
												else
												{
													bFound = true;
												}
												break;
											}
										}
										if (!bFound)
										{
											new(Node->Paths) FUTPathLink(Node, PolyRef, DestNode, Link.ref, NULL, FMath::Min<int32>(Node->MinPolyEdgeSize.Radius, DestNode->MinPolyEdgeSize.Radius), FMath::Min<int32>(Node->MinPolyEdgeSize.Height, DestNode->MinPolyEdgeSize.Height), 0);
											// note: reverse path will be created when we iterate that node
										}
									}
								}
							}
						}
					}
				}
			}
		}
		// search for islands and add a node, then repeat the expansion/linking
		for (int32 i = InternalMesh->getMaxTiles() - 1; i >= 0; i--)
		{
			const dtMeshTile* TileData = InternalMesh->getTile(i);
			if (TileData != NULL && TileData->header != NULL && TileData->dataSize > 0) // indicates a real tile, not on the free list
			{
				for (int32 j = 0; j < TileData->header->polyCount; j++)
				{
					NavNodeRef PolyRef = InternalMesh->encodePolyId(TileData->salt, i, j);
					if (!PolyToNode.Contains(PolyRef))
					{
						UUTPathNode* Node = NewObject<UUTPathNode>(this);
						PathNodes.Add(Node);
						PolyToNode.Add(PolyRef, Node);
						Node->Polys.Add(PolyRef);
						SetNodeSize(Node);
						bNodesAdded = true;
						break; // TODO: not a good plan, way too slow
					}
				}
				if (bNodesAdded)
				{
					break; // TODO: not a good plan, way too slow
				}
			}
		}
	}

	// sanity check our data
#if UE_BUILD_DEBUG
	for (UUTPathNode* Node : PathNodes)
	{
		for (NavNodeRef PolyRef : Node->Polys)
		{
			check(PolyToNode.FindRef(PolyRef) == Node);
		}
		for (const FUTPathLink& Link : Node->Paths)
		{
			check(Link.End->Polys.Contains(Link.EndPoly));
		}
		for (TWeakObjectPtr<AActor> POI : Node->POIs)
		{
			check(Node->Polys.Contains(FindNearestPoly(POI->GetActorLocation(), GetPOIExtent(POI.Get()))));
		}
	}
#endif

	// calculate path distances
	// this could be optimized for build time and/or memory use by considering only edge polys
	for (UUTPathNode* Node : PathNodes)
	{
		for (FUTPathLink& Link : Node->Paths)
		{
			if (Link.Spec == NULL) // TODO: call ReachSpec function to precalculate distance?
			{
				Link.Distances.Reset(); // just in case
				for (NavNodeRef PolyRef : Node->Polys)
				{
					Link.Distances.Add(CalcPolyDistance(PolyRef, Link.EndPoly));
				}
			}
		}
	}

	// calculate node locations
	for (UUTPathNode* Node : PathNodes)
	{
		// get center of bounding box encompassing all the polys
		FBox NodeBB(0);
		TArray<FVector> PolyCenters;
		for (NavNodeRef PolyRef : Node->Polys)
		{
			const dtPoly* PolyData = NULL;
			const dtMeshTile* TileData = NULL;
			InternalMesh->getTileAndPolyByRef(PolyRef, &TileData, &PolyData);
			if (PolyData != NULL && TileData != NULL)
			{
				FVector VertSum(FVector::ZeroVector);
				for (int32 i = 0; i < PolyData->vertCount; i++)
				{
					FVector VertLoc = Recast2UnrealPoint(&TileData->verts[PolyData->verts[i] * 3]);
					VertSum += VertLoc;
					NodeBB += VertLoc;
				}
				PolyCenters.Add(VertSum / float(PolyData->vertCount));
			}
		}
		FVector BBCenter = NodeBB.GetCenter();

		// use the center of the tile closest to the node's BB center as the node's Location
		float BestDist = FLT_MAX;
		for (const FVector& Center : PolyCenters)
		{
			float Dist = (Center - BBCenter).SizeSquared();
			if (Dist < BestDist)
			{
				Node->Location = Center;
				BestDist = Dist;
			}
		}
	}

	// query POIs for special paths
	for (UUTPathNode* Node : PathNodes)
	{
		for (TWeakObjectPtr<AActor> POI : Node->POIs)
		{
			IUTPathBuilderInterface* Builder = Cast<IUTPathBuilderInterface>(POI.Get());
			if (Builder != NULL)
			{
				Builder->AddSpecialPaths(Node, this);
			}
		}
	}
	for (IUTPathBuilderInterface* Builder : NonPOIBuilders)
	{
		Builder->AddSpecialPaths(NULL, this);
	}

	// start jump path generation (spread over a number of ticks)
	SpecialLinkBuildNodeIndex = 0;
	SpecialLinkBuildPass = 0;
	if (bUserRequestedBuild)
	{
		BuildSpecialLinks(MAX_int32);
	}

	// assemble the list of ReachSpecs for GC purposes
	for (UUTPathNode* Node : PathNodes)
	{
		for (const FUTPathLink& Link : Node->Paths)
		{
			if (Link.Spec.IsValid())
			{
				AllReachSpecs.Add(Link.Spec.Get());
			}
		}
	}

	if (MapCheckLog.NumMessages(EMessageSeverity::Warning) > 0)
	{
		MapCheckLog.Open();
	}

#if WITH_EDITORONLY_DATA
	if (NodeRenderer != NULL)
	{
		// we need to update the rendering immediately since a new build could be started and wipe the data before the end of the frame (e.g. if user is dragging things in the editor)
		NodeRenderer->RecreateRenderState_Concurrent();
	}
	RequestDrawingUpdate();
#endif
}

bool AUTRecastNavMesh::IsValidJumpPoint(const FVector& TestPolyCenter) const
{
	// skip if poly is under the world's KillZ
	// ideally LD wouldn't have put the nav bounds this low anyway
	if (TestPolyCenter.Z <= GetWorld()->GetWorldSettings()->KillZ)
	{
		return false;
	}
	else
	{
		// TODO: workaround for Recast generating polys (and therefore nodes) in the internal geometry of some meshes
		//		poly is not in walkable space
		TArray<FOverlapResult> Overlaps;
		if (GetWorld()->OverlapMultiByChannel(Overlaps, TestPolyCenter + FVector(0.0f, 0.0f, AgentHeight * 0.5f), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(1.0f), FCollisionQueryParams(), WorldResponseParams))
		{
			return false;
		}
		else
		{
			// check that it's not in a kill volume
			// TODO: ideally we could make KillZVolumes block navigation despite not having blocking collision
			for (const FOverlapResult& TestHit : Overlaps)
			{
				if (Cast<AKillZVolume>(TestHit.Actor.Get()) != NULL)
				{
					return false;
				}
			}

			return true;
		}
	}
}

void AUTRecastNavMesh::BuildSpecialLinks(int32 NumToProcess)
{
	if (SpecialLinkBuildNodeIndex >= 0)
	{
#if WITH_EDITORONLY_DATA
		FSecondsCounter TimeCounter(LastNodeBuildDuration);
#endif

		if (ScoutClass == NULL || ScoutClass.GetDefaultObject()->GetCharacterMovement() == NULL)
		{
			// can't build if no scout to figure out jumps
			SpecialLinkBuildNodeIndex = INDEX_NONE;
			SpecialLinkBuildPass = 0;
		}
		else
		{
			auto CalcJumpPathDistance = [this](FUTPathLink& Link)
			{
				FVector Center = GetPolyCenter(Link.EndPoly);
				if (Link.AdditionalEndPolys.Num() > 0)
				{
					for (NavNodeRef ExtraPoly : Link.AdditionalEndPolys)
					{
						Center += GetPolyCenter(ExtraPoly);
					}
					Center /= (Link.AdditionalEndPolys.Num() + 1);

					// change core end to the one closest to the center we found
					TArray<NavNodeRef> AllEndPolys(Link.AdditionalEndPolys);
					AllEndPolys.Add(Link.EndPoly);

					int32 Best = INDEX_NONE;
					float BestDistSq = FLT_MAX;
					for (int32 i = 0; i < AllEndPolys.Num(); i++)
					{
						float DistSq = (GetPolyCenter(AllEndPolys[i]) - Center).SizeSquared();
						if (DistSq < BestDistSq)
						{
							Best = i;
							BestDistSq = DistSq;
						}
					}
					if (AllEndPolys[Best] != Link.EndPoly)
					{
						AllEndPolys.RemoveAt(Best);
						Link.EndPoly = AllEndPolys[Best];
						Link.AdditionalEndPolys = AllEndPolys;
					}
				}

				for (NavNodeRef SrcPolyRef : Link.Start->Polys)
				{
					Link.Distances.Add(CalcPolyDistance(SrcPolyRef, Link.StartEdgePoly) + FMath::TruncToInt((Center - GetPolyCenter(Link.StartEdgePoly)).Size()));
				}
			};

			dtNavMeshQuery& InternalQuery = GetRecastNavMeshImpl()->SharedNavQuery;

			FActorSpawnParameters SpawnParams;
			SpawnParams.bNoCollisionFail = true;
			ACharacter* DefaultScout = ScoutClass.GetDefaultObject();
			float BaseJumpZ = DefaultEffectiveJumpZ;
			if (BaseJumpZ == 0.0 && DefaultScout->GetCharacterMovement() != NULL)
			{
				BaseJumpZ = DefaultScout->GetCharacterMovement()->JumpZVelocity * 0.95f; // slightly less so we can be more confident in the jumps
			}

			float MoveSpeed = ScoutClass.GetDefaultObject()->GetCharacterMovement()->MaxWalkSpeed;
			FVector HeightAdjust(0.0f, 0.0f, AgentHeight * 0.5f);

			FCapsuleSize PathSize = GetHumanPathSize();

			// HACK: avoid placing jumps to extra lift navmesh geometry created to work around lack of movable navmeshes
			TArray<FVector> LiftHackLocs;
			for (TActorIterator<AUTLift> It(GetWorld()); It; ++It)
			{
				TArray<FVector> Stops = It->GetStops();
				if (Stops.Num() > 1)
				{
					LiftHackLocs.Add(Stops.Last());
				}
			}

			const bool bDisplayProgressDialog = (GIsEditor && NumToProcess >= PathNodes.Num());
			if (bDisplayProgressDialog)
			{
				GWarn->BeginSlowTask(NSLOCTEXT("UT", "BuildSpecialLinks", "Building UT Paths"), true, false);
			}

			if (SpecialLinkBuildPass == 0)
			{
				for (; SpecialLinkBuildNodeIndex < PathNodes.Num() && NumToProcess > 0; SpecialLinkBuildNodeIndex++, NumToProcess--)
				{
					UUTPathNode* Node = PathNodes[SpecialLinkBuildNodeIndex];
					if (Node->bDestinationOnly)
					{
						continue;
					}
					if (bDisplayProgressDialog)
					{
						GWarn->UpdateProgress(SpecialLinkBuildNodeIndex, PathNodes.Num() * 2);
					}
					for (NavNodeRef PolyRef : Node->Polys)
					{
						FVector PolyCenter = GetPolyCenter(PolyRef);

						if (!IsValidJumpPoint(PolyCenter))
						{
							continue;
						}

						float SegmentVerts[36];
						int32 NumSegments = 0;
						InternalQuery.getPolyWallSegments(PolyRef, GetDefaultDetourFilter(), SegmentVerts, NULL, &NumSegments, 6);
						for (int32 i = 0; i < NumSegments; i++)
						{
							// wall
							FVector WallCenter = (Recast2UnrealPoint(&SegmentVerts[(i * 2) * 3]) + Recast2UnrealPoint(&SegmentVerts[(i * 2 + 1) * 3])) * 0.5f + HeightAdjust;

							// search for polys to try testing jump reach to
							// TODO: is there a better method to get polys in a 2D circle...?
							for (TMap<NavNodeRef, UUTPathNode*>::TConstIterator It(PolyToNode); It; ++It)
							{
								if (It.Value() != Node)
								{
									FVector TestLoc = GetPolyCenter(It.Key());
									// if there's a valid jump more than ~45 degrees off the direction to the wall there is probably another poly wall in this polygon that will handle it
									// we add a little leeway to ~50 degrees since we're only testing the wall center and not the whole thing
									// FIXME: replace NumSegments hack and threshold dot test with loop that uses wall with best angle
									if ((TestLoc - WallCenter).Size2D() < JumpTestThreshold2D && (NumSegments == 1 || ((TestLoc - WallCenter).GetSafeNormal2D() | (WallCenter - PolyCenter).GetSafeNormal2D()) > 0.64f) &&
										!IsInPain(GetWorld(), TestLoc)) // TODO: need to allow pain volumes (flag node)
									{
										if (!IsValidJumpPoint(TestLoc))
										{
											continue;
										}
										// make sure not directly walk reachable
										bool bWalkReachable = true;
										{
											FVector RecastStartVect = Unreal2RecastPoint(PolyCenter); // note, not wall loc so we're less likely to barely catch on corners
											float RecastStart[3] = { RecastStartVect.X, RecastStartVect.Y, RecastStartVect.Z };
											FVector RecastEndVect = Unreal2RecastPoint(TestLoc);
											float RecastEnd[3] = { RecastEndVect.X, RecastEndVect.Y, RecastEndVect.Z };

											float HitTime = 1.0f;
											float HitNormal[3];
											if (dtStatusSucceed(InternalQuery.raycast(PolyRef, RecastStart, RecastEnd, GetDefaultDetourFilter(), &HitTime, HitNormal, NULL, NULL, 0)))
											{
												bWalkReachable = (HitTime >= 1.0f);
											}
										}
										// HACK: avoid placing jumps to extra lift navmesh geometry created to work around lack of movable navmeshes
										bool bSkipForLift = false;
										if (!bWalkReachable)
										{
											for (const FVector& LiftLoc : LiftHackLocs)
											{
												if ((TestLoc - LiftLoc).SizeSquared() < 250000.0f) // 500 * 500
												{
													bSkipForLift = true;
													break;
												}
											}
										}
										if (!bWalkReachable && !bSkipForLift)
										{
											TestLoc += HeightAdjust;
											float RequiredJumpZ = 0.0f;
											if (OnlyJumpReachable(DefaultScout, WallCenter, TestLoc, PolyRef, It.Key(), -1.0f, &RequiredJumpZ))
											{
												bool bNeedsJumpSpec = RequiredJumpZ > BaseJumpZ;
												// TODO: account for MaxFallSpeed
												bool bFound = false;
												for (FUTPathLink& ExistingLink : Node->Paths)
												{
													if (ExistingLink.End == It.Value() && ExistingLink.StartEdgePoly == PolyRef)
													{
														bool bValid = false;
														if (!bNeedsJumpSpec)
														{
															if (ExistingLink.Spec.Get() == NULL)
															{
																bValid = true;
															}
														}
														else
														{
															// accept if existing jump is reasonably close in requirements
															UUTReachSpec_HighJump* JumpSpec = Cast<UUTReachSpec_HighJump>(ExistingLink.Spec.Get());
															if (JumpSpec != NULL && JumpSpec->RequiredJumpZ > RequiredJumpZ && RequiredJumpZ > JumpSpec->RequiredJumpZ * 0.9f)
															{
																bValid = true;
															}
														}
														if (bValid)
														{
															ExistingLink.AdditionalEndPolys.Add(It.Key());
															bFound = true;
															break;
														}
													}
												}

												if (!bFound)
												{
													UUTReachSpec_HighJump* JumpSpec = NULL;
													if (bNeedsJumpSpec)
													{
														JumpSpec = NewObject<UUTReachSpec_HighJump>(Node);
														JumpSpec->RequiredJumpZ = RequiredJumpZ;
														JumpSpec->GravityVolume = FindPhysicsVolume(GetWorld(), WallCenter, FCollisionShape::MakeSphere(0.0f));
														JumpSpec->OriginalGravityZ = (JumpSpec->GravityVolume != NULL) ? JumpSpec->GravityVolume->GetGravityZ() : GetWorld()->GetGravityZ();
														AllReachSpecs.Add(JumpSpec);
													}
													FUTPathLink* NewLink = new(Node->Paths) FUTPathLink(Node, PolyRef, It.Value(), It.Key(), JumpSpec, PathSize.Radius, PathSize.Height, R_JUMP);
												}
											}
										}
									}
								}
							}
						}
					}
					// calculate distance and core end point of all jump paths
					// we use the closest poly to the center of the group
					for (FUTPathLink& Link : Node->Paths)
					{
						if (Link.Distances.Num() == 0)
						{
							CalcJumpPathDistance(Link);
						}
					}
				}
				if (!PathNodes.IsValidIndex(SpecialLinkBuildNodeIndex))
				{
					SpecialLinkBuildPass++;
					SpecialLinkBuildNodeIndex = 0;
				}
			}
			// second pass: find all jump downs that are one way and jump test back up
			// this is a more optimized approach to generating jump paths from open areas up to ledges and such
			// which won't be detected by the previous pass due to checking only jumps from poly walls
			if (SpecialLinkBuildPass == 1)
			{
				FCollisionShape ScoutShape = FCollisionShape::MakeCapsule(DefaultScout->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), DefaultScout->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
				for (; SpecialLinkBuildNodeIndex < PathNodes.Num() && NumToProcess > 0; SpecialLinkBuildNodeIndex++, NumToProcess--)
				{
					UUTPathNode* Node = PathNodes[SpecialLinkBuildNodeIndex];
					if (bDisplayProgressDialog)
					{
						GWarn->UpdateProgress(SpecialLinkBuildNodeIndex + PathNodes.Num(), PathNodes.Num() * 2);
					}
					for (const FUTPathLink& Link : Node->Paths)
					{
						if ( !Link.End->bDestinationOnly && (Link.ReachFlags & R_JUMP) &&
							(!Link.Spec.IsValid() || (Cast<UUTReachSpec_HighJump>(Link.Spec.Get()) != NULL && !((UUTReachSpec_HighJump*)Link.Spec.Get())->bJumpFromEdgePolyCenter)) &&
							GetPolyCenter(Link.StartEdgePoly).Z > GetPolyCenter(Link.EndPoly).Z )
						{
							bool bFound = false;
							/*for (const FUTPathLink& BackLink : Link.End->Paths)
							{
								if (BackLink.End == Node && BackLink.EndPoly == Link.StartEdgePoly)// && (BackLink.StartEdgePoly == Link.EndPoly || Link.AdditionalEndPolys.Contains(BackLink.StartEdgePoly)))
								{
									bFound = true;
									break;
								}
							}*/
							if (!bFound)
							{
								NavNodeRef EndPoly = Link.StartEdgePoly;
								UUTPathNode* StartNode = const_cast<UUTPathNode*>(Link.End.Get()); // hacky, but ultimately we could look it up in our array anyway
								const FVector TestLoc = GetPolyCenter(EndPoly) + HeightAdjust;

								// HACK: avoid placing jumps to extra lift navmesh geometry created to work around lack of movable navmeshes
								bool bSkipForLift = false;
								for (const FVector& LiftLoc : LiftHackLocs)
								{
									if ((TestLoc - LiftLoc).SizeSquared() < 250000.0f) // 500 * 500
									{
										bSkipForLift = true;
										break;
									}
								}
								if (!bSkipForLift)
								{
									TArray<NavNodeRef> TestPolys;
									TestPolys.Add(Link.EndPoly);
									TestPolys += Link.AdditionalEndPolys;
									// sort the polys by distance, which will tend to prioritize shorter, easier to complete jumps
									TestPolys.Sort([=](const NavNodeRef A, const NavNodeRef B) { return (GetPolyCenter(A) - TestLoc).SizeSquared() < (GetPolyCenter(B) - TestLoc).SizeSquared(); });

									// search for existing links and skip testing any polys for which the path would be equal or greater difficulty than those already present
									for (int32 i = 0; i < TestPolys.Num(); i++)
									{
										if (Link.End->Paths.ContainsByPredicate([&](const FUTPathLink& TestItem) { return TestItem.End == Node && TestItem.StartEdgePoly == TestPolys[i] && TestItem.EndPoly == EndPoly; }))
										{
											TestPolys.RemoveAt(i, TestPolys.Num() - i);
											break;
										}
									}

									for (NavNodeRef StartPoly : TestPolys)
									{
										FVector StartLoc = GetPolySurfaceCenter(StartPoly) + HeightAdjust;
										if (GetWorld()->FindTeleportSpot(DefaultScout, StartLoc, (TestLoc - StartLoc).GetSafeNormal2D().Rotation()))
										{
											StartLoc.Z += 0.5f; // avoid precision issues

											APhysicsVolume* GravityVolume = FindPhysicsVolume(GetWorld(), StartLoc, FCollisionShape::MakeSphere(0.0f));
											const float GravityZ = (GravityVolume != NULL) ? GravityVolume->GetGravityZ() : GetWorld()->GetGravityZ();

											// test from closest wall as well as center to try to reduce jump requirements
											/*FVector BestWallCenter = StartLoc;
											float BestDist = FLT_MAX;
											{
												float SegmentVerts[36];
												int32 NumSegments = 0;
												InternalQuery.getPolyWallSegments(StartPoly, GetDefaultDetourFilter(), SegmentVerts, NULL, &NumSegments, 6);
												for (int32 i = 0; i < NumSegments; i++)
												{
													FVector WallCenter = (Recast2UnrealPoint(&SegmentVerts[(i * 2) * 3]) + Recast2UnrealPoint(&SegmentVerts[(i * 2 + 1) * 3])) * 0.5f + HeightAdjust;
													float Dist = (WallCenter - TestLoc).Size();
													if (Dist < BestDist)
													{
														BestWallCenter = WallCenter;
														BestDist = Dist;
													}
												}
											}*/

											float RequiredJumpZ = 0.0f;
											if (JumpTraceTest(StartLoc, TestLoc, StartPoly, EndPoly, ScoutShape, DefaultScout->GetCharacterMovement()->MaxWalkSpeed, GravityZ, BaseJumpZ, -1.0f, &RequiredJumpZ) && RequiredJumpZ > BaseJumpZ)
											{
												// TODO: account for MaxFallSpeed
												UUTReachSpec_HighJump* JumpSpec = NewObject<UUTReachSpec_HighJump>(StartNode);
												JumpSpec->bJumpFromEdgePolyCenter = true;
												JumpSpec->RequiredJumpZ = RequiredJumpZ;
												// see if dodge jump can improve reachability
												const AUTCharacter* UTScout = Cast<AUTCharacter>(DefaultScout);
												if (UTScout != NULL && UTScout->UTCharacterMovement->DodgeImpulseHorizontal > DefaultScout->GetCharacterMovement()->MaxWalkSpeed)
												{
													float DodgeJumpZ = 0.0f;
													if (JumpTraceTest(StartLoc, TestLoc, StartPoly, EndPoly, ScoutShape, UTScout->UTCharacterMovement->DodgeImpulseHorizontal, GravityZ, BaseJumpZ, RequiredJumpZ, &DodgeJumpZ))
													{
														JumpSpec->DodgeJumpZMult = DodgeJumpZ / RequiredJumpZ;
													}
												}
												JumpSpec->GravityVolume = GravityVolume;
												JumpSpec->OriginalGravityZ = GravityZ;
												AllReachSpecs.Add(JumpSpec);
												FUTPathLink* NewLink = new(StartNode->Paths) FUTPathLink(StartNode, StartPoly, Node, EndPoly, JumpSpec, PathSize.Radius, PathSize.Height, R_JUMP);
												CalcJumpPathDistance(*NewLink);
												break;
											}
										}
									}
								}
							}
						}
					}
				}
				if (!PathNodes.IsValidIndex(SpecialLinkBuildNodeIndex))
				{
					SpecialLinkBuildPass++;
					SpecialLinkBuildNodeIndex = 0;
				}
			}
			if (bDisplayProgressDialog)
			{
				GWarn->EndSlowTask();
			}
			if (SpecialLinkBuildPass > 1)
			{
				int32 JumpCount = 0;
				int32 TotalCount = 0;
				for (UUTPathNode* Node : PathNodes)
				{
					for (const FUTPathLink& Link : Node->Paths)
					{
						TotalCount++;
						if (Link.ReachFlags & R_JUMP)
						{
							JumpCount++;
						}
					}
				}
				UE_LOG(UT, Log, TEXT("Built %i total paths (%i jump paths)"), TotalCount, JumpCount);

				UE_LOG(UT, Log, TEXT("PathNode special link building complete"));
				SpecialLinkBuildNodeIndex = INDEX_NONE;
				SpecialLinkBuildPass = 0;
#if WITH_EDITORONLY_DATA
				if (NodeRenderer != NULL)
				{
					// we need to update the rendering immediately since a new build could be started and wipe the data before the end of the frame (e.g. if user is dragging things in the editor)
					NodeRenderer->RecreateRenderState_Concurrent();
				}
#endif
			}
		}
	}
}

void AUTRecastNavMesh::DeletePaths()
{
	for (UUTPathNode* Node : PathNodes)
	{
		Node->MarkPendingKill();
	}
	PathNodes.Empty();
	PolyToNode.Empty();
	AllReachSpecs.Empty();
	POIToNode.Empty();
	SpecialLinkBuildNodeIndex = INDEX_NONE;
	SpecialLinkBuildPass = 0;

#if WITH_EDITORONLY_DATA
	if (NodeRenderer != NULL && !HasAnyFlags(RF_BeginDestroyed))
	{
		// we need to update the rendering immediately since a new build could be started and wipe the data before the end of the frame (e.g. if user is dragging things in the editor)
		NodeRenderer->RecreateRenderState_Concurrent();
	}
#endif
}

void AUTRecastNavMesh::RebuildAll()
{
	bIsBuilding = true;
#if UE_EDITOR
	bUserRequestedBuild |= FEditorBuildUtils::IsBuildingNavigationFromUserRequest();
#endif
	DeletePaths();
	Super::RebuildAll();
}

void AUTRecastNavMesh::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// it would be nice if we were just given a callback when things were done instead of having to poll...
	bool bNewIsBuilding = NavDataGenerator.Get() != NULL && NavDataGenerator->IsBuildInProgress(false);
	if (bIsBuilding && !bNewIsBuilding)
	{
		// build is done, post process
		BuildNodeNetwork();
		bUserRequestedBuild = false;
	}
	else if (bNewIsBuilding)
	{
#if WITH_EDITOR
		bUserRequestedBuild |= FEditorBuildUtils::IsBuildingNavigationFromUserRequest();
#endif
		if (PathNodes.Num() > 0)
		{
			// clear data right away since it refers to poly IDs that are being rebuilt
			DeletePaths();
		}
	}
	else if (SpecialLinkBuildNodeIndex >= 0)
	{
		BuildSpecialLinks(1);
	}
	bIsBuilding = bNewIsBuilding;

#if WITH_EDITOR
	// HACK: cache flag that says if we need to rebuild since ARecastNavMesh implementation doesn't work in game
	if (GIsEditor)
	{
		bNeedsRebuild = NeedsRebuild();
	}
#endif
}

bool FSingleEndpointEval::InitForPathfinding(APawn* Asker, const FNavAgentProperties& AgentProps, AUTRecastNavMesh* NavData)
{
	GoalNode = NavData->FindNearestNode(GoalLoc, (GoalActor != NULL) ? NavData->GetPOIExtent(GoalActor) : NavData->GetHumanPathSize().GetExtent());
	if (GoalNode == NULL && GoalActor != NULL)
	{
		// try bottom of collision, since Recast doesn't seem to care about extent
		GoalNode = NavData->FindNearestNode(GoalLoc - FVector(0.0f, 0.0f, GoalActor->GetSimpleCollisionHalfHeight()), NavData->GetPOIExtent(GoalActor));
	}
	return GoalNode != NULL;
}
float FSingleEndpointEval::Eval(APawn* Asker, const FNavAgentProperties& AgentProps, const UUTPathNode* Node, const FVector& EntryLoc, int32 TotalDistance)
{
	return (Node == GoalNode) ? 10.0f : 0.0f;
}

UUTPathNode* AUTRecastNavMesh::FindNearestNode(const FVector& TestLoc, const FVector& Extent) const
{
	NavNodeRef PolyRef = FindNearestPoly(TestLoc, Extent);
	return PolyToNode.FindRef(PolyRef);
}

bool AUTRecastNavMesh::RaycastWithZCheck(const FVector& RayStart, const FVector& RayEnd, FVector* HitLocation, NavNodeRef* LastPoly) const
{
	// partially copied from ARecastNavMesh::NavMeshRaycast() and FPImplRecastNavMesh::Raycast2D() to work around some issues
	BeginBatchQuery();
	
	FRaycastResult RayResult;

	const dtQueryFilter* QueryFilter = GetDefaultDetourFilter();

	FRecastSpeciaLinkFilter LinkFilter(UNavigationSystem::GetCurrent(GetWorld()), NULL);
	
	dtNavMeshQuery NavQuery;
	dtNavMeshQuery& NavQueryVariable = IsInGameThread() ? GetRecastNavMeshImpl()->SharedNavQuery : NavQuery;
	NavQueryVariable.init(GetRecastNavMeshImpl()->GetRecastMesh(), RECAST_MAX_SEARCH_NODES, &LinkFilter);
	
	const FCapsuleSize HumanSize = GetHumanPathSize();
	const float Extent[3] = { float(HumanSize.Radius), float(HumanSize.Height), float(HumanSize.Radius) };

	const FVector RecastStart = Unreal2RecastPoint(RayStart);
	const FVector RecastEnd = Unreal2RecastPoint(RayEnd);

	NavNodeRef StartNode = INVALID_NAVNODEREF;
	NavQueryVariable.findNearestPoly(&RecastStart.X, Extent, QueryFilter, &StartNode, NULL);

	// TODO: here's the change: if the start poly can't be found, consider it a hit at time 0 instead of a miss
	bool bResult = (StartNode != INVALID_NAVNODEREF && dtStatusSucceed(NavQueryVariable.raycast(StartNode, &RecastStart.X, &RecastEnd.X, QueryFilter, &RayResult.HitTime, &RayResult.HitNormal.X, RayResult.CorridorPolys, &RayResult.CorridorPolysCount, RayResult.GetMaxCorridorSize())));
	FinishBatchQuery();
	if (bResult)
	{
		if (LastPoly != NULL)
		{
			*LastPoly = RayResult.CorridorPolys[RayResult.CorridorPolysCount - 1];
		}
		if (RayResult.HasHit())
		{
			if (HitLocation != NULL)
			{
				*HitLocation = (RayStart + (RayEnd - RayStart) * RayResult.HitTime);
			}
			return true;
		}
		else
		{
			if (HitLocation != NULL)
			{
				*HitLocation = RayEnd;
			}
			// handle the Z axis issue in the recast function by making sure the end poly is the one we expect
			NavNodeRef EndNode = INVALID_NAVNODEREF;
			NavQueryVariable.findNearestPoly(&RecastEnd.X, Extent, QueryFilter, &EndNode, NULL);
			if (EndNode != INVALID_NAVNODEREF && RayResult.CorridorPolysCount > 0 && RayResult.CorridorPolys[RayResult.CorridorPolysCount - 1] != EndNode)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}
	else
	{
		if (LastPoly != NULL)
		{
			*LastPoly = INVALID_NAVNODEREF;
		}
		if (HitLocation != NULL)
		{
			*HitLocation = RayStart;
		}
		return true;
	}
}

TArray<FLine> AUTRecastNavMesh::GetPolyWalls(NavNodeRef PolyRef) const
{
	TArray<FLine> ResultLines;

	float SegmentVerts[36];
	int32 NumSegments = 0;
	GetRecastNavMeshImpl()->SharedNavQuery.getPolyWallSegments(PolyRef, GetDefaultDetourFilter(), SegmentVerts, nullptr, &NumSegments, 6);
	for (int32 i = 0; i < NumSegments; i++)
	{
		// wall
		ResultLines.Add(FLine(Recast2UnrealPoint(&SegmentVerts[(i * 2) * 3]), Recast2UnrealPoint(&SegmentVerts[(i * 2 + 1) * 3])));
	}
	return ResultLines;
}

float AUTRecastNavMesh::GetPolyZAtLoc(NavNodeRef PolyID, const FVector2D& Loc2D) const
{
	FVector RecastLoc = Unreal2RecastPoint(FVector(Loc2D, 0.0f));
	float Result = 0.0f;
	if (dtStatusSucceed(GetRecastNavMeshImpl()->SharedNavQuery.getPolyHeight(PolyID, (float*)&RecastLoc, &Result)))
	{
		return Result;
	}
	else
	{
		return GetPolyCenter(PolyID).Z;
	}
}

NavNodeRef AUTRecastNavMesh::FindAnchorPoly(const FVector& TestLoc, APawn* Asker, const FNavAgentProperties& AgentProps) const
{
	AUTCharacter* P = Cast<AUTCharacter>(Asker);
	if (P != NULL && P->LastReachedMoveTarget.TargetPoly != INVALID_NAVNODEREF && HasReachedTarget(P, AgentProps, P->LastReachedMoveTarget))
	{
		return P->LastReachedMoveTarget.TargetPoly;
	}
	else
	{
		NavNodeRef StartPoly = FindNearestPoly(TestLoc, FVector(AgentProps.AgentRadius, AgentProps.AgentRadius, AgentProps.AgentHeight * 0.5f));
		if (StartPoly != INVALID_NAVNODEREF)
		{
			// currently in a water volume the polys are at the bottom of the water area
			APhysicsVolume* Volume = FindPhysicsVolume(GetWorld(), TestLoc, FCollisionShape::MakeCapsule(AgentProps.AgentRadius, AgentProps.AgentHeight * 0.5f));
			if (Volume->bWaterVolume && Volume->GetBrushComponent() != NULL)
			{
				FHitResult Hit;
				if (Volume->GetBrushComponent()->LineTraceComponent(Hit, TestLoc - FVector(0.0f, 0.0f, 100000.0f), TestLoc, FCollisionQueryParams()))
				{
					StartPoly = FindNearestPoly(Hit.Location + FVector(0.0f, 0.0f, AgentProps.AgentHeight * 0.5f), FVector(AgentProps.AgentRadius, AgentProps.AgentRadius, AgentProps.AgentHeight * 0.5f));
				}
			}
		}
		// HACK: handle lifts (see FindLiftPoly())
		if (StartPoly == INVALID_NAVNODEREF)
		{
			StartPoly = FindLiftPoly(Asker, AgentProps);
		}
		return StartPoly;
	}
}

NavNodeRef AUTRecastNavMesh::FindLiftPoly(APawn* Asker, const FNavAgentProperties& AgentProps) const
{
	if (Asker == NULL || GetRecastNavMeshImpl()->DetourNavMesh == NULL)
	{
		return INVALID_NAVNODEREF;
	}
	else
	{
		UPrimitiveComponent* MovementBase = Asker->GetMovementBase();
		if (MovementBase == NULL)
		{
			return INVALID_NAVNODEREF;
		}
		else
		{
			FVector Vel = MovementBase->GetComponentVelocity();
			if (Vel.IsZero())
			{
				return INVALID_NAVNODEREF;
			}
			else
			{
				// extend the search query by a large amount in the direction of the movement
				const FVector AgentLoc = Asker->GetNavAgentLocation();
				const FVector AgentExtent(AgentProps.AgentRadius, AgentProps.AgentRadius, AgentProps.AgentHeight * 0.5f);
				FBox TestBox(AgentLoc - AgentExtent, AgentLoc + AgentExtent);
				const FVector TraceEnd = Asker->GetNavAgentLocation() + Vel * 2.0f;
				TestBox += TraceEnd;
				
				TestBox = Unreal2RecastBox(TestBox);
				float RecastCenter[3] = { TestBox.GetCenter().X, TestBox.GetCenter().Y, TestBox.GetCenter().Z };
				float RecastExtent[3] = { TestBox.GetExtent().X, TestBox.GetExtent().Y, TestBox.GetExtent().Z };
				dtNavMeshQuery& InternalQuery = GetRecastNavMeshImpl()->SharedNavQuery;
				NavNodeRef ResultPolys[10];
				int32 NumPolys = 0;
				InternalQuery.queryPolygons(RecastCenter, RecastExtent, GetDefaultDetourFilter(), ResultPolys, &NumPolys, ARRAY_COUNT(ResultPolys));
				float BestDist = FLT_MAX;
				NavNodeRef BestResult = INVALID_NAVNODEREF;
				for (int32 i = 0; i < NumPolys; i++)
				{
					// do a more precise intersection against the poly bounding box
					TArray<FVector> PolyVerts;
					GetPolyVerts(ResultPolys[i], PolyVerts);
					FBox PolyBox(0);
					for (const FVector& Vert : PolyVerts)
					{
						PolyBox += Vert;
					}
					FVector HitLocation, HitNormal;
					float HitTime;
					if (FMath::LineExtentBoxIntersection(PolyBox, AgentLoc, TraceEnd, AgentExtent * FVector(1.5f, 1.5f, 1.0f), HitLocation, HitNormal, HitTime))
					{
						float Dist = (PolyBox.GetCenter() - AgentLoc).SizeSquared();
						if (Dist < BestDist)
						{
							BestResult = ResultPolys[i];
							BestDist = Dist;
						}
					}
				}

				return BestResult;
			}
		}
	}
}

void AUTRecastNavMesh::CalcReachParams(APawn* Asker, const FNavAgentProperties& AgentProps, int32& Radius, int32& Height, int32& MaxFallSpeed, uint32& MoveFlags)
{
	Radius = FMath::TruncToInt(AgentProps.AgentRadius);
	Height = FMath::TruncToInt(AgentProps.AgentHeight * 0.5f);
	MaxFallSpeed = 0; // FIXME
	MoveFlags = 0;
	if (AgentProps.bCanJump)
	{
		MoveFlags |= R_JUMP;
	}

	if (AgentProps.bCanCrouch)
	{
		// unfortunate that this isn't in FNavAgentProperties...
		ACharacter* C = Cast<ACharacter>(Asker);
		if (C != NULL && C->GetCharacterMovement() != NULL)
		{
			Height = FMath::Min<int32>(Height, FMath::TruncToInt(C->GetCharacterMovement()->CrouchedHalfHeight));
		}
	}

	if (Asker != NULL)
	{
		AUTBot* B = Cast<AUTBot>(Asker->Controller);
		if (B != NULL)
		{
			B->SetupSpecialPathAbilities();
		}
	}
}

bool AUTRecastNavMesh::FindBestPath(APawn* Asker, const FNavAgentProperties& AgentProps, FUTNodeEvaluator& NodeEval, const FVector& StartLoc, float& Weight, bool bAllowDetours, TArray<FRouteCacheItem>& NodeRoute)
{
	DECLARE_CYCLE_STAT(TEXT("UT node pathing time"), STAT_Navigation_UTPathfinding, STATGROUP_Navigation);

	SCOPE_CYCLE_COUNTER(STAT_Navigation_UTPathfinding);

	NodeRoute.Reset();
	bool bNeedMoveToStartNode = false;
	NavNodeRef StartPoly = FindAnchorPoly(StartLoc, Asker, AgentProps);
	if (StartPoly == INVALID_NAVNODEREF)
	{
		bNeedMoveToStartNode = true;
		// first just try bigger extent, in case close to mesh
		StartPoly = FindNearestPoly(StartLoc, FVector(AgentProps.AgentRadius * 2.0f, AgentProps.AgentRadius * 2.0f, AgentProps.AgentHeight * 0.5f));
		if (StartPoly == INVALID_NAVNODEREF)
		{
			// TODO: radial search and do simple traces to try to find valid loc
		}
	}
	UUTPathNode* StartNode = PolyToNode.FindRef(StartPoly);
	// TODO: do we need something to deal with the character being on a poly with connections too small to get out of?
	if (StartPoly == INVALID_NAVNODEREF || StartNode == NULL)
	{
		// TODO: should we try to get the location back on the mesh?
		return false;
	}
	else if (!NodeEval.InitForPathfinding(Asker, AgentProps, this))
	{
		return false;
	}
	else
	{
		int32 Radius, Height, MaxFallSpeed;
		uint32 MoveFlags;
		CalcReachParams(Asker, AgentProps, Radius, Height, MaxFallSpeed, MoveFlags);

		struct FEvaluatedNode
		{
			const UUTPathNode* Node;
			NavNodeRef Poly;
			int32 TotalDistance;
			FEvaluatedNode* PrevPath;
			FEvaluatedNode* PrevOrdered;
			FEvaluatedNode* NextOrdered;
			bool bAlreadyVisited;

			FEvaluatedNode(const UUTPathNode* InNode, NavNodeRef InPoly, TMap<const UUTPathNode*, FEvaluatedNode*>& InNodeMap)
				: Node(InNode), Poly(InPoly), TotalDistance(BLOCKED_PATH_COST), PrevPath(NULL), PrevOrdered(NULL), NextOrdered(NULL), bAlreadyVisited(false)
			{
				InNodeMap.Add(Node, this);
			}
		};
		FEvaluatedNode* EvalNodePool = (FEvaluatedNode*)FMemory_Alloca(sizeof(FEvaluatedNode) * PathNodes.Num());
		int32 EvalNodePoolIndex = 0;

		TMap<const UUTPathNode*, FEvaluatedNode*> NodeMap;

		FEvaluatedNode* CurrentNode = new(EvalNodePool + EvalNodePoolIndex++) FEvaluatedNode(StartNode, StartPoly, NodeMap);
		CurrentNode->TotalDistance = 0;
		FEvaluatedNode* LastAdd = CurrentNode;
		FEvaluatedNode* BestDest = NULL;
		while (CurrentNode != NULL)
		{
			float ThisWeight = NodeEval.Eval(Asker, AgentProps, CurrentNode->Node, (CurrentNode->TotalDistance == 0) ? StartLoc : GetPolyCenter(CurrentNode->Poly), CurrentNode->TotalDistance);
			if (ThisWeight > Weight)
			{
				Weight = ThisWeight;
				BestDest = CurrentNode;
				if (ThisWeight > 1.0f)
				{
					break;
				}
			}

			int32 NextDistance = 0;
			for (int32 i = 0; i < CurrentNode->Node->Paths.Num(); i++)
			{
				if (CurrentNode->Node->Paths[i].End.IsValid() && CurrentNode->Node->Paths[i].Supports(Radius, Height, MoveFlags))
				{
					FEvaluatedNode* NextNode = NodeMap.FindRef(CurrentNode->Node->Paths[i].End.Get());
					if (NextNode == NULL)
					{
						NextNode = new(EvalNodePool + EvalNodePoolIndex++) FEvaluatedNode(CurrentNode->Node->Paths[i].End.Get(), CurrentNode->Node->Paths[i].EndPoly, NodeMap);
					}
					if (!NextNode->bAlreadyVisited)
					{
						NextDistance = CurrentNode->Node->Paths[i].CostFor(Asker, AgentProps, CurrentNode->Poly, this);
						if (NextDistance < BLOCKED_PATH_COST)
						{
							NextDistance += NodeEval.GetTransientCost(CurrentNode->Node->Paths[i], Asker, AgentProps, CurrentNode->Poly, NextDistance + CurrentNode->TotalDistance);
						}
						if (NextDistance < BLOCKED_PATH_COST)
						{
							// don't allow zero or negative distance - could create a loop
							if (NextDistance <= 0)
							{
								UE_LOG(UT, Warning, TEXT("FindBestPath(): negative weight %d from %s to %s (%s)"), NextDistance, *CurrentNode->Node->GetName(), *NextNode->Node->GetName(), *GetNameSafe(CurrentNode->Node->Paths[i].Spec.Get()));

								NextDistance = 1;
							}

							int32 NewTotalDistance = NextDistance + CurrentNode->TotalDistance;
							if (NextNode->TotalDistance > NewTotalDistance)
							{
								NextNode->Poly = CurrentNode->Node->Paths[i].EndPoly;
								NextNode->PrevPath = CurrentNode;
								if (NextNode->PrevOrdered) //remove from old position
								{
									NextNode->PrevOrdered->NextOrdered = NextNode->NextOrdered;
									if (NextNode->NextOrdered)
									{
										NextNode->NextOrdered->PrevOrdered = NextNode->PrevOrdered;
									}
									if (LastAdd == NextNode || LastAdd->TotalDistance > NextNode->TotalDistance)
									{
										LastAdd = NextNode->PrevOrdered;
									}
									NextNode->PrevOrdered = NULL;
									NextNode->NextOrdered = NULL;
								}
								NextNode->TotalDistance = NewTotalDistance;

								// LastAdd is a good starting point for searching the list and inserting this node
								FEvaluatedNode* InsertAtNode = LastAdd;
								if (InsertAtNode->TotalDistance <= NewTotalDistance)
								{
									while (InsertAtNode->NextOrdered != NULL && InsertAtNode->NextOrdered->TotalDistance < NewTotalDistance)
									{
										InsertAtNode = InsertAtNode->NextOrdered;
									}
								}
								else
								{
									while (InsertAtNode->PrevOrdered != NULL && InsertAtNode->TotalDistance > NewTotalDistance)
									{
										InsertAtNode = InsertAtNode->PrevOrdered;
									}
								}

								if (InsertAtNode->NextOrdered != NextNode)
								{
									if (InsertAtNode->NextOrdered != NULL)
									{
										InsertAtNode->NextOrdered->PrevOrdered = NextNode;
									}
									NextNode->NextOrdered = InsertAtNode->NextOrdered;
									InsertAtNode->NextOrdered = NextNode;
									NextNode->PrevOrdered = InsertAtNode;
								}
								LastAdd = NextNode;
							}
						}
					}
				}
			}
			CurrentNode = CurrentNode->NextOrdered;
		}

		if (BestDest == NULL)
		{
			return false;
		}
		else
		{
			FEvaluatedNode* NextRouteNode = BestDest;
			while (NextRouteNode->PrevPath != NULL) // don't need first node, we're already there
			{
				NodeRoute.Insert(FRouteCacheItem(NextRouteNode->Node, GetPolyCenter(NextRouteNode->Poly), NextRouteNode->Poly), 0);
				NextRouteNode = NextRouteNode->PrevPath;
			}

			// ask any ReachSpecs along path if there is an Actor target to assign to the route point
			if (NodeRoute.Num() > 0)
			{
				{
					int32 LinkIndex = NextRouteNode->Node->GetBestLinkTo(NextRouteNode->Poly, NodeRoute[0], Asker, AgentProps, this);
					if (LinkIndex != INDEX_NONE && NextRouteNode->Node->Paths[LinkIndex].Spec.IsValid())
					{
						NodeRoute[0].Actor = NextRouteNode->Node->Paths[LinkIndex].Spec->GetMoveTargetActor();
					}
				}
				for (int32 i = 1; i < NodeRoute.Num(); i++)
				{
					int32 LinkIndex = NodeRoute[i - 1].Node->GetBestLinkTo(NodeRoute[i - 1].TargetPoly, NodeRoute[i], Asker, AgentProps, this);
					if (LinkIndex != INDEX_NONE && NodeRoute[i - 1].Node->Paths[LinkIndex].Spec.IsValid())
					{
						NodeRoute[i].Actor = NodeRoute[i - 1].Node->Paths[LinkIndex].Spec->GetMoveTargetActor();
					}
				}
			}

			FVector RouteGoalLoc = FVector::ZeroVector;
			AActor* RouteGoal = NULL;
			if (NodeEval.GetRouteGoal(RouteGoal, RouteGoalLoc))
			{
				new(NodeRoute) FRouteCacheItem(RouteGoal, RouteGoalLoc, FindNearestPoly(RouteGoalLoc, FVector(AgentProps.AgentRadius, AgentProps.AgentRadius, AgentProps.AgentHeight)));
			}

			if (bNeedMoveToStartNode || NodeRoute.Num() == 0) // make sure success always returns a route
			{
				NodeRoute.Insert(FRouteCacheItem(NextRouteNode->Node, GetPolyCenter(NextRouteNode->Poly), NextRouteNode->Poly), 0);
			}

			if (bAllowDetours && !bNeedMoveToStartNode && Asker != NULL && NodeRoute.Num() > ((RouteGoal != NULL) ? 2 : 1))
			{
				FVector NextLoc = NodeRoute[0].GetLocation(Asker);
				FVector NextDir = (NextLoc - StartLoc).GetSafeNormal();
				float MaxDetourDist = (StartLoc - NextLoc).Size() * 1.5f;
				// TODO: get movement speed for non-characters somehow
				const float MoveSpeed = FMath::Max<float>(1.0f, (Cast<ACharacter>(Asker) != NULL) ? ((ACharacter*)Asker)->GetCharacterMovement()->GetMaxSpeed() : GetDefault<AUTCharacter>()->GetCharacterMovement()->MaxWalkSpeed);
				MaxDetourDist = FMath::Max<float>(MaxDetourDist, MoveSpeed * 2.0f);
				AUTBot* B = Cast<AUTBot>(Asker->Controller);
				AActor* BestDetour = NULL;
				float BestDetourWeight = 0.0f;
				for (TWeakObjectPtr<AActor> POI : NextRouteNode->Node->POIs)
				{
					if (POI.IsValid())
					{
						AUTPickup* Pickup = Cast<AUTPickup>(POI.Get());
						AUTDroppedPickup* DroppedPickup = Cast<AUTDroppedPickup>(POI.Get());
						if (Pickup != NULL || DroppedPickup != NULL)
						{
							FVector POILoc = POI->GetActorLocation();
							float Dist = (POILoc - NextLoc).Size();
							bool bValid = (DroppedPickup != NULL);
							if (!bValid && Pickup != NULL)
							{
								// we assume detour relevant pickups are close enough to see that they're active so don't skip out on those even for low skill bots
								bValid = Pickup->State.bActive || Pickup->GetRespawnTimeOffset(Asker) < FMath::Min<float>(Dist / MoveSpeed + 1.0f, B->RespawnPredictionTime);
							}
							if (bValid)
							{
								// reject detours too far behind desired path
								float Angle = (POILoc - StartLoc).GetSafeNormal() | NextDir;
								float MaxDist = (MaxDetourDist / (2.0f - Angle));
								if (Dist < MaxDist)
								{
									float NewDetourWeight;
									if (Pickup != NULL)
									{
										NewDetourWeight = Pickup->DetourWeight(Asker, Dist) / FMath::Max<float>(Dist, 1.0f);
									}
									else
									{
										NewDetourWeight = DroppedPickup->DetourWeight(Asker, Dist) / FMath::Max<float>(Dist, 1.0f);
									}
									if (NewDetourWeight > BestDetourWeight)
									{
										BestDetour = POI.Get();
									}
								}
							}
						}
					}
				}
				if (BestDetour != NULL)
				{
					// intentional double height to be sure we get a poly
					NavNodeRef DetourPoly = FindNearestPoly(BestDetour->GetActorLocation(), FVector(AgentProps.AgentRadius, AgentProps.AgentRadius, AgentProps.AgentHeight));
					if (DetourPoly != INVALID_NAVNODEREF && NextRouteNode->Node->Polys.Contains(DetourPoly))
					{
						NodeRoute.Insert(FRouteCacheItem(BestDetour, BestDetour->GetActorLocation(), DetourPoly), 0);
					}
				}
			}
			
			// pull off any route points that have actually been reached already
			// this is a workaround for sliver polygons causing AI confusion with the poly it is on
			if (Asker != NULL)
			{
				while (NodeRoute.Num() > 1 && HasReachedTarget(Asker, AgentProps, NodeRoute[0]))
				{
					NodeRoute.RemoveAt(0);
				}
			}

			return true;
		}
	}
}

bool AUTRecastNavMesh::FindPolyPath(FVector StartLoc, const FNavAgentProperties& AgentProps, const FRouteCacheItem& Target, TArray<NavNodeRef>& PolyRoute, bool bSkipCurrentPoly) const
{
	PolyRoute.Reset();
	NavNodeRef StartPoly = FindNearestPoly(StartLoc, FVector(AgentProps.AgentRadius, AgentProps.AgentRadius, AgentProps.AgentHeight * 0.5f));
	if (StartPoly == INVALID_NAVNODEREF || Target.TargetPoly == INVALID_NAVNODEREF)
	{
		return false;
	}
	else
	{
		StartLoc = Unreal2RecastPoint(StartLoc);
		float RecastStart[3] = { StartLoc.X, StartLoc.Y, StartLoc.Z };
		FVector RecastEndVect = Unreal2RecastPoint(GetPolySurfaceCenter(Target.TargetPoly));
		float RecastEnd[3] = { RecastEndVect.X, RecastEndVect.Y, RecastEndVect.Z };
		dtQueryResult PathData;
		dtStatus Result = GetRecastNavMeshImpl()->SharedNavQuery.findPath(StartPoly, Target.TargetPoly, RecastStart, RecastEnd, GetDefaultDetourFilter(), PathData, NULL);
		if (!dtStatusSucceed(Result) || (Result & DT_PARTIAL_RESULT))
		{
			return false;
		}
		else
		{
			for (int32 j = bSkipCurrentPoly ? 1 : 0; j < PathData.size(); j++) // skip first node because we're on it
			{
				PolyRoute.Add(PathData.getRef(j));
			}
			return true;
		}
	}
}

bool AUTRecastNavMesh::DoStringPulling(const FVector& OrigStartLoc, const TArray<NavNodeRef>& PolyRoute, const FNavAgentProperties& AgentProps, TArray<FComponentBasedPosition>& MovePoints) const
{
	if (PolyRoute.Num() == 0)
	{
		return false;
	}
	else
	{
		FVector StartLoc = Unreal2RecastPoint(OrigStartLoc);
		float RecastStart[3] = { StartLoc.X, StartLoc.Y, StartLoc.Z };
		FVector RecastEndVect = Unreal2RecastPoint(GetPolySurfaceCenter(PolyRoute.Last()));
		float RecastEnd[3] = { RecastEndVect.X, RecastEndVect.Y, RecastEndVect.Z };
		int32 NumResultPoints = PolyRoute.Num() * 3;
		float* ResultPoints = (float*)FMemory_Alloca(sizeof(float)* 3 * NumResultPoints);
		unsigned char* ResultFlags = (unsigned char*)FMemory_Alloca(sizeof(unsigned char)* NumResultPoints);
		NavNodeRef* ResultPolys = (NavNodeRef*)FMemory_Alloca(sizeof(NavNodeRef)* NumResultPoints);

		// TODO: dtPathCorridor does a heap allocation and copy that we can probably find a way to avoid
		dtPathCorridor Corridor;
		Corridor.init(PolyRoute.Num());
		Corridor.reset(PolyRoute[0], RecastStart);
		Corridor.optimizePathTopology(&GetRecastNavMeshImpl()->SharedNavQuery, GetDefaultDetourFilter()); // could probably skip this if perf is a concern
		Corridor.setCorridor(RecastEnd, PolyRoute.GetData(), PolyRoute.Num());
		int32 ReturnedPoints = Corridor.findCorners(ResultPoints, ResultFlags, ResultPolys, NumResultPoints, &GetRecastNavMeshImpl()->SharedNavQuery, GetDefaultDetourFilter(), AgentProps.AgentRadius);

		if (ReturnedPoints <= 0)
		{
			return false;
		}
		else
		{
			FVector HeightAdd(0.0f, 0.0f, AgentProps.AgentHeight * 0.5f); // returned points are on the surface of the mesh, raise to agent center
			for (int32 i = 0; i < ReturnedPoints; i++)
			{
				MovePoints.Add(FComponentBasedPosition(Recast2UnrealPoint(ResultPoints + i * 3) + HeightAdd));
			}
			return true;
		}
	}
}

bool AUTRecastNavMesh::GetMovePoints(const FVector& OrigStartLoc, APawn* Asker, const FNavAgentProperties& AgentProps, const FRouteCacheItem& Target, const TArray<FRouteCacheItem>& FullRoute, TArray<FComponentBasedPosition>& MovePoints, FUTPathLink& NodeLink, float* TotalDistance) const
{
	bool bResult = false;

	MovePoints.Reset();
	if (TotalDistance != NULL)
	{
		*TotalDistance = 0.0f;
	}
	NodeLink = FUTPathLink();

	NavNodeRef StartPoly = FindAnchorPoly(OrigStartLoc, Asker, AgentProps);
	if (StartPoly == INVALID_NAVNODEREF)
	{
		// we're off the navmesh, but FindBestPath() may have suggested a re-entry point to get us in here
		// try moving directly to target, checking simple trace to make sure we don't have something completely invalid
		FCollisionQueryParams Params(FName(TEXT("GetMovePointsFallback")), false, Asker);
		if (!GetWorld()->SweepTestByChannel(OrigStartLoc + FVector(0.0f, 0.0f, AgentProps.AgentHeight), Target.GetLocation(Asker) + FVector(0.0f, 0.0f, AgentProps.AgentHeight), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeBox(FVector(10.0f, 10.0f, 5.0f)), Params))
		{
			MovePoints.Add(FComponentBasedPosition(Target.GetLocation(Asker)));
			if (TotalDistance != NULL)
			{
				*TotalDistance = (Target.GetLocation(Asker) - OrigStartLoc).Size();
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
		UUTPathNode* StartNode = PolyToNode.FindRef(StartPoly);
		if (StartNode != NULL)
		{
			int32 LinkIndex = StartNode->GetBestLinkTo(StartPoly, Target, Asker, AgentProps, this);
			if (LinkIndex != INDEX_NONE)
			{
				NodeLink = StartNode->Paths[LinkIndex];
				bResult = NodeLink.GetMovePoints(OrigStartLoc, Asker, AgentProps, Target, FullRoute, this, MovePoints);
			}
		}

		if (!bResult)
		{
			TArray<NavNodeRef> PolyRoute;
			bResult = FindPolyPath(OrigStartLoc, AgentProps, Target, PolyRoute, false) && PolyRoute.Num() > 0 && DoStringPulling(OrigStartLoc, PolyRoute, AgentProps, MovePoints);
		}

		if (bResult)
		{
			// calculate distance if desired
			if (TotalDistance != NULL)
			{
				for (int32 i = 0; i < MovePoints.Num(); i++)
				{
					*TotalDistance += (i == 0) ? (MovePoints[i].Get() - OrigStartLoc).Size() : (MovePoints[i].Get() - MovePoints[i - 1].Get()).Size();
				}
			}
			return true;
		}
		else
		{
			return false;
		}
	}
}

bool AUTRecastNavMesh::HasReachedTarget(APawn* Asker, const FNavAgentProperties& AgentProps, const FRouteCacheItem& Target) const
{
	bool bResult = [&]() -> bool
	{
		if (Asker == NULL)
		{
			return false;
		}
		else
		{
			// TODO: probably some kind of interface needed here

			AUTPickup* Pickup = NULL;
			AUTTeleporter* Teleporter = NULL;
			if (Target.Actor.IsValid())
			{
				Pickup = Cast<AUTPickup>(Target.Actor.Get());
				Teleporter = Cast<AUTTeleporter>(Target.Actor.Get());
			}
			if (Pickup != NULL && Pickup->State.bActive)
			{
				return Pickup->IsOverlappingActor(Asker);
			}
			else if (Teleporter != NULL && Teleporter->IsOverlappingActor(Asker))
			{
				Teleporter->OnOverlapBegin(Asker);
				return true;
			}
			else if (Target.IsDirectTarget())
			{
				// if direct location with no nav data then require pawn box to touch target point
				FBox TestBox(0);
				FVector Extent(AgentProps.AgentRadius, AgentProps.AgentRadius, AgentProps.AgentHeight * 0.5);
				FVector AskerLoc = Asker->GetActorLocation();
				TestBox += AskerLoc + Extent;
				TestBox += AskerLoc - Extent;
				return TestBox.IsInside(Target.GetLocation(Asker));
			}
			else
			{
				NavNodeRef MyPoly = FindNearestPoly(Asker->GetNavAgentLocation(), FVector(AgentProps.AgentRadius, AgentProps.AgentRadius, AgentProps.AgentHeight * 0.5f));
				if (MyPoly == Target.TargetPoly)
				{
					return true;
				}
				else if (MyPoly == INVALID_NAVNODEREF)
				{
					return false;
				}
				else
				{
					// in cases of multiple Z axis walkable surfaces adjacent to each other, we might not quite get the polygon we expect
					// if we think we're in the right place, do a more general intersection test to see if we're touching the right polygon even though the mesh doesn't judge it the "best" one
					FBox TestBox(0);
					FVector Extent(AgentProps.AgentRadius * 0.5f, AgentProps.AgentRadius * 0.5f, AgentProps.AgentHeight * 0.5); // intentionally half radius
					FVector AskerLoc = Asker->GetActorLocation(); // note: using Actor location here, not agent location
					TestBox += AskerLoc + Extent;
					TestBox += AskerLoc - Extent;
					if (!TestBox.IsInside(Target.GetLocation(Asker)))
					{
						return false;
					}
					else
					{
						dtNavMeshQuery& InternalQuery = GetRecastNavMeshImpl()->SharedNavQuery;

						FVector RecastAskerLoc = Unreal2RecastPoint(AskerLoc);
						FVector QueryExtent = FVector(AgentProps.AgentRadius, AgentProps.AgentRadius, AgentProps.AgentHeight * 0.5f);
						float RecastExtent[3] = { QueryExtent.X, QueryExtent.Z, QueryExtent.Y };
						NavNodeRef Polys[16];
						int32 PolyCount = 0;
						if (dtStatusSucceed(InternalQuery.queryPolygons((float*)&RecastAskerLoc, RecastExtent, GetDefaultDetourFilter(), Polys, &PolyCount, ARRAY_COUNT(Polys))))
						{
							for (int32 i = 0; i < PolyCount; i++)
							{
								if (Polys[i] == Target.TargetPoly)
								{
									return true;
								}
							}
						}
						return false;
					}
				}
			}
		}
	}();
	if (bResult)
	{
		AUTCharacter* P = Cast<AUTCharacter>(Asker);
		if (P != NULL)
		{
			P->LastReachedMoveTarget = Target;
		}
	}
	return bResult;
}

void AUTRecastNavMesh::FindAdjacentPolys(APawn* Asker, const FNavAgentProperties& AgentProps, NavNodeRef StartPoly, bool bOnlyWalkable, TArray<NavNodeRef>& Polys) const
{
	Polys.Empty();
	UUTPathNode* Node = PolyToNode.FindRef(StartPoly);
	if (Node != NULL)
	{
		const dtNavMesh* InternalMesh = GetRecastNavMeshImpl()->GetRecastMesh();
		// find adjacent navmesh polys in same pathnode
		const dtPoly* PolyData = NULL;
		const dtMeshTile* TileData = NULL;
		InternalMesh->getTileAndPolyByRef(StartPoly, &TileData, &PolyData);
		uint32 i = PolyData->firstLink;
		while (i != DT_NULL_LINK)
		{
			const dtLink& Link = InternalMesh->getLink(TileData, i);
			i = Link.next;
			if (Node->Polys.Contains(Link.ref))
			{
				Polys.Add(Link.ref);
			}
		}
		// add polys in adjacent pathnodes
		for (const FUTPathLink& Link : Node->Paths)
		{
			if (Link.StartEdgePoly == StartPoly && (!bOnlyWalkable || (Link.ReachFlags == 0 && !Link.Spec.IsValid())))
			{
				Polys.AddUnique(Link.EndPoly);
				for (NavNodeRef OtherPoly : Link.AdditionalEndPolys)
				{
					Polys.AddUnique(OtherPoly);
				}
			}
		}
	}
}

#if !UE_SERVER && WITH_EDITOR && WITH_EDITORONLY_DATA
void AUTRecastNavMesh::ClearRebuildWarning()
{
	if (NeedsRebuildWarning.IsValid())
	{
		NeedsRebuildWarning.Get()->SetCompletionState(SNotificationItem::CS_None);
		NeedsRebuildWarning.Get()->ExpireAndFadeout();
		NeedsRebuildWarning.Reset();
	}
}
#endif

void AUTRecastNavMesh::PreSave()
{
#if !UE_SERVER && WITH_EDITOR && WITH_EDITORONLY_DATA
	if (NeedsRebuild() && !IsRunningCommandlet())
	{
		ClearRebuildWarning();
		
		FNotificationInfo Info(NSLOCTEXT("UTNavigationBuild", "NeedsRebuild", "Navigation needs to be rebuilt."));
		Info.bFireAndForget = false;
		Info.bUseThrobber = false;
		Info.FadeOutDuration = 0.0f;
		Info.ExpireDuration = 0.0f;
		Info.ButtonDetails.Add(FNotificationButtonInfo(NSLOCTEXT("NavigationBuild", "NavigationBuildOk", "Ok"), FText(), FSimpleDelegate::CreateUObject(this, &AUTRecastNavMesh::ClearRebuildWarning)));

		NeedsRebuildWarning = FSlateNotificationManager::Get().AddNotification(Info);
		if (NeedsRebuildWarning.IsValid())
		{
			NeedsRebuildWarning.Get()->SetCompletionState(SNotificationItem::CS_Pending);
		}
	}
#endif

	Super::PreSave();
}

void AUTRecastNavMesh::PreInitializeComponents()
{
	// set initial POI map based on what was saved in the nodes
	if (GetWorld()->IsGameWorld())
	{
		for (UUTPathNode* Node : PathNodes)
		{
			for (TWeakObjectPtr<AActor> POI : Node->POIs)
			{
				if (POI.IsValid())
				{
					POIToNode.Add(POI.Get(), Node);
				}
			}
		}
	}

	Super::PreInitializeComponents();
}

void AUTRecastNavMesh::BeginPlay()
{
	Super::BeginPlay();

	LoadMapLearningData();
}

static const int32 LearningDataSizeLimit = 20000000;

FString AUTRecastNavMesh::GetMapLearningDataFilename() const
{
	// determine filename based on map GUID
	// this causes the data to be invalidated on editor changes
	// and not invalidated on map copy/move/rename
	return FPaths::GameSavedDir() + GetOutermost()->GetGuid().ToString() + TEXT(".ai");
}

void AUTRecastNavMesh::SaveMapLearningData()
{
	if (GetWorld()->IsGameWorld())
	{
		FString Filename = GetMapLearningDataFilename();
		// serialize all path nodes and reachspecs using the SaveGame property flag as the filter
		TArray<uint8> Data;
		FArchive* DataAr = NULL;
		DataAr = new FMemoryWriter(Data, true);
		DataAr->ArIsSaveGame = true;
		FObjectAndNameAsStringProxyArchive OuterAr(*DataAr, false);
		OuterAr.ArIsSaveGame = true;
		for (UUTPathNode* Node : PathNodes)
		{
			FString SimplePathName = Node->GetPathName(GetOuter());
			OuterAr << SimplePathName;
			Node->SerializeScriptProperties(OuterAr);
		}
		for (UUTReachSpec* Spec : AllReachSpecs)
		{
			FString SimplePathName = Spec->GetPathName(GetOuter());
			OuterAr << SimplePathName;
			Spec->SerializeScriptProperties(OuterAr);
		}
		delete DataAr;
		// compress and write to disk
		int32 UncompressedSize = Data.Num();
		TArray<uint8> CompressedData;
		int32 CompressedSize = UncompressedSize;
		CompressedData.SetNum(CompressedSize);
		if (FCompression::CompressMemory(ECompressionFlags(COMPRESS_ZLIB | COMPRESS_BiasMemory), CompressedData.GetData(), CompressedSize, Data.GetData(), Data.Num()))
		{
			CompressedData.SetNum(CompressedSize);
			FArchive* FileAr = IFileManager::Get().CreateFileWriter(*Filename);
			if (FileAr != NULL)
			{
				*FileAr << UncompressedSize;
				*FileAr << CompressedData;
				delete FileAr;
			}
		}
	}
}

void AUTRecastNavMesh::LoadMapLearningData()
{
	if (GetWorld()->IsGameWorld())
	{
		FString Filename = GetMapLearningDataFilename();
		if (FPaths::FileExists(Filename))
		{
			TArray<uint8> Data;
			FArchive* DataAr = NULL;
			{
				FArchive* FileAr = IFileManager::Get().CreateFileReader(*Filename);
				if (FileAr != NULL)
				{
					int32 UncompressedSize = 0;
					*FileAr << UncompressedSize;
					TArray<uint8> CompressedData;
					*FileAr << CompressedData;
					if (UncompressedSize > LearningDataSizeLimit) // sanity check for corrupt data that could OOM crash
					{
						UE_LOG(UT, Error, TEXT("Failed to load AI map data for %s; size limit of %i exceeded"), *GetOutermost()->GetName(), LearningDataSizeLimit);
					}
					else
					{
						Data.AddUninitialized(UncompressedSize);
						if (FCompression::UncompressMemory(ECompressionFlags(COMPRESS_ZLIB | COMPRESS_BiasMemory), Data.GetData(), UncompressedSize, CompressedData.GetData(), CompressedData.Num()))
						{
							DataAr = new FMemoryReader(Data, true);
						}
					}
					delete FileAr;
				}
			}
			if (DataAr != NULL)
			{
				FObjectAndNameAsStringProxyArchive OuterAr(*DataAr, false);
				OuterAr.ArIsSaveGame = true;
				// serialize objects until we reach the end of the data
				// we have to halt if any are not found as we're not storing seeking info that would allow skipping missing objects
				while (!OuterAr.AtEnd())
				{
					FString PathName;
					OuterAr << PathName;
					UObject* Obj = StaticFindObject(UObject::StaticClass(), GetOuter(), *PathName);
					if (Obj != NULL)
					{
						Obj->SerializeScriptProperties(OuterAr);
					}
					else
					{
						UE_LOG(UT, Error, TEXT("Failed to load AI map data for %s"), *GetOutermost()->GetName());
						break;
					}
				}
				delete DataAr;
			}
		}
	}
}

void AUTRecastNavMesh::AddToNavigation(AActor* NewPOI)
{
	// in editor this will be handled by path building
	if (GetWorld()->IsGameWorld() && NewPOI != NULL && !NewPOI->bPendingKillPending)
	{
		// remove any previous entry
		// we don't early out because the POI may have moved
		RemoveFromNavigation(NewPOI);

		UUTPathNode* BestNode = FindNearestNode(NewPOI->GetActorLocation(), NewPOI->GetSimpleCollisionCylinderExtent());
		if (BestNode == NULL)
		{
			// try bottom of cylinder instead
			BestNode = FindNearestNode(NewPOI->GetActorLocation() - FVector(0.0f, 0.0f, NewPOI->GetSimpleCollisionHalfHeight()), NewPOI->GetSimpleCollisionCylinderExtent());
		}
		if (BestNode != NULL)
		{
			BestNode->POIs.Add(NewPOI);
			POIToNode.Add(NewPOI, BestNode);
		}
		else
		{
			UE_LOG(UT, Warning, TEXT("Failed to add %s to path network: no nearby navigable area was found"), *NewPOI->GetName());
		}
	}
}
void AUTRecastNavMesh::RemoveFromNavigation(AActor* OldPOI)
{
	// in editor this will be handled by path building
	if (GetWorld()->IsGameWorld())
	{
		UUTPathNode* Node = POIToNode.FindRef(OldPOI);
		if (Node != NULL)
		{
			Node->POIs.Remove(OldPOI);
			POIToNode.Remove(OldPOI);
		}
	}
}

void AUTRecastNavMesh::GetNodeTriangleMap(TMap<const UUTPathNode*, FNavMeshTriangleList>& TriangleMap)
{
	if (GetRecastNavMeshImpl() != NULL)
	{
		const dtNavMesh* InternalMesh = GetRecastNavMeshImpl()->GetRecastMesh();
		if (InternalMesh != NULL)
		{
			TMultiMap<const UUTPathNode*, const dtMeshTile*> NodeToTile;

			for (TMap<NavNodeRef, UUTPathNode*>::TConstIterator It(PolyToNode); It; ++It)
			{
				const UUTPathNode* Node = It.Value();
				FNavMeshTriangleList& TriangleData = TriangleMap.FindOrAdd(Node);

				const dtMeshTile* Tile = NULL;
				const dtPoly* Poly = NULL;
				if (dtStatusSucceed(InternalMesh->getTileAndPolyByRef(It.Key(), &Tile, &Poly)))
				{
					dtMeshHeader const* const Header = Tile->header;
					if (Header != NULL && Poly->getType() == DT_POLYTYPE_GROUND)
					{
						// add vertices if not already added
						if (NodeToTile.FindPair(Node, Tile) == NULL)
						{
							NodeToTile.Add(Node, Tile);
							// add all the poly verts
							float* F = Tile->verts;
							for (int32 VertIdx = 0; VertIdx < Header->vertCount; ++VertIdx)
							{
								TriangleData.Verts.Add(Recast2UnrealPoint(F));
								F += 3;
							}
							int32 const DetailVertIndexBase = Header->vertCount;
							// add the detail verts
							F = Tile->detailVerts;
							for (int32 DetailVertIdx = 0; DetailVertIdx < Header->detailVertCount; ++DetailVertIdx)
							{
								TriangleData.Verts.Add(Recast2UnrealPoint(F));
								F += 3;
							}
						}

						// add triangle indices
						uint32 BaseVertIndex = 0;
						{
							// figure out base index for this tile's vertices
							TArray<const dtMeshTile*> ExistingTiles;
							NodeToTile.MultiFind(Node, ExistingTiles, true);
							for (const dtMeshTile* TestTile : ExistingTiles)
							{
								if (TestTile == Tile)
								{
									break;
								}
								else
								{
									BaseVertIndex += TestTile->header->vertCount + TestTile->header->detailVertCount;
								}
							}
						}

						const dtPolyDetail* DetailPoly = &Tile->detailMeshes[InternalMesh->decodePolyIdPoly(It.Key())];
						for (int32 TriIdx = 0; TriIdx < DetailPoly->triCount; ++TriIdx)
						{
							int32 DetailTriIdx = (DetailPoly->triBase + TriIdx) * 4;
							const unsigned char* DetailTri = &Tile->detailTris[DetailTriIdx];

							// calc indices into the vert buffer we just populated
							int32 TriVertIndices[3];
							for (int32 TriVertIdx = 0; TriVertIdx < 3; ++TriVertIdx)
							{
								if (DetailTri[TriVertIdx] < Poly->vertCount)
								{
									TriVertIndices[TriVertIdx] = BaseVertIndex + Poly->verts[DetailTri[TriVertIdx]];
								}
								else
								{
									TriVertIndices[TriVertIdx] = BaseVertIndex + Header->vertCount + (DetailPoly->vertBase + DetailTri[TriVertIdx] - Poly->vertCount);
								}
							}

							new(TriangleData.Triangles) FNavMeshTriangleList::FTriangle(TriVertIndices);
						}
					}
				}
			}
		}
	}
}