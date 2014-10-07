// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTRecastNavMesh.h"
#include "UTNavGraphRenderingComponent.h"
#include "RecastNavMeshGenerator.h"
#include "Runtime/Engine/Private/AI/Navigation/PImplRecastNavMesh.h"
#include "Runtime/Navmesh/Public/Detour/DetourNavMesh.h"
#include "Runtime/Navmesh/Public/Detour/DetourNavMeshQuery.h"
#include "Runtime/Navmesh/Public/DetourCrowd/DetourPathCorridor.h"
#include "Runtime/Engine/Private/AI/Navigation/RecastHelpers.h"
#include "UTPathBuilderInterface.h"
#include "UTDroppedPickup.h"
#if WITH_EDITOR
#include "EditorBuildUtils.h"
#endif

UUTPathBuilderInterface::UUTPathBuilderInterface(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{}

AUTRecastNavMesh::AUTRecastNavMesh(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
#if WITH_EDITOR
	if (GIsEditor && !IsTemplate())
	{
		EditorTick = new FUTNavMeshEditorTick(this);
	}
#if WITH_EDITORONLY_DATA
	NodeRenderer = PCIP.CreateEditorOnlyDefaultSubobject<UUTNavGraphRenderingComponent>(this, FName(TEXT("NodeRenderer")));
#endif
#endif

#if WITH_NAVIGATION_GENERATOR
	SpecialLinkBuildNodeIndex = INDEX_NONE;
#endif

	SizeSteps.Add(FCapsuleSize(46, 92));
	SizeSteps.Add(FCapsuleSize(46, 64));
	JumpTestThreshold2D = 2048.0f;
	ScoutClass = AUTCharacter::StaticClass();

	MaxTileGridWidth = 512;
	MaxTileGridHeight = 512;
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

const dtQueryFilter* AUTRecastNavMesh::GetDefaultDetourFilter() const
{
	return ((const FRecastQueryFilter*)GetDefaultQueryFilterImpl())->GetAsDetourQueryFilter();
}

FCapsuleSize AUTRecastNavMesh::GetSteppedEdgeSize(const struct dtPoly* PolyData, const struct dtMeshTile* TileData, const struct dtLink& Link) const
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
	float Height = 0.0f;
	InternalQuery.getPolyHeight(Link.ref, Vert1, &Height);
	// TODO: recast is giving us negative poly heights sometimes. Is Abs() the right answer?
	ActualSize.Height = FMath::Abs<int32>(FMath::TruncToInt(Height * 0.5f));

	InternalQuery.getPolyHeight(Link.ref, Vert2, &Height);
	ActualSize.Height = FMath::Min<int32>(FMath::Abs<int32>(FMath::TruncToInt(Height * 0.5f)), ActualSize.Height);

	// we get some really low heights... maybe it doesn't account for AgentHeight?
	ActualSize.Height += FMath::TruncToInt(AgentHeight);

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
					FCapsuleSize NewSize = GetSteppedEdgeSize(PolyData, TileData, Link);
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

bool AUTRecastNavMesh::OnlyJumpReachable(APawn* Scout, FVector Start, const FVector& End, NavNodeRef StartPoly, NavNodeRef EndPoly, float MaxJumpZ, float* RequiredJumpZ, float* MaxFallSpeed) const
{
	ACharacter* Char = Cast<ACharacter>(Scout);
	if (Char == NULL || Char->CharacterMovement == NULL || Char->CapsuleComponent == NULL)
	{
		// TODO: what about jumping vehicles?
		return false;
	}
	else if (!GetWorld()->FindTeleportSpot(Char, Start, (End - Start).SafeNormal2D().Rotation()))
	{
		// can't fit at start location
		return false;
	}
	else
	{
		FCollisionShape ScoutShape = FCollisionShape::MakeCapsule(Char->CapsuleComponent->GetUnscaledCapsuleRadius(), Char->CapsuleComponent->GetUnscaledCapsuleHalfHeight());
		const float TimeStep = ScoutShape.GetCapsuleRadius() / Char->CharacterMovement->MaxWalkSpeed;
		float GravityZ = GetWorld()->GetDefaultGravityZ(); // FIXME: query physics volumes (always, or just at start?)

		if (StartPoly == INVALID_NAVNODEREF)
		{
			StartPoly = FindNearestPoly(Start, ScoutShape.GetExtent());
		}
		if (EndPoly == INVALID_NAVNODEREF)
		{
			EndPoly = FindNearestPoly(End, ScoutShape.GetExtent());
		}
	
		const FVector TotalDiff = End - Start;
		// move start position to edge of walkable surface
		{
			FVector MoveSlice = TotalDiff.SafeNormal2D() * ScoutShape.GetCapsuleRadius();
			FHitResult Hit;
			bool bAnyHit = false;
			while ( !GetWorld()->SweepSingle(Hit, Start, Start + FVector(0.0f, 0.0f, AgentMaxStepHeight), FQuat::Identity, ECC_Pawn, ScoutShape, FCollisionQueryParams()) &&
					!GetWorld()->SweepSingle(Hit, Start + FVector(0.0f, 0.0f, AgentMaxStepHeight), Start + MoveSlice + FVector(0.0f, 0.0f, AgentMaxStepHeight), FQuat::Identity, ECC_Pawn, ScoutShape, FCollisionQueryParams()) &&
					GetWorld()->SweepSingle(Hit, Start + MoveSlice + FVector(0.0f, 0.0f, AgentMaxStepHeight), Start + MoveSlice - FVector(0.0f, 0.0f, 50.0f), FQuat::Identity, ECC_Pawn, ScoutShape, FCollisionQueryParams()) &&
					!Hit.bStartPenetrating )
			{
				bAnyHit = true;
				Start = Hit.Location;
				FBox TestBox(0);
				TestBox += Start + ScoutShape.GetExtent();
				TestBox += Start - ScoutShape.GetExtent();
				if (FMath::PointBoxIntersection(End, TestBox))
				{
					// reached target without jumping
					return false;
				}
				NavNodeRef NewStartPoly = FindNearestPoly(Start, ScoutShape.GetExtent());
				if (NewStartPoly != INVALID_NAVNODEREF && NewStartPoly != StartPoly)
				{
					// made it to another walk reachable poly, test jump from there instead
					return false;
				}
			}
			if (bAnyHit)
			{
				// this slight extra nudge is meant to make sure minor floating point discrepancies don't cause the zero JumpZ fall test to fail unnecessarily
				Start += MoveSlice * 0.1f;
			}
		}

		float XYTime = TotalDiff.Size2D() / Char->CharacterMovement->MaxWalkSpeed;
		float DefaultJumpZ = (DefaultEffectiveJumpZ != 0.0f) ? DefaultEffectiveJumpZ : (Char->CharacterMovement->JumpZVelocity * 0.95f);
		float DesiredJumpZ = TotalDiff.Z / XYTime - 0.5 * GravityZ * XYTime;
		if (MaxJumpZ >= 0.0f)
		{
			DefaultJumpZ = FMath::Min<float>(DefaultJumpZ, MaxJumpZ);
			DesiredJumpZ = FMath::Min<float>(DesiredJumpZ, MaxJumpZ);
		}
		for (int32 i = 0; i < ((DesiredJumpZ > DefaultJumpZ) ? 3 : 2); i++)
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
			if (RequiredJumpZ != NULL)
			{
				*RequiredJumpZ = JumpZ;
			}

			FVector CurrentLoc = Start;
			float ZSpeed = JumpZ;
			float TimeRemaining = 10.0f; // large number for sanity
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
				if (Start.Z - End.Z < -ScoutShape.GetCapsuleHalfHeight() && ZSpeed < 0.0f)
				{
					// missed, fell below target
					break;
				}

				FVector Diff = End - CurrentLoc;
				FVector NewVelocity = Diff.SafeNormal2D() * FMath::Min<float>(Diff.Size2D() / TimeStep, Char->CharacterMovement->MaxWalkSpeed);
				NewVelocity.Z = ZSpeed;
				ZSpeed += GravityZ * TimeStep;
				FVector NewLoc = CurrentLoc + NewVelocity * TimeStep;
				FHitResult Hit;
				if (GetWorld()->SweepSingle(Hit, CurrentLoc, NewLoc, FQuat::Identity, ECC_Pawn, ScoutShape, FCollisionQueryParams()))
				{
					if (Hit.Time > 0.0f && CurrentLoc != Hit.Location)
					{
						// we got some movement so take it
						CurrentLoc = Hit.Location;
					}
					else
					{
						// try Z only
						CurrentLoc -= Diff.SafeNormal2D(); // avoid float precision penetration issues
						if ((NewVelocity.X == 0.0f && NewVelocity.Y == 0.0f) || GetWorld()->SweepSingle(Hit, CurrentLoc, FVector(CurrentLoc.X, CurrentLoc.Y, NewLoc.Z), FQuat::Identity, ECC_Pawn, ScoutShape, FCollisionQueryParams()))
						{
							if (EndPoly != INVALID_NAVNODEREF && EndPoly == FindNearestPoly(CurrentLoc, ScoutShape.GetExtent()))
							{
								// if we made it to the poly we got close enough
								if (MaxFallSpeed != NULL)
								{
									*MaxFallSpeed = ZSpeed;
								}
								return true;
							}
							// give up, nowhere to go
							break;
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
}

/** returns true if the specified location is in a pain zone/volume */
static bool IsInPain(UWorld* World, const FVector& TestLoc)
{
	TArray<FOverlapResult> Hits;
	World->OverlapMulti(Hits, TestLoc, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(0.f), FComponentQueryParams());
	
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

FCapsuleSize AUTRecastNavMesh::GetHumanPathSize() const
{
	if (ScoutClass != NULL)
	{
		FCapsuleSize ActualSize(FMath::TruncToInt(ScoutClass.GetDefaultObject()->CapsuleComponent->GetUnscaledCapsuleRadius()), FMath::TruncToInt(ScoutClass.GetDefaultObject()->CapsuleComponent->GetUnscaledCapsuleHalfHeight()));

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
	else
	{
		return FCapsuleSize(FMath::TruncToInt(AgentRadius), FMath::TruncToInt(AgentHeight * 0.5f));
	}
}

FVector AUTRecastNavMesh::GetPOIExtent(AActor* POI) const
{
	// enforce a minimum extent for checks
	// this handles cases where the POI doesn't define any colliding primitives (i.e. just a point in space that AI should be aware of)
	FVector MinPOIExtent(AgentRadius, AgentRadius, AgentHeight * 0.5f);
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

#if WITH_NAVIGATION_GENERATOR
void AUTRecastNavMesh::BuildNodeNetwork()
{
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

	// make sure generation params are in the list
	SizeSteps.AddUnique(FCapsuleSize(FMath::TruncToInt(AgentRadius), FMath::TruncToInt(AgentMaxHeight)));

	// list of IUTPathBuilderInterface implementing Actors that don't want to be added as POIs but still want path building callbacks
	TArray<IUTPathBuilderInterface*> NonPOIBuilders;

	for (FActorIterator It(GetWorld()); It; ++It)
	{
		IUTPathBuilderInterface* Builder = InterfaceCast<IUTPathBuilderInterface>(*It);
		if (Builder != NULL)
		{
			if (!Builder->IsPOI())
			{
				NonPOIBuilders.Add(Builder);
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
			}
		}
	}

	const dtNavMesh* InternalMesh = GetRecastNavMeshImpl()->GetRecastMesh();
	dtNavMeshQuery& InternalQuery = GetRecastNavMeshImpl()->SharedNavQuery;

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
								FCapsuleSize OtherSize = GetSteppedEdgeSize(PolyData, TileData, Link);
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
													// this is important for pathfinding as otherwise the correct path to use depends on later points which would make pathing very complex)
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
			IUTPathBuilderInterface* Builder = InterfaceCast<IUTPathBuilderInterface>(POI.Get());
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

#if WITH_EDITORONLY_DATA
	if (NodeRenderer != NULL)
	{
		// we need to update the rendering immediately since a new build could be started and wipe the data before the end of the frame (e.g. if user is dragging things in the editor)
		NodeRenderer->RecreateRenderState_Concurrent();
	}
#endif
}

void AUTRecastNavMesh::BuildSpecialLinks(int32 NumToProcess)
{
	if (SpecialLinkBuildNodeIndex >= 0)
	{
		if (ScoutClass == NULL || ScoutClass.GetDefaultObject()->CharacterMovement == NULL)
		{
			// can't build if no scout to figure out jumps
			SpecialLinkBuildNodeIndex = INDEX_NONE;
		}
		else
		{
			dtNavMeshQuery& InternalQuery = GetRecastNavMeshImpl()->SharedNavQuery;

			FActorSpawnParameters SpawnParams;
			SpawnParams.bNoCollisionFail = true;
			ACharacter* DefaultScout = ScoutClass.GetDefaultObject();
			float BaseJumpZ = DefaultEffectiveJumpZ;
			if (BaseJumpZ == 0.0 && DefaultScout->CharacterMovement != NULL)
			{
				BaseJumpZ = DefaultScout->CharacterMovement->JumpZVelocity * 0.95; // slightly less so we can be more confident in the jumps
			}

			float MoveSpeed = ScoutClass.GetDefaultObject()->CharacterMovement->MaxWalkSpeed;
			FVector HeightAdjust(0.0f, 0.0f, AgentHeight * 0.5f);

			FCapsuleSize PathSize = GetHumanPathSize();

			for (; SpecialLinkBuildNodeIndex < PathNodes.Num() && NumToProcess > 0; SpecialLinkBuildNodeIndex++, NumToProcess--)
			{
				UUTPathNode* Node = PathNodes[SpecialLinkBuildNodeIndex];
				for (NavNodeRef PolyRef : Node->Polys)
				{
					FVector PolyCenter = GetPolyCenter(PolyRef);

					float SegmentVerts[36];
					NavNodeRef NeighborPolys[6];
					int32 NumSegments = 0;
					InternalQuery.getPolyWallSegments(PolyRef, GetDefaultDetourFilter(), NULL, 0, SegmentVerts, NeighborPolys, &NumSegments, 6);
					for (int32 i = 0; i < NumSegments; i++)
					{
						if (NeighborPolys[i] == INVALID_NAVNODEREF)
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
									if ((TestLoc - WallCenter).Size2D() < JumpTestThreshold2D && ((TestLoc - WallCenter).SafeNormal2D() | (WallCenter - PolyCenter).SafeNormal2D()) > 0.64f &&
										!IsInPain(GetWorld(), TestLoc)) // TODO: probably want to allow pain volumes in some situations... maybe make LDs manually specify those edge cases?
									{
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
										if (!bWalkReachable)
										{
											TestLoc += HeightAdjust;
											float RequiredJumpZ = 0.0f;
											if (OnlyJumpReachable(DefaultScout, WallCenter, TestLoc, PolyRef, It.Key(), BaseJumpZ, &RequiredJumpZ))
											{
												// TODO: account for RequiredJumpZ and MaxFallSpeed
												bool bFound = false;
												for (FUTPathLink& ExistingLink : Node->Paths)
												{
													if (ExistingLink.End == It.Value() && ExistingLink.StartEdgePoly == PolyRef)
													{
														ExistingLink.AdditionalEndPolys.Add(It.Key());
														bFound = true;
														break;
													}
												}

												if (!bFound)
												{
													FUTPathLink* NewLink = new(Node->Paths) FUTPathLink(Node, PolyRef, It.Value(), It.Key(), NULL, PathSize.Radius, PathSize.Height, R_JUMP);
												}
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
						
						for (NavNodeRef SrcPolyRef : Node->Polys)
						{
							Link.Distances.Add(FMath::TruncToInt((Center - GetPolyCenter(SrcPolyRef)).Size()));
						}
					}
				}
			}
			if (!PathNodes.IsValidIndex(SpecialLinkBuildNodeIndex))
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
#if WITH_NAVIGATION_GENERATOR
	SpecialLinkBuildNodeIndex = INDEX_NONE;
#endif

#if WITH_EDITORONLY_DATA
	if (NodeRenderer != NULL && !HasAnyFlags(RF_BeginDestroyed))
	{
		// we need to update the rendering immediately since a new build could be started and wipe the data before the end of the frame (e.g. if user is dragging things in the editor)
		NodeRenderer->RecreateRenderState_Concurrent();
	}
#endif
}

void AUTRecastNavMesh::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
#if WITH_EDITOR
	if (EditorTick != NULL)
	{
		EditorTick->bWasTicked = true;
	}
#endif
	if (!GetWorld()->IsPaused())
	{
		// ANavigationData::TickActor(), included because that version doesn't call Super
		PurgeUnusedPaths();
		if (NextObservedPathsTickInSeconds >= 0.f)
		{
			NextObservedPathsTickInSeconds -= DeltaTime;
			if (NextObservedPathsTickInSeconds <= 0.f)
			{
				RepathRequests.Reserve(ObservedPaths.Num());

				for (int32 PathIndex = ObservedPaths.Num() - 1; PathIndex >= 0; --PathIndex)
				{
					if (ObservedPaths[PathIndex].IsValid())
					{
						FNavPathSharedPtr SharedPath = ObservedPaths[PathIndex].Pin();
						FNavigationPath* Path = SharedPath.Get();
						EPathObservationResult::Type Result = Path->TickPathObservation();
						switch (Result)
						{
						case EPathObservationResult::NoLongerObserving:
							ObservedPaths.RemoveAtSwap(PathIndex, 1, /*bAllowShrinking=*/false);
							break;

						case EPathObservationResult::NoChange:
							// do nothing
							break;

						case EPathObservationResult::RequestRepath:
							RepathRequests.Add(FNavPathRecalculationRequest(SharedPath, ENavPathUpdateType::GoalMoved));
							break;

						default:
							check(false && "unhandled EPathObservationResult::Type in ANavigationData::TickActor");
							break;
						}
					}
					else
					{
						ObservedPaths.RemoveAtSwap(PathIndex, 1, /*bAllowShrinking=*/false);
					}
				}

				if (ObservedPaths.Num() > 0)
				{
					NextObservedPathsTickInSeconds = ObservedPathsTickInterval;
				}
			}
		}

		if (RepathRequests.Num() > 0)
		{
			// @todo batch-process it!
			for (auto RecalcRequest : RepathRequests)
			{
				FPathFindingQuery Query(RecalcRequest.Path);
				// @todo consider supplying NavAgentPropertied from path's querier
				const FPathFindingResult Result = FindPath(FNavAgentProperties(), Query.SetPathInstanceToUpdate(RecalcRequest.Path));

				if (Result.IsSuccessful())
				{
					RecalcRequest.Path->DoneUpdating(RecalcRequest.Reason);
					if (RecalcRequest.Reason == ENavPathUpdateType::NavigationChanged)
					{
						RegisterActivePath(RecalcRequest.Path);
					}
				}
				else
				{
					RecalcRequest.Path->RePathFailed();
				}
			}

			RepathRequests.Reset();
		}

		// AActor::TickActor(), included directly since ANavigationData::TickActor() doesn't call Super
		const bool bShouldTick = ((TickType != LEVELTICK_ViewportsOnly) || ShouldTickIfViewportsOnly());
		if (bShouldTick)
		{
			if (!IsPendingKill())
			{
				Tick(DeltaTime);	// perform any tick functions unique to an actor subclass
			}
		}
	}
}

void AUTRecastNavMesh::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// it would be nice if we were just given a callback when things were done instead of having to poll...
	bool bNewIsBuilding = NavDataGenerator.IsValid() && (NavDataGenerator->IsBuildInProgress(true) || ((FRecastNavMeshGenerator*)NavDataGenerator.Get())->HasResultsPending());
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
			// clear data right away since it refers to tile IDs that are being rebuilt
			DeletePaths();
		}
	}
	else if (SpecialLinkBuildNodeIndex >= 0)
	{
		BuildSpecialLinks(1);
	}
	bIsBuilding = bNewIsBuilding;
}
#endif

bool FSingleEndpointEval::InitForPathfinding(APawn* Asker, const FNavAgentProperties& AgentProps, AUTRecastNavMesh* NavData)
{
	GoalNode = NavData->FindNearestNode(GoalLoc, (GoalActor != NULL) ? GoalActor->GetSimpleCollisionCylinderExtent() : FVector::ZeroVector);
	if (GoalNode == NULL && GoalActor != NULL)
	{
		// try bottom of collision, since Recast doesn't seem to care about extent
		GoalNode = NavData->FindNearestNode(GoalLoc - FVector(0.0f, 0.0f, GoalActor->GetSimpleCollisionHalfHeight()), GoalActor->GetSimpleCollisionCylinderExtent());
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

bool AUTRecastNavMesh::RaycastWithZCheck(const FVector& RayStart, const FVector& RayEnd, float ZExtent, FVector& HitLocation) const
{
	struct FRecastZCheckFilter : public FRecastQueryFilter
	{
		float ZMin, ZMax;
		const AUTRecastNavMesh* NavMesh;

		FRecastZCheckFilter()
		: FRecastQueryFilter(true), ZMin(0.0f), ZMax(0.0f), NavMesh(NULL)
		{}

		virtual bool passVirtualFilter(const dtPolyRef ref, const dtMeshTile* tile, const dtPoly* poly) const
		{
			if (!passInlineFilter(ref, tile, poly))
			{
				return false;
			}
			else
			{
				FVector PolyCenter = NavMesh->GetPolyCenter(ref);
				return (PolyCenter.Z >= ZMin && PolyCenter.Z <= ZMax);
			}
		}
	};
	TSharedPtr<FNavigationQueryFilter> FilterContainer = MakeShareable(new FNavigationQueryFilter);
	FilterContainer->SetFilterType<FRecastZCheckFilter>();
	((FRecastZCheckFilter*)FilterContainer->GetImplementation())->NavMesh = this;
	((FRecastZCheckFilter*)FilterContainer->GetImplementation())->ZMin = FMath::Min<float>(RayStart.Z, RayEnd.Z) - ZExtent;
	((FRecastZCheckFilter*)FilterContainer->GetImplementation())->ZMax = FMath::Max<float>(RayStart.Z, RayEnd.Z) + ZExtent;

	return NavMeshRaycast(this, RayStart, RayEnd, HitLocation, FilterContainer);
}

NavNodeRef AUTRecastNavMesh::FindLiftPoly(APawn* Asker, const FNavAgentProperties& AgentProps) const
{
	if (Asker == NULL)
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
				FBox TestBox(AgentLoc + AgentExtent, AgentLoc - AgentExtent);
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
					if (FMath::LineBoxIntersection(PolyBox, AgentLoc, AgentLoc + TraceEnd, TraceEnd.SafeNormal()))
					{
						float Dist = (PolyBox.GetCenter() - AgentLoc).SizeSquared();
						if (Dist < BestDist)
						{
							UE_LOG(UT, Log, TEXT("Found lift poly"));
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
		// Radius = CrouchRadius;
		// Height = CrouchHeight;
	}
}

bool AUTRecastNavMesh::FindBestPath(APawn* Asker, const FNavAgentProperties& AgentProps, FUTNodeEvaluator& NodeEval, const FVector& StartLoc, float& Weight, bool bAllowDetours, TArray<FRouteCacheItem>& NodeRoute, TArray<NavNodeRef>* PolyRoute)
{
	DECLARE_CYCLE_STAT(TEXT("UT node pathing time"), STAT_Navigation_UTPathfinding, STATGROUP_Navigation);

	SCOPE_CYCLE_COUNTER(STAT_Navigation_UTPathfinding);

	NodeRoute.Reset();
	if (PolyRoute != NULL)
	{
		PolyRoute->Reset();
	}
	bool bNeedMoveToStartNode = false;
	NavNodeRef StartPoly = FindNearestPoly(StartLoc, FVector(AgentProps.AgentRadius, AgentProps.AgentRadius, AgentProps.AgentHeight * 0.5f));
	// HACK: handle lifts (see FindLiftPoly())
	if (StartPoly == INVALID_NAVNODEREF)
	{
		StartPoly = FindLiftPoly(Asker, AgentProps);
	}
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
			float ThisWeight = NodeEval.Eval(Asker, AgentProps, CurrentNode->Node, GetPolyCenter(CurrentNode->Poly), CurrentNode->TotalDistance);
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

			if (bNeedMoveToStartNode)
			{
				NodeRoute.Insert(FRouteCacheItem(NextRouteNode->Node, GetPolyCenter(NextRouteNode->Poly), NextRouteNode->Poly), 0);
			}

			FVector RouteGoalLoc = FVector::ZeroVector;
			AActor* RouteGoal = NULL;
			if (NodeEval.GetRouteGoal(RouteGoal, RouteGoalLoc))
			{
				new(NodeRoute) FRouteCacheItem(RouteGoal, RouteGoalLoc, FindNearestPoly(RouteGoalLoc, FVector(AgentProps.AgentRadius, AgentProps.AgentRadius, AgentProps.AgentHeight)));
			}

			if (bAllowDetours && !bNeedMoveToStartNode && Asker != NULL && NodeRoute.Num() > ((RouteGoal != NULL) ? 2 : 1))
			{
				FVector NextLoc = NodeRoute[1].GetLocation(Asker);
				FVector NextDir = (NextLoc - StartLoc).SafeNormal();
				AActor* BestDetour = NULL;
				float BestDetourWeight = 0.0f;
				for (TWeakObjectPtr<AActor> POI : NextRouteNode->Node->POIs)
				{
					if (POI.IsValid())
					{
						AUTPickup* Pickup = Cast<AUTPickup>(POI.Get());
						AUTDroppedPickup* DroppedPickup = Cast<AUTDroppedPickup>(POI.Get());
						if ((Pickup != NULL && Pickup->State.bActive) || DroppedPickup != NULL) // TODO: flag for pickup timing
						{
							// reject detours too far behind desired path
							FVector POILoc = POI->GetActorLocation();
							float Angle = (POILoc - StartLoc).SafeNormal() | NextDir;
							float MaxDist = (Angle > 0.5f) ? FLT_MAX : (3000.0f / (2.0f - Angle));
							float Dist = (POILoc - NextLoc).Size();
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

			if (PolyRoute != NULL)
			{
				dtNavMeshQuery& InternalQuery = GetRecastNavMeshImpl()->SharedNavQuery;

				NavNodeRef CurrentPoly = StartPoly;
				FVector CurrentRecastLoc = Unreal2RecastPoint(StartLoc);

				for (int32 i = 0; i < NodeRoute.Num(); i++)
				{
					float RecastStart[3] = { CurrentRecastLoc.X, CurrentRecastLoc.Y, CurrentRecastLoc.Z };
					FVector RecastEndVect = Unreal2RecastPoint(GetPolyCenter(NodeRoute[i].TargetPoly));
					float RecastEnd[3] = { RecastEndVect.X, RecastEndVect.Y, RecastEndVect.Z };
					dtQueryResult PathData;
					InternalQuery.findPath(CurrentPoly, NodeRoute[i].TargetPoly, RecastStart, RecastEnd, GetDefaultDetourFilter(), PathData, NULL);
					for (int32 j = 0; j < PathData.size(); j++)
					{
						PolyRoute->Add(PathData.getRef(j));
					}

					CurrentRecastLoc = RecastEndVect;
					CurrentPoly = NodeRoute[i].TargetPoly;
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
		FVector RecastEndVect = Unreal2RecastPoint(GetPolyCenter(Target.TargetPoly));
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
		FVector RecastEndVect = Unreal2RecastPoint(GetPolyCenter(PolyRoute.Last()));
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
		Corridor.setCorridor(RecastEnd, PolyRoute.GetTypedData(), PolyRoute.Num());
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

	NavNodeRef StartPoly = FindNearestPoly(OrigStartLoc, FVector(AgentProps.AgentRadius, AgentProps.AgentRadius, AgentProps.AgentHeight * 0.5f));
	// HACK: handle lifts (see FindLiftPoly())
	if (StartPoly == INVALID_NAVNODEREF)
	{
		StartPoly = FindLiftPoly(Asker, AgentProps);
	}
	if (StartPoly == INVALID_NAVNODEREF)
	{
		// we're off the navmesh, but FindBestPath() may have suggested a re-entry point to get us in here
		// try moving directly to target, checking simple trace to make sure we don't have something completely invalid
		FCollisionQueryParams Params(FName(TEXT("GetMovePointsFallback")), false, Asker);
		if (!GetWorld()->SweepTest(OrigStartLoc + FVector(0.0f, 0.0f, AgentProps.AgentHeight), Target.GetLocation(Asker) + FVector(0.0f, 0.0f, AgentProps.AgentHeight), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeBox(FVector(10.0f, 10.0f, 5.0f)), Params))
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

bool AUTRecastNavMesh::HasReachedTarget(APawn* Asker, const FNavAgentProperties& AgentProps, const FRouteCacheItem& Target, const FUTPathLink& PathLink) const
{
	if (Asker == NULL)
	{
		return false;
	}
	else
	{
		// TODO: probably some kind of interface needed here

		AUTPickup* Pickup = NULL;
		if (Target.Actor.IsValid())
		{
			Pickup = Cast<AUTPickup>(Target.Actor.Get());
		}
		if (Pickup != NULL && Pickup->State.bActive)
		{
			return Pickup->IsOverlappingActor(Asker);
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
					FVector RecastExtent = Unreal2RecastPoint(FVector(AgentProps.AgentRadius, AgentProps.AgentRadius, AgentProps.AgentHeight * 0.5f));
					NavNodeRef Polys[16];
					int32 PolyCount = 0;
					if (dtStatusSucceed(InternalQuery.queryPolygons((float*)&RecastAskerLoc, (float*)&RecastExtent, GetDefaultDetourFilter(), Polys, &PolyCount, ARRAY_COUNT(Polys))))
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