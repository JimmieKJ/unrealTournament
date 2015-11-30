// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "AIModulePrivate.h"
#include "Misc/CoreMisc.h"
#include "Net/UnrealNetwork.h"
#include "NavMeshRenderingHelpers.h"
#include "DebugRenderSceneProxy.h"
#include "NavMeshGameplayDebuggerObject.h"


UNavMeshGameplayDebuggerObject::UNavMeshGameplayDebuggerObject(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	LastDataCaptureTime = 0;
}

void UNavMeshGameplayDebuggerObject::CollectDataToReplicate(APlayerController* MyPC, AActor *SelectedActor)
{
	Super::CollectDataToReplicate(MyPC, SelectedActor);

#if ENABLED_GAMEPLAY_DEBUGGER
	UWorld* ActiveWorld = GetWorld();
	if (ActiveWorld && ActiveWorld->TimeSeconds - LastDataCaptureTime > 5)
	{
		ENetMode NetMode = ActiveWorld->GetNetMode();
		LastDataCaptureTime = ActiveWorld->TimeSeconds;
		APawn const *PlayerPawn = MyPC ? MyPC->GetPawnOrSpectator() : nullptr;
		APawn const *SelectedActorAsPawn = Cast<APawn>(SelectedActor);

		APawn const *DestPawn = SelectedActorAsPawn ? SelectedActorAsPawn : (SelectedActor ? Cast<APawn>(SelectedActor) : PlayerPawn);
		if (DestPawn)
		{
			ServerCollectNavmeshData(DestPawn);
		}
	}
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void UNavMeshGameplayDebuggerObject::DrawCollectedData(APlayerController* MyPC, AActor *SelectedActor)
{
	Super::DrawCollectedData(MyPC, SelectedActor);

#if ENABLED_GAMEPLAY_DEBUGGER
	if (LocalReplicationData.RepDataSize != 0 && LocalReplicationData.RepData.Num() != LocalReplicationData.RepDataSize)
	{
		const int32 RepPct = 100 * LocalReplicationData.RepData.Num() / LocalReplicationData.RepDataSize;
		PrintString(DefaultContext, FString::Printf(TEXT("NavMesh Replicating: {red}%d%%\n"), RepPct));
		return;
	}
#endif
}

bool UNavMeshGameplayDebuggerObject::ServerDiscardNavmeshData_Validate()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	return true;
#endif
	return false;
}

namespace NavMeshGameplayDebugger
{
	struct FShortVector
	{
		int16 X;
		int16 Y;
		int16 Z;

		FShortVector()
			:X(0), Y(0), Z(0)
		{

		}

		FShortVector(const FVector& Vec)
			: X(Vec.X)
			, Y(Vec.Y)
			, Z(Vec.Z)
		{

		}
		FShortVector& operator=(const FVector& R)
		{
			X = R.X;
			Y = R.Y;
			Z = R.Z;
			return *this;
		}

		FVector ToVector() const
		{
			return FVector(X, Y, Z);
		}
	};

	struct FOffMeshLinkFlags
	{
		uint8 Radius : 6;
		uint8 Direction : 1;
		uint8 ValidEnds : 1;
	};
	struct FOffMeshLink
	{
		FShortVector Left;
		FShortVector Right;
		FColor Color;
		union
		{
			FOffMeshLinkFlags PackedFlags;
			uint8 ByteFlags;
		};
	};

	struct FAreaPolys
	{
		TArray<int32> Indices;
		FColor Color;
	};

	struct FTileData
	{
		FVector Location;
		TArray<FAreaPolys> Areas;
		TArray<FShortVector> Verts;
		TArray<FOffMeshLink> Links;
	};
}

FArchive& operator<<(FArchive& Ar, NavMeshGameplayDebugger::FShortVector& ShortVector)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	Ar << ShortVector.X;
	Ar << ShortVector.Y;
	Ar << ShortVector.Z;
#endif
	return Ar;
}

FArchive& operator<<(FArchive& Ar, NavMeshGameplayDebugger::FOffMeshLink& Data)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	Ar << Data.Left;
	Ar << Data.Right;
	Ar << Data.Color.R;
	Ar << Data.Color.G;
	Ar << Data.Color.B;
	Ar << Data.ByteFlags;
#endif
	return Ar;
}

FArchive& operator<<(FArchive& Ar, NavMeshGameplayDebugger::FAreaPolys& Data)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	Ar << Data.Indices;
	Ar << Data.Color.R;
	Ar << Data.Color.G;
	Ar << Data.Color.B;
	Data.Color.A = 255;
#endif
	return Ar;
}


FArchive& operator<<(FArchive& Ar, NavMeshGameplayDebugger::FTileData& Data)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	Ar << Data.Areas;
	Ar << Data.Location;
	Ar << Data.Verts;
	Ar << Data.Links;
#endif
	return Ar;
}

void UNavMeshGameplayDebuggerObject::ServerDiscardNavmeshData_Implementation()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	LocalReplicationData.RepData.Empty();
#endif
}

FORCEINLINE bool LineInCorrectDistance(const FVector& PlayerLoc, const FVector& Start, const FVector& End, float CorrectDistance = -1)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	const float MaxDistance = CorrectDistance > 0 ? (CorrectDistance*CorrectDistance) : ARecastNavMesh::GetDrawDistanceSq();

	if ((FVector::DistSquared(Start, PlayerLoc) > MaxDistance || FVector::DistSquared(End, PlayerLoc) > MaxDistance))
	{
		return false;
	}
#endif
	return true;
}

#if WITH_RECAST
ARecastNavMesh* UNavMeshGameplayDebuggerObject::GetNavData(const AActor *TargetActor)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	if (NavSys == NULL)
	{
		return NULL;
	}

	// Try to get the correct nav-mesh relative to the selected actor.
	const APawn* TargetPawn = Cast<APawn>(TargetActor);
	if (TargetPawn != NULL)
	{
		const FNavAgentProperties& NavAgentProperties = TargetPawn->GetNavAgentPropertiesRef();
		return Cast<ARecastNavMesh>(NavSys->GetNavDataForProps(NavAgentProperties));
	}

	// If it wasn't found, just get the main nav-mesh data.
	return Cast<ARecastNavMesh>(NavSys->GetMainNavData(FNavigationSystem::DontCreate));
#endif //ENABLED_GAMEPLAY_DEBUGGER

	return NULL;
}
#endif

void UNavMeshGameplayDebuggerObject::ServerCollectNavmeshData(const AActor *SelectedActor)
{
#if ENABLED_GAMEPLAY_DEBUGGER && WITH_RECAST
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	ARecastNavMesh* NavData = NavSys ? GetNavData(SelectedActor) : nullptr;
	if (NavData == NULL)
	{
		LocalReplicationData.RepData.Empty();
		return;
	}

	const FVector TargetLocation = SelectedActor ? SelectedActor->GetActorLocation() : FVector::ZeroVector;

	TArray<int32> Indices;
	int32 TileX = 0;
	int32 TileY = 0;
	const int32 DeltaX[] = { 0, 1, 1, 0, -1, -1, -1, 0, 1 };
	const int32 DeltaY[] = { 0, 0, 1, 1, 1, 0, -1, -1, -1 };

	NavData->BeginBatchQuery();

	// add 3x3 neighborhood of target
	int32 TargetTileX = 0;
	int32 TargetTileY = 0;
	NavData->GetNavMeshTileXY(TargetLocation, TargetTileX, TargetTileY);
	for (int32 i = 0; i < ARRAY_COUNT(DeltaX); i++)
	{
		const int32 NeiX = TargetTileX + DeltaX[i];
		const int32 NeiY = TargetTileY + DeltaY[i];
		NavData->GetNavMeshTilesAt(NeiX, NeiY, Indices);
	}

	const FNavDataConfig& NavConfig = NavData->GetConfig();
	FColor NavMeshColors[RECAST_MAX_AREAS];
	NavMeshColors[RECAST_DEFAULT_AREA] = NavConfig.Color.DWColor() > 0 ? NavConfig.Color : NavMeshRenderColor_RecastMesh;
	for (uint8 i = 0; i < RECAST_DEFAULT_AREA; ++i)
	{
		NavMeshColors[i] = NavData->GetAreaIDColor(i);
	}

	TArray<uint8> UncompressedBuffer;
	FMemoryWriter ArWriter(UncompressedBuffer);

	int32 NumIndices = Indices.Num();
	ArWriter << NumIndices;

	for (int32 i = 0; i < NumIndices; i++)
	{
		FRecastDebugGeometry NavMeshGeometry;
		NavMeshGeometry.bGatherPolyEdges = false;
		NavMeshGeometry.bGatherNavMeshEdges = false;
		NavData->GetDebugGeometry(NavMeshGeometry, Indices[i]);

		NavMeshGameplayDebugger::FTileData TileData;
		const FBox TileBoundingBox = NavData->GetNavMeshTileBounds(Indices[i]);
		TileData.Location = TileBoundingBox.GetCenter();
		for (int32 VertIndex = 0; VertIndex < NavMeshGeometry.MeshVerts.Num(); ++VertIndex)
		{
			const NavMeshGameplayDebugger::FShortVector SV = NavMeshGeometry.MeshVerts[VertIndex] - TileData.Location;
			TileData.Verts.Add(SV);
		}

		for (int32 iArea = 0; iArea < RECAST_MAX_AREAS; iArea++)
		{
			const int32 NumTris = NavMeshGeometry.AreaIndices[iArea].Num();
			if (NumTris)
			{
				NavMeshGameplayDebugger::FAreaPolys AreaPolys;
				for (int32 AreaIndicesIndex = 0; AreaIndicesIndex < NavMeshGeometry.AreaIndices[iArea].Num(); ++AreaIndicesIndex)
				{
					AreaPolys.Indices.Add(NavMeshGeometry.AreaIndices[iArea][AreaIndicesIndex]);
				}
				AreaPolys.Color = NavMeshColors[iArea];
				TileData.Areas.Add(AreaPolys);
			}
		}

		TileData.Links.Reserve(NavMeshGeometry.OffMeshLinks.Num());
		for (int32 iLink = 0; iLink < NavMeshGeometry.OffMeshLinks.Num(); iLink++)
		{
			const FRecastDebugGeometry::FOffMeshLink& SrcLink = NavMeshGeometry.OffMeshLinks[iLink];
			NavMeshGameplayDebugger::FOffMeshLink Link;
			Link.Left = SrcLink.Left - TileData.Location;
			Link.Right = SrcLink.Right - TileData.Location;
			Link.Color = ((SrcLink.Direction && SrcLink.ValidEnds) || (SrcLink.ValidEnds & FRecastDebugGeometry::OMLE_Left)) ? DarkenColor(NavMeshColors[SrcLink.AreaID]) : NavMeshRenderColor_OffMeshConnectionInvalid;
			Link.PackedFlags.Radius = (int8)SrcLink.Radius;
			Link.PackedFlags.Direction = SrcLink.Direction;
			Link.PackedFlags.ValidEnds = SrcLink.ValidEnds;
			TileData.Links.Add(Link);
		}

		ArWriter << TileData;
	}
	NavData->FinishBatchQuery();
	const ENetMode NetMode = GetWorld()->GetNetMode();
	RequestDataReplication(TEXT("NavMesh"), UncompressedBuffer, NetMode != NM_Standalone ? EGameplayDebuggerReplicate::WithCompressed : EGameplayDebuggerReplicate::WithoutCompression);
#endif
}

void UNavMeshGameplayDebuggerObject::OnDataReplicationComplited(const TArray<uint8>& ReplicatedData)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	MarkRenderStateDirty();
#endif
}

void UNavMeshGameplayDebuggerObject::PrepareNavMeshData(struct FNavMeshSceneProxyData* CurrentData, const UPrimitiveComponent* InComponent, UWorld* World) const
{
#if ENABLED_GAMEPLAY_DEBUGGER && WITH_RECAST
	if (LocalReplicationData.RepDataSize != LocalReplicationData.RepData.Num())
	{
		return;
	}

	if (CurrentData)
	{
		CurrentData->Reset();
		CurrentData->bNeedsNewData = false;

		// read serialized values
		CurrentData->bEnableDrawing = LocalReplicationData.RepData.Num() > 0;
		if (!CurrentData->bEnableDrawing)
		{
			return;
		}

		FMemoryReader ArReader(LocalReplicationData.RepData);

		int32 NumTiles = 0;
		ArReader << NumTiles;

		int32 IndexesOffset = 0;
		for (int32 iTile = 0; iTile < NumTiles; iTile++)
		{
			NavMeshGameplayDebugger::FTileData TileData;
			ArReader << TileData;

			FVector OffsetLocation = TileData.Location;
			TArray<FVector> Verts;
			Verts.Reserve(TileData.Verts.Num());
			for (int32 VertIndex = 0; VertIndex < TileData.Verts.Num(); ++VertIndex)
			{
				const FVector Loc = TileData.Verts[VertIndex].ToVector() + OffsetLocation;
				Verts.Add(Loc);
			}
			CurrentData->Bounds += FBox(Verts);

			for (int32 iArea = 0; iArea < TileData.Areas.Num(); iArea++)
			{
				const NavMeshGameplayDebugger::FAreaPolys& SrcArea = TileData.Areas[iArea];
				FNavMeshSceneProxyData::FDebugMeshData DebugMeshData;
				DebugMeshData.ClusterColor = SrcArea.Color;
				DebugMeshData.ClusterColor.A = 128;

				for (int32 iTri = 0; iTri < SrcArea.Indices.Num(); iTri += 3)
				{
					const int32 Index0 = SrcArea.Indices[iTri + 0];
					const int32 Index1 = SrcArea.Indices[iTri + 1];
					const int32 Index2 = SrcArea.Indices[iTri + 2];

					FVector V0 = Verts[Index0];
					FVector V1 = Verts[Index1];
					FVector V2 = Verts[Index2];
					CurrentData->TileEdgeLines.Add(FDebugRenderSceneProxy::FDebugLine(V0 + CurrentData->NavMeshDrawOffset, V1 + CurrentData->NavMeshDrawOffset, NavMeshRenderColor_Recast_TileEdges));
					CurrentData->TileEdgeLines.Add(FDebugRenderSceneProxy::FDebugLine(V1 + CurrentData->NavMeshDrawOffset, V2 + CurrentData->NavMeshDrawOffset, NavMeshRenderColor_Recast_TileEdges));
					CurrentData->TileEdgeLines.Add(FDebugRenderSceneProxy::FDebugLine(V2 + CurrentData->NavMeshDrawOffset, V0 + CurrentData->NavMeshDrawOffset, NavMeshRenderColor_Recast_TileEdges));

					AddTriangleHelper(DebugMeshData, Index0 + IndexesOffset, Index1 + IndexesOffset, Index2 + IndexesOffset);
				}

				for (int32 iVert = 0; iVert < Verts.Num(); iVert++)
				{
					AddVertexHelper(DebugMeshData, Verts[iVert] + CurrentData->NavMeshDrawOffset);
				}
				CurrentData->MeshBuilders.Add(DebugMeshData);
				IndexesOffset += Verts.Num();
			}

			for (int32 i = 0; i < TileData.Links.Num(); i++)
			{
				const NavMeshGameplayDebugger::FOffMeshLink& SrcLink = TileData.Links[i];

				const FVector V0 = SrcLink.Left.ToVector() + OffsetLocation + CurrentData->NavMeshDrawOffset;
				const FVector V1 = SrcLink.Right.ToVector() + OffsetLocation + CurrentData->NavMeshDrawOffset;
				const FColor LinkColor = SrcLink.Color;

				CacheArc(CurrentData->NavLinkLines, V0, V1, 0.4f, 4, LinkColor, LinkLines_LineThickness);

				const FVector VOffset(0, 0, FVector::Dist(V0, V1) * 1.333f);
				CacheArrowHead(CurrentData->NavLinkLines, V1, V0 + VOffset, 30.f, LinkColor, LinkLines_LineThickness);
				if (SrcLink.PackedFlags.Direction)
				{
					CacheArrowHead(CurrentData->NavLinkLines, V0, V1 + VOffset, 30.f, LinkColor, LinkLines_LineThickness);
				}

				// if the connection as a whole is valid check if there are any of ends is invalid
				if (LinkColor != NavMeshRenderColor_OffMeshConnectionInvalid)
				{
					if (SrcLink.PackedFlags.Direction && (SrcLink.PackedFlags.ValidEnds & FRecastDebugGeometry::OMLE_Left) == 0)
					{
						// left end invalid - mark it
						DrawWireCylinder(CurrentData->NavLinkLines, V0, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), NavMeshRenderColor_OffMeshConnectionInvalid, SrcLink.PackedFlags.Radius, 30 /*NavMesh->AgentMaxStepHeight*/, 16, 0, DefaultEdges_LineThickness);
					}

					if ((SrcLink.PackedFlags.ValidEnds & FRecastDebugGeometry::OMLE_Right) == 0)
					{
						DrawWireCylinder(CurrentData->NavLinkLines, V1, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), NavMeshRenderColor_OffMeshConnectionInvalid, SrcLink.PackedFlags.Radius, 30 /*NavMesh->AgentMaxStepHeight*/, 16, 0, DefaultEdges_LineThickness);
					}
				}
			}
		}
	}
#endif
}

class FDebugRenderSceneProxy* UNavMeshGameplayDebuggerObject::CreateSceneProxy(const UPrimitiveComponent* InComponent, UWorld* World, AActor* SelectedActor)
{
#if ENABLED_GAMEPLAY_DEBUGGER && WITH_RECAST	
	FNavMeshSceneProxyData NewNavmeshRenderData;
	NewNavmeshRenderData.Reset();
	NewNavmeshRenderData.bNeedsNewData = false;
	NewNavmeshRenderData.bEnableDrawing = false;
	PrepareNavMeshData(&NewNavmeshRenderData, InComponent, World);

	return NewNavmeshRenderData.bEnableDrawing ? new FRecastRenderingSceneProxy(InComponent, &NewNavmeshRenderData, true) : nullptr;
#endif
	return nullptr;
}

