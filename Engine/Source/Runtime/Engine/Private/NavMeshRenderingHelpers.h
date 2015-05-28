// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "DebugRenderSceneProxy.h"
#include "Debug/DebugDrawService.h"
#include "DynamicMeshBuilder.h"
#include "AI/Navigation/RecastHelpers.h"
#include "BatchedElements.h"
#include "MeshBatch.h"
#include "Engine/TextureDefines.h"
#include "SceneManagement.h"
#include "AI/Navigation/NavMeshRenderingComponent.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "GenericOctreePublic.h"
#include "GenericOctree.h"

static const FColor NavMeshRenderColor_Recast_TriangleEdges(255,255,255);
static const FColor NavMeshRenderColor_Recast_TileEdges(16,16,16,32);
static const FColor NavMeshRenderColor_Recast_NavMeshEdges(32,63,0,220);
static const FColor NavMeshRenderColor_RecastMesh(140,255,0,164);
static const FColor NavMeshRenderColor_TileBounds(255,255,64,255);
static const FColor NavMeshRenderColor_PathCollidingGeom(255,255,255,40);
static const FColor NavMeshRenderColor_RecastTileBeingRebuilt(255,0,0,64);
static const FColor NavMeshRenderColor_OffMeshConnectionInvalid(64,64,64);

static const float DefaultEdges_LineThickness = 0.0f;
static const float PolyEdges_LineThickness = 1.5f;
static const float NavMeshEdges_LineThickness = 3.5f;
static const float LinkLines_LineThickness = 2.0f;
static const float ClusterLinkLines_LineThickness = 2.0f;

FORCEINLINE FColor DarkenColor(const FColor& Base)
{
	const uint32 Col = Base.DWColor();
	return FColor(((Col >> 1) & 0x007f7f7f) | (Col & 0xff000000));
}

struct FNavMeshSceneProxyData : public TSharedFromThis<FNavMeshSceneProxyData, ESPMode::ThreadSafe>
{
#if WITH_RECAST

	FNavMeshSceneProxyData() : NavMeshDrawOffset(0,0,10.f), bNeedsNewData(true) {}

	void Reset()
	{
		for (int32 Index = 0; Index < RECAST_MAX_AREAS; Index++)
		{
			NavMeshGeometry.AreaIndices[Index].Reset();
		}
		NavMeshGeometry.MeshVerts.Reset();
		for (int32 Index = 0; Index < RECAST_MAX_AREAS; Index++)
		{
			NavMeshGeometry.AreaIndices[Index].Reset();
		}
		NavMeshGeometry.BuiltMeshIndices.Reset();
		NavMeshGeometry.PolyEdges.Reset();
		NavMeshGeometry.NavMeshEdges.Reset();
		NavMeshGeometry.OffMeshLinks.Reset();
		NavMeshGeometry.Clusters.Reset();
		NavMeshGeometry.ClusterLinks.Reset();
		NavMeshGeometry.OffMeshSegments.Reset();
		for (int32 Index = 0; Index < RECAST_MAX_AREAS; Index++)
		{
			NavMeshGeometry.OffMeshSegmentAreas[Index].Reset();
		}
		TileEdgeLines.Reset();
		NavMeshEdgeLines.Reset();
		NavLinkLines.Reset();
		ClusterLinkLines.Reset();
		PathCollidingGeomIndices.Reset();
		PathCollidingGeomVerts.Reset();
		OctreeBounds.Reset();
		MeshBuilders.Reset();
		DebugLabels.Reset();
		Bounds.Init();

		bNeedsNewData = true;
		bDataGathered = false;
		bSkipDistanceCheck = false;
		bDrawClusters = false;
		bEnableDrawing = false;
		bDrawPathCollidingGeometry = false;
	}

	FRecastDebugGeometry NavMeshGeometry;

	TArray<FDebugRenderSceneProxy::FDebugLine> TileEdgeLines;
	TArray<FDebugRenderSceneProxy::FDebugLine> NavMeshEdgeLines;
	TArray<FDebugRenderSceneProxy::FDebugLine> NavLinkLines;
	TArray<FDebugRenderSceneProxy::FDebugLine> ClusterLinkLines;

	TArray<int32> PathCollidingGeomIndices;
	TArray<FDynamicMeshVertex> PathCollidingGeomVerts;

	TArray<FBoxCenterAndExtent>	OctreeBounds;
		
	FColor NavMeshColors[RECAST_MAX_AREAS];

	struct FDebugMeshData
	{
		TArray<FDynamicMeshVertex> Vertices;
		TArray<int32> Indices;
		FColor ClusterColor;
	};
	TArray<FDebugMeshData> MeshBuilders;

	struct FDebugThickLine : FDebugRenderSceneProxy::FDebugLine
	{
		FDebugThickLine(const FVector &InStart, const FVector &InEnd, const FColor &InColor, float InThickness)
			: FDebugLine(InStart, InEnd, InColor)
			, Thickness(InThickness)
		{

		}

		float Thickness;
	};
	TArray<FDebugThickLine> ThickLineItems;;

	struct FDebugText
	{
		const FVector Location;
		const FString Text;
		FDebugText(const FVector& InLocation, const FString& InText) : Location(InLocation), Text(InText)
		{}
	};
	TArray<FDebugText> DebugLabels;
	FBox Bounds;

	FVector NavMeshDrawOffset;
	uint32 bDataGathered : 1;
	uint32 bSkipDistanceCheck : 1;
	uint32 bDrawClusters : 1;
	uint32 bEnableDrawing:1;
	uint32 bDrawPathCollidingGeometry:1;
	uint32 bNeedsNewData:1;
#endif // WITH_RECAST
};

#if WITH_RECAST

FORCEINLINE bool LineInView(const FVector& Start, const FVector& End, const FSceneView* View, bool bUseDistanceCheck)
{
	if (bUseDistanceCheck && (FVector::DistSquared(Start, View->ViewMatrices.ViewOrigin) > ARecastNavMesh::GetDrawDistanceSq()
		|| FVector::DistSquared(End, View->ViewMatrices.ViewOrigin) > ARecastNavMesh::GetDrawDistanceSq()))
	{
		return false;
	}

	for(int32 PlaneIdx=0;PlaneIdx<View->ViewFrustum.Planes.Num();++PlaneIdx)
	{
		const FPlane& CurPlane = View->ViewFrustum.Planes[PlaneIdx];
		if(CurPlane.PlaneDot(Start) > 0.f && CurPlane.PlaneDot(End) > 0.f)
		{
			return false;
		}
	}

	return true;
}

FORCEINLINE bool LineInCorrectDistance(const FVector& Start, const FVector& End, const FSceneView* View, float CorrectDistance = -1)
{
	const float MaxDistance = CorrectDistance > 0 ? (CorrectDistance*CorrectDistance) : ARecastNavMesh::GetDrawDistanceSq();

	if ((FVector::DistSquared(Start, View->ViewMatrices.ViewOrigin) > MaxDistance
		|| FVector::DistSquared(End, View->ViewMatrices.ViewOrigin) > MaxDistance))
	{
		return false;
	}
	return true;
}

class FNavMeshIndexBuffer : public FIndexBuffer
{
public:
	TArray<int32> Indices;

	virtual void InitRHI() override
	{
		if (Indices.Num() > 0)
		{
			FRHIResourceCreateInfo CreateInfo;
			IndexBufferRHI = RHICreateIndexBuffer(sizeof(int32), Indices.Num() * sizeof(int32), BUF_Static, CreateInfo);

			// Write the indices to the index buffer.
			void* Buffer = RHILockIndexBuffer(IndexBufferRHI, 0, Indices.Num() * sizeof(int32), RLM_WriteOnly);
			FMemory::Memcpy(Buffer, Indices.GetData(), Indices.Num() * sizeof(int32));
			RHIUnlockIndexBuffer(IndexBufferRHI);
		}
	}
};

/** Vertex Buffer */
class FNavMeshVertexBuffer : public FVertexBuffer
{
public:
	TArray<FDynamicMeshVertex> Vertices;

	virtual void InitRHI() override
	{
		if (Vertices.Num() > 0)
		{
			FRHIResourceCreateInfo CreateInfo;
			VertexBufferRHI = RHICreateVertexBuffer(Vertices.Num() * sizeof(FDynamicMeshVertex), BUF_Static, CreateInfo);

			// Copy the vertex data into the vertex buffer.
			void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI, 0, Vertices.Num() * sizeof(FDynamicMeshVertex), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, Vertices.GetData(), Vertices.Num() * sizeof(FDynamicMeshVertex));
			RHIUnlockVertexBuffer(VertexBufferRHI);
		}
	}

};

/** Vertex Factory */
class FNavMeshVertexFactory : public FLocalVertexFactory
{
public:

	FNavMeshVertexFactory()
	{}


	/** Initialization */
	void Init(const FNavMeshVertexBuffer* VertexBuffer)
	{
		if (IsInRenderingThread())
		{
			// Initialize the vertex factory's stream components.
			DataType NewData;
			NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, Position, VET_Float3);
			NewData.TextureCoordinates.Add(
				FVertexStreamComponent(VertexBuffer, STRUCT_OFFSET(FDynamicMeshVertex, TextureCoordinate), sizeof(FDynamicMeshVertex), VET_Float2)
				);
			NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, TangentX, VET_PackedNormal);
			NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, TangentZ, VET_PackedNormal);
			SetData(NewData);
		}
		else
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				InitNavMeshVertexFactory,
				FNavMeshVertexFactory*, VertexFactory, this,
				const FNavMeshVertexBuffer*, VertexBuffer, VertexBuffer,
				{
				// Initialize the vertex factory's stream components.
				DataType NewData;
				NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, Position, VET_Float3);
				NewData.TextureCoordinates.Add(
					FVertexStreamComponent(VertexBuffer, STRUCT_OFFSET(FDynamicMeshVertex, TextureCoordinate), sizeof(FDynamicMeshVertex), VET_Float2)
					);
				NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, TangentX, VET_PackedNormal);
				NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, TangentZ, VET_PackedNormal);
				VertexFactory->SetData(NewData);
				});
		}
	}
};


class FRecastRenderingSceneProxy : public FDebugRenderSceneProxy
{
public:
	FRecastRenderingSceneProxy(const UPrimitiveComponent* InComponent, FNavMeshSceneProxyData* InProxyData,  bool ForceToRender = false) 
		: FDebugRenderSceneProxy(InComponent)
		, bRequestedData(false)
		, bForceRendering(ForceToRender)
	{
		ProxyData.Reset();
		if (InProxyData)
		{
			ProxyData = *InProxyData;
		}
		RenderingComponent = Cast<const UNavMeshRenderingComponent>(InComponent);

		const int32 NumberOfMeshes = ProxyData.MeshBuilders.Num();
		if (!NumberOfMeshes)
		{
			return;
		}

		MeshColors.Reserve(NumberOfMeshes + (ProxyData.bDrawPathCollidingGeometry && ProxyData.PathCollidingGeomVerts.Num() > 0 ? 1 : 0));
		MeshBatchElements.Reserve(NumberOfMeshes);
		const FMaterialRenderProxy* ParentMaterial = GEngine->DebugMeshMaterial->GetRenderProxy(false);
		for (int32 Index = 0; Index < NumberOfMeshes; ++Index)
		{
			const auto& CurrentMeshBuilder = ProxyData.MeshBuilders[Index];

			FMeshBatchElement Element;
			Element.FirstIndex = IndexBuffer.Indices.Num();
			Element.NumPrimitives = FMath::FloorToInt(CurrentMeshBuilder.Indices.Num() / 3);
			Element.MinVertexIndex = VertexBuffer.Vertices.Num();
			Element.MaxVertexIndex = Element.MinVertexIndex + CurrentMeshBuilder.Vertices.Num() - 1;
			Element.IndexBuffer = &IndexBuffer;
			MeshBatchElements.Add(Element);

			MeshColors.Add(FColoredMaterialRenderProxy(ParentMaterial, CurrentMeshBuilder.ClusterColor));

			VertexBuffer.Vertices.Append( CurrentMeshBuilder.Vertices );
			IndexBuffer.Indices.Append( CurrentMeshBuilder.Indices );
		}

		MeshColors.Add(FColoredMaterialRenderProxy(ParentMaterial, NavMeshRenderColor_PathCollidingGeom));

		VertexFactory.Init(&VertexBuffer);
		BeginInitResource(&IndexBuffer);
		BeginInitResource(&VertexBuffer);
		BeginInitResource(&VertexFactory);
	}

	virtual ~FRecastRenderingSceneProxy() 
	{
		VertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();

		ProxyData.Reset();
	}


	void RegisterDebugDrawDelgate() override
	{
		DebugTextDrawingDelegate = FDebugDrawDelegate::CreateRaw(this, &FRecastRenderingSceneProxy::DrawDebugLabels);
		DebugTextDrawingDelegateHandle = UDebugDrawService::Register(TEXT("Navigation"), DebugTextDrawingDelegate);
	}

	void UnregisterDebugDrawDelgate() override
	{
		if (DebugTextDrawingDelegate.IsBound())
		{
			UDebugDrawService::Unregister(DebugTextDrawingDelegateHandle);
		}
	}

	FORCEINLINE uint8 GetBit(int32 v, uint8 bit) const
	{
		return (v & (1 << bit)) >> bit;
	}

	FORCEINLINE FColor GetClusterColor(int32 Idx) const
	{
		const uint8 r = 1 + GetBit(Idx, 1) + GetBit(Idx, 3) * 2;
		const uint8 g = 1 + GetBit(Idx, 2) + GetBit(Idx, 4) * 2;
		const uint8 b = 1 + GetBit(Idx, 0) + GetBit(Idx, 5) * 2;
		return FColor(r*63, g*63, b*63, 164);
	}

	void DrawDebugBox(FPrimitiveDrawInterface* PDI, FVector const& Center, FVector const& Box, FColor const& Color) const
	{
		// no debug line drawing on dedicated server
		if (PDI != NULL)
		{
			PDI->DrawLine(Center + FVector(Box.X, Box.Y, Box.Z), Center + FVector(Box.X, -Box.Y, Box.Z), Color, SDPG_World);
			PDI->DrawLine(Center + FVector(Box.X, -Box.Y, Box.Z), Center + FVector(-Box.X, -Box.Y, Box.Z), Color, SDPG_World);
			PDI->DrawLine(Center + FVector(-Box.X, -Box.Y, Box.Z), Center + FVector(-Box.X, Box.Y, Box.Z), Color, SDPG_World);
			PDI->DrawLine(Center + FVector(-Box.X, Box.Y, Box.Z), Center + FVector(Box.X, Box.Y, Box.Z), Color, SDPG_World);

			PDI->DrawLine(Center + FVector(Box.X, Box.Y, -Box.Z), Center + FVector(Box.X, -Box.Y, -Box.Z), Color, SDPG_World);
			PDI->DrawLine(Center + FVector(Box.X, -Box.Y, -Box.Z), Center + FVector(-Box.X, -Box.Y, -Box.Z), Color, SDPG_World);
			PDI->DrawLine(Center + FVector(-Box.X, -Box.Y, -Box.Z), Center + FVector(-Box.X, Box.Y, -Box.Z), Color, SDPG_World);
			PDI->DrawLine(Center + FVector(-Box.X, Box.Y, -Box.Z), Center + FVector(Box.X, Box.Y, -Box.Z), Color, SDPG_World);

			PDI->DrawLine(Center + FVector(Box.X, Box.Y, Box.Z), Center + FVector(Box.X, Box.Y, -Box.Z), Color, SDPG_World);
			PDI->DrawLine(Center + FVector(Box.X, -Box.Y, Box.Z), Center + FVector(Box.X, -Box.Y, -Box.Z), Color, SDPG_World);
			PDI->DrawLine(Center + FVector(-Box.X, -Box.Y, Box.Z), Center + FVector(-Box.X, -Box.Y, -Box.Z), Color, SDPG_World);
			PDI->DrawLine(Center + FVector(-Box.X, Box.Y, Box.Z), Center + FVector(-Box.X, Box.Y, -Box.Z), Color, SDPG_World);
		}
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RecastRenderingSceneProxy_GetDynamicMeshElements);

		if (!ProxyData.bEnableDrawing) //check if we have any data to render
		{
			return;
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				const bool bVisible = !!View->Family->EngineShowFlags.Navigation || bForceRendering;
				if (!bVisible)
				{
					continue;
				}
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

				const bool bSkipDistanceCheck = GIsEditor && (GEngine->GetDebugLocalPlayer() == NULL);

				for (int32 Index = 0; Index < ProxyData.OctreeBounds.Num(); ++Index)
				{
					const FBoxCenterAndExtent& Bounds = ProxyData.OctreeBounds[Index];
					DrawDebugBox(PDI, Bounds.Center, Bounds.Extent, FColor::White);
				}

				// Draw Mesh
				if (MeshBatchElements.Num())
				{
					for (int32 Index = 0; Index < MeshBatchElements.Num(); ++Index)
					{
						if (MeshBatchElements[Index].NumPrimitives == 0)
						{
							continue;
						}

					FMeshBatch& Mesh = Collector.AllocateMesh();
					FMeshBatchElement& BatchElement = Mesh.Elements[0];
					BatchElement = MeshBatchElements[Index];
					BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(FMatrix::Identity, GetBounds(), GetLocalBounds(), false, UseEditorDepthTest());

						Mesh.bWireframe = false;
						Mesh.VertexFactory = &VertexFactory;
						Mesh.MaterialRenderProxy = &MeshColors[Index];
						Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
						Mesh.Type = PT_TriangleList;
						Mesh.DepthPriorityGroup = SDPG_World;
						Mesh.bCanApplyViewModeOverrides = false;
						Collector.AddMesh(ViewIndex, Mesh);
					}

					if (ProxyData.PathCollidingGeomIndices.Num() > 2)
					{
						FDynamicMeshBuilder MeshBuilder;
						MeshBuilder.AddVertices(ProxyData.PathCollidingGeomVerts);
						MeshBuilder.AddTriangles(ProxyData.PathCollidingGeomIndices);

						MeshBuilder.GetMesh(FMatrix::Identity, &MeshColors[MeshBatchElements.Num()], SDPG_World, false, false, ViewIndex, Collector);
					}
				}

				int32 Num = ProxyData.NavMeshEdgeLines.Num();
				PDI->AddReserveLines(SDPG_World, Num, false, false);
				PDI->AddReserveLines(SDPG_Foreground, Num, false, true);
				for (int32 Index = 0; Index < Num; ++Index)
				{
					const FDebugLine &Line = ProxyData.NavMeshEdgeLines[Index];
					if (LineInView(Line.Start, Line.End, View, false))
					{
						if (LineInCorrectDistance(Line.Start, Line.End, View))
						{
							PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_World, NavMeshEdges_LineThickness, 0, true);
						}
						else if (GIsEditor)
						{
							PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_Foreground, DefaultEdges_LineThickness, 0, true);
						}
					}
				}


				Num = ProxyData.ClusterLinkLines.Num();
				PDI->AddReserveLines(SDPG_World, Num, false, false);
				PDI->AddReserveLines(SDPG_Foreground, Num, false, true);
				for (int32 Index = 0; Index < Num; ++Index)
				{
					const FDebugLine &Line = ProxyData.ClusterLinkLines[Index];
					if (LineInView(Line.Start, Line.End, View, false))
					{
						if (LineInCorrectDistance(Line.Start, Line.End, View))
						{
							PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_World, ClusterLinkLines_LineThickness, 0, true);
						}
						else if (GIsEditor)
						{
							PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_Foreground, DefaultEdges_LineThickness, 0, true);
						}
					}
				}

				Num = ProxyData.TileEdgeLines.Num();
				PDI->AddReserveLines(SDPG_World, Num, false, false);
				PDI->AddReserveLines(SDPG_Foreground, Num, false, true);
				for (int32 Index = 0; Index < Num; ++Index)
				{
					const FDebugLine &Line = ProxyData.TileEdgeLines[Index];
					if (LineInView(Line.Start, Line.End, View, false))
					{
						if (LineInCorrectDistance(Line.Start, Line.End, View))
						{
							PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_World, PolyEdges_LineThickness, 0, true);
						}
						else if (GIsEditor)
						{
							PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_Foreground, DefaultEdges_LineThickness, 0, true);
						}
					}
				}

				Num = ProxyData.NavLinkLines.Num();
				PDI->AddReserveLines(SDPG_World, Num, false, false);
				PDI->AddReserveLines(SDPG_Foreground, Num, false, true);
				for (int32 Index = 0; Index < Num; ++Index)
				{
					const FDebugLine &Line = ProxyData.NavLinkLines[Index];
					if (LineInView(Line.Start, Line.End, View, false))
					{
						if (LineInCorrectDistance(Line.Start, Line.End, View))
						{
							PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_World, LinkLines_LineThickness, 0, true);
						}
						else if (GIsEditor)
						{
							PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_Foreground, DefaultEdges_LineThickness, 0, true);
						}
					}
				}

				Num = ProxyData.ThickLineItems.Num();
				PDI->AddReserveLines(SDPG_Foreground, Num, false, true);
				for (int32 Index = 0; Index < Num; ++Index)
				{
					const auto &Line = ProxyData.ThickLineItems[Index];
					if (LineInView(Line.Start, Line.End, View, false))
					{
						if (LineInCorrectDistance(Line.Start, Line.End, View))
						{
							PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_World, Line.Thickness, 0, true);
						}
						else if (GIsEditor)
						{
							PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_Foreground, DefaultEdges_LineThickness, 0, true);
						}
					}
				}

			}
		}
	}

	FORCEINLINE bool PointInView(const FVector& Location, const FSceneView* View)
	{
		return View->ViewFrustum.IntersectBox(Location, FVector::ZeroVector);
	}

	void DrawDebugLabels(UCanvas* Canvas, APlayerController*) override
	{
		const bool bVisible = (Canvas && Canvas->SceneView && !!Canvas->SceneView->Family->EngineShowFlags.Navigation) || bForceRendering;
		if (ProxyData.bNeedsNewData == true || ProxyData.bEnableDrawing == false || !bVisible || ProxyData.DebugLabels.Num() == 0)
		{
			return;
		}

		const FColor OldDrawColor = Canvas->DrawColor;
		Canvas->SetDrawColor(FColor::White);
		const FSceneView* View = Canvas->SceneView;
		UFont* Font = GEngine->GetSmallFont();
		const FNavMeshSceneProxyData::FDebugText* DebugText = ProxyData.DebugLabels.GetData();
		for (int32 i = 0 ; i < ProxyData.DebugLabels.Num(); ++i, ++DebugText)
		{
			if (PointInView(DebugText->Location, View))
			{
				const FVector ScreenLoc = Canvas->Project(DebugText->Location);
				Canvas->DrawText(Font, DebugText->Text, ScreenLoc.X, ScreenLoc.Y);
			}
		}

		Canvas->SetDrawColor(OldDrawColor);
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) override
	{
		const bool bVisible = !!View->Family->EngineShowFlags.Navigation || bForceRendering;
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = bVisible && IsShown(View);
		Result.bDynamicRelevance = true;
		Result.bNormalTranslucencyRelevance = bVisible && IsShown(View);
		return Result;
	}

	virtual uint32 GetMemoryFootprint( void ) const override { return sizeof( *this ) + GetAllocatedSize(); }
	uint32 GetAllocatedSize( void ) const 
	{ 
		return FDebugRenderSceneProxy::GetAllocatedSize() + 
			(sizeof(FNavMeshSceneProxyData) + ProxyData.NavMeshGeometry.GetAllocatedSize()
			+ ProxyData.TileEdgeLines.GetAllocatedSize() + ProxyData.NavMeshEdgeLines.GetAllocatedSize() + ProxyData.NavLinkLines.GetAllocatedSize()
			+ ProxyData.PathCollidingGeomIndices.GetAllocatedSize() + ProxyData.PathCollidingGeomVerts.GetAllocatedSize()
			+ ProxyData.DebugLabels.GetAllocatedSize() + ProxyData.ClusterLinkLines.GetAllocatedSize() + ProxyData.MeshBuilders.GetAllocatedSize()
			+ IndexBuffer.Indices.GetAllocatedSize() + VertexBuffer.Vertices.GetAllocatedSize() + MeshColors.GetAllocatedSize() + MeshBatchElements.GetAllocatedSize());
	}

private:
	FNavMeshSceneProxyData ProxyData;
	
	FNavMeshIndexBuffer IndexBuffer;
	FNavMeshVertexBuffer VertexBuffer;
	FNavMeshVertexFactory VertexFactory;

	TArray <FColoredMaterialRenderProxy> MeshColors;
	TArray<FMeshBatchElement> MeshBatchElements;

	FDebugDrawDelegate DebugTextDrawingDelegate;
	FDelegateHandle DebugTextDrawingDelegateHandle;
	TWeakObjectPtr<UNavMeshRenderingComponent> RenderingComponent;
	uint32 bRequestedData : 1;
	uint32 bForceRendering : 1;
};
#endif // WITH_RECAST

FORCEINLINE void AppendGeometry(TArray<FVector>& OutVertexBuffer, TArray<int32>& OutIndexBuffer,
								const float* Coords, int32 NumVerts, const int32* Faces, int32 NumFaces)
{
	const int32 VertIndexBase = OutVertexBuffer.Num();
	for (int32 VertIdx = 0; VertIdx < NumVerts*3; VertIdx+=3)
	{
		OutVertexBuffer.Add(Recast2UnrealPoint(&Coords[VertIdx]));
	}

	const int32 FirstNewFaceVertexIndex = OutIndexBuffer.Num();
	const uint32 NumIndices = NumFaces * 3;
	OutIndexBuffer.AddUninitialized(NumIndices);
	for (uint32 Index = 0; Index < NumIndices; ++Index)
	{
		OutIndexBuffer[FirstNewFaceVertexIndex + Index] = VertIndexBase + Faces[Index];
	}
}

FORCEINLINE FVector EvalArc(const FVector& Org, const FVector& Dir, const float h, const float u)
{
	FVector Pt = Org + Dir * u;
	Pt.Z += h * (1-(u*2-1)*(u*2-1));

	return Pt;
}

FORCEINLINE void CacheArc(TArray<FDebugRenderSceneProxy::FDebugLine>& DebugLines, const FVector& Start, const FVector& End, const float Height, const uint32 Segments, const FLinearColor& Color, float LineThickness=0)
{
	if (Segments == 0)
	{
		return;
	}

	const float ArcPtsScale = 1.0f / (float)Segments;
	const FVector Dir = End - Start;
	const float Length = Dir.Size();

	FVector Prev = Start;
	for (uint32 i = 1; i <= Segments; ++i)
	{
		const float u = i * ArcPtsScale;
		const FVector Pt = EvalArc(Start, Dir, Length*Height, u);

		DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(Prev, Pt, Color));
		Prev = Pt;
	}
}

FORCEINLINE void CacheArrowHead(TArray<FDebugRenderSceneProxy::FDebugLine>& DebugLines, const FVector& Tip, const FVector& Origin, const float Size, const FLinearColor& Color, float LineThickness=0)
{
	FVector Ax, Ay, Az(0,1,0);
	Ay = Origin - Tip;
	Ay.Normalize();
	Ax = FVector::CrossProduct(Az, Ay);

	FHitProxyId HitProxyId;
	DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(Tip, FVector(Tip.X + Ay.X*Size + Ax.X*Size/3, Tip.Y + Ay.Y*Size + Ax.Y*Size/3, Tip.Z + Ay.Z*Size + Ax.Z*Size/3), Color));
	DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(Tip, FVector(Tip.X + Ay.X*Size - Ax.X*Size/3, Tip.Y + Ay.Y*Size - Ax.Y*Size/3, Tip.Z + Ay.Z*Size - Ax.Z*Size/3), Color));
}

FORCEINLINE void DrawWireCylinder(TArray<FDebugRenderSceneProxy::FDebugLine>& DebugLines,const FVector& Base,const FVector& X,const FVector& Y,const FVector& Z,FColor Color,float Radius,float HalfHeight,int32 NumSides,uint8 DepthPriority, float LineThickness=0)
{
	const float	AngleDelta = 2.0f * PI / NumSides;
	FVector	LastVertex = Base + X * Radius;

	FHitProxyId HitProxyId;
	for(int32 SideIndex = 0;SideIndex < NumSides;SideIndex++)
	{
		const FVector Vertex = Base + (X * FMath::Cos(AngleDelta * (SideIndex + 1)) + Y * FMath::Sin(AngleDelta * (SideIndex + 1))) * Radius;

		DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(LastVertex - Z * HalfHeight,Vertex - Z * HalfHeight,Color));
		DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(LastVertex + Z * HalfHeight,Vertex + Z * HalfHeight,Color));
		DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(LastVertex - Z * HalfHeight,LastVertex + Z * HalfHeight,Color));

		LastVertex = Vertex;
	}
}

FORCEINLINE uint8 GetBit(int32 v, uint8 bit)
{
	return (v & (1 << bit)) >> bit;
}

FORCEINLINE FColor GetClusterColor(int32 Idx)
{
	uint8 r = 1 + GetBit(Idx, 1) + GetBit(Idx, 3) * 2;
	uint8 g = 1 + GetBit(Idx, 2) + GetBit(Idx, 4) * 2;
	uint8 b = 1 + GetBit(Idx, 0) + GetBit(Idx, 5) * 2;
	return FColor(r*63, g*63, b*63, 164);
}

#if WITH_RECAST
FORCEINLINE static void AddVertexHelper(FNavMeshSceneProxyData::FDebugMeshData& MeshData, const FVector& Pos, const FColor Color = FColor::White)
{
	const int32 VertexIndex = MeshData.Vertices.Num();
	FDynamicMeshVertex* Vertex = new(MeshData.Vertices) FDynamicMeshVertex;
	Vertex->Position = Pos;
	Vertex->TextureCoordinate = FVector2D::ZeroVector;
	Vertex->TangentX = FVector(1.0f, 0.0f, 0.0f);
	Vertex->TangentZ = FVector(0.0f, 1.0f, 0.0f);
	// store the sign of the determinant in TangentZ.W (-1=0,+1=255)
	Vertex->TangentZ.Vector.W = 255;
	Vertex->Color = Color;
}

FORCEINLINE static void AddTriangleHelper(FNavMeshSceneProxyData::FDebugMeshData& MeshData, int32 V0, int32 V1, int32 V2)
{
	MeshData.Indices.Add(V0);
	MeshData.Indices.Add(V1);
	MeshData.Indices.Add(V2);
}
#endif	//WITH_RECAST
