// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TileRendering.cpp: Tile rendering implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "TileRendering.h"
#include "EngineModule.h"
#include "LocalVertexFactory.h"
#include "MeshBatch.h"
#include "RendererInterface.h"
#include "SceneUtils.h"
#include "CanvasTypes.h"

/** 
* vertex data for a screen quad 
*/
struct FMaterialTileVertex
{
	FVector			Position;
	FPackedNormal	TangentX;
	FPackedNormal	TangentZ;
	uint32			Color;
	float			U;
	float			V;

	inline void Initialize(float InX, float InY, float InU, float InV)
	{
		Position.X = InX; 
		Position.Y = InY; 
		Position.Z = 0.0f;
		TangentX = FVector(1, 0, 0); 
		//TangentY = FVector(0, 1, 0); 
		TangentZ = FVector(0, 0, 1);
		// TangentZ.w contains the sign of the tangent basis determinant. Assume +1
		TangentZ.Vector.W = 255;
		Color = FColor(255,255,255,255).DWColor();
		U = InU; 
		V = InV;
	}
};

/** 
* Vertex buffer
*/
class FMaterialTileVertexBuffer : public FVertexBuffer
{
public:
	/** 
	* Initialize the RHI for this rendering resource 
	*/
	virtual void InitRHI() override
	{
		// used with a tristrip, so only 4 vertices are needed
		uint32 Size = 4 * sizeof(FMaterialTileVertex);
		// create vertex buffer
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(Size,BUF_Static,CreateInfo, Buffer);
				
		// first vertex element
		FMaterialTileVertex* DestVertex = (FMaterialTileVertex*)Buffer;

		// fill out the verts
		DestVertex[0].Initialize(1, -1, 1, 1);
		DestVertex[1].Initialize(1, 1, 1, 0);
		DestVertex[2].Initialize(-1, -1, 0, 1);
		DestVertex[3].Initialize(-1, 1, 0, 0);

		// Unlock the buffer.
		RHIUnlockVertexBuffer(VertexBufferRHI);        
	}
};
TGlobalResource<FMaterialTileVertexBuffer> GTileRendererVertexBuffer;

/**
 * Vertex factory for rendering tiles.
 */
class FTileVertexFactory : public FLocalVertexFactory
{
public:

	/** Default constructor. */
	FTileVertexFactory()
	{
		FLocalVertexFactory::DataType Data;
		// position
		Data.PositionComponent = FVertexStreamComponent(
			&GTileRendererVertexBuffer,STRUCT_OFFSET(FMaterialTileVertex,Position),sizeof(FMaterialTileVertex),VET_Float3);
		// tangents
		Data.TangentBasisComponents[0] = FVertexStreamComponent(
			&GTileRendererVertexBuffer,STRUCT_OFFSET(FMaterialTileVertex,TangentX),sizeof(FMaterialTileVertex),VET_PackedNormal);
		Data.TangentBasisComponents[1] = FVertexStreamComponent(
			&GTileRendererVertexBuffer,STRUCT_OFFSET(FMaterialTileVertex,TangentZ),sizeof(FMaterialTileVertex),VET_PackedNormal);
		// color
		Data.ColorComponent = FVertexStreamComponent(
			&GTileRendererVertexBuffer,STRUCT_OFFSET(FMaterialTileVertex,Color),sizeof(FMaterialTileVertex),VET_Color);
		// UVs
		Data.TextureCoordinates.Add(FVertexStreamComponent(
			&GTileRendererVertexBuffer,STRUCT_OFFSET(FMaterialTileVertex,U),sizeof(FMaterialTileVertex),VET_Float2));

		// update the data
		SetData(Data);
	}
};
TGlobalResource<FTileVertexFactory> GTileVertexFactory;

/**
 * Mesh used to render tiles.
 */
class FTileMesh : public FRenderResource
{
public:

	/** The mesh element. */
	FMeshBatch MeshElement;

	virtual void InitRHI() override
	{
		FMeshBatchElement& BatchElement = MeshElement.Elements[0];
		MeshElement.VertexFactory = &GTileVertexFactory;
		MeshElement.DynamicVertexStride = sizeof(FMaterialTileVertex);
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = 2;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = 3;
		MeshElement.ReverseCulling = false;
		MeshElement.UseDynamicData = true;
		MeshElement.Type = PT_TriangleStrip;
		MeshElement.DepthPriorityGroup = SDPG_Foreground;
		BatchElement.PrimitiveUniformBufferResource = &GIdentityPrimitiveUniformBuffer;
	}

	virtual void ReleaseRHI() override
	{
		MeshElement.Elements[0].PrimitiveUniformBuffer.SafeRelease();
	}
};
TGlobalResource<FTileMesh> GTileMesh;

static void CreateTileVerices(FMaterialTileVertex DestVertex[4], const class FSceneView& View, bool bNeedsToSwitchVerticalAxis, float X, float Y, float SizeX, float SizeY, float U, float V, float SizeU, float SizeV, const FColor InVertexColor)
{
	if (bNeedsToSwitchVerticalAxis)
	{
		DestVertex[0].Initialize(X + SizeX, View.ViewRect.Height() - (Y + SizeY), U + SizeU, V + SizeV);
		DestVertex[1].Initialize(X, View.ViewRect.Height() - (Y + SizeY), U, V + SizeV);
		DestVertex[2].Initialize(X + SizeX, View.ViewRect.Height() - Y, U + SizeU, V);
		DestVertex[3].Initialize(X, View.ViewRect.Height() - Y, U, V);		
	}
	else
	{
		DestVertex[0].Initialize(X + SizeX, Y, U + SizeU, V);
		DestVertex[1].Initialize(X, Y, U, V);
		DestVertex[2].Initialize(X + SizeX, Y + SizeY, U + SizeU, V + SizeV);
		DestVertex[3].Initialize(X, Y + SizeY, U, V + SizeV);
	}

	DestVertex[0].Color = InVertexColor.DWColor();
	DestVertex[1].Color = InVertexColor.DWColor();
	DestVertex[2].Color = InVertexColor.DWColor();
	DestVertex[3].Color = InVertexColor.DWColor();
}

void FTileRenderer::DrawTile(FRHICommandListImmediate& RHICmdList, const class FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, bool bNeedsToSwitchVerticalAxis, float X, float Y, float SizeX, float SizeY, float U, float V, float SizeU, float SizeV, bool bIsHitTesting, const FHitProxyId HitProxyId, const FColor InVertexColor)
{
	// create verts
	FMaterialTileVertex DestVertex[4];
	CreateTileVerices(DestVertex, View, bNeedsToSwitchVerticalAxis, X, Y, SizeX, SizeY, U, V, SizeU, SizeV, InVertexColor);

	// update the FMeshBatch
	FMeshBatch& Mesh = GTileMesh.MeshElement;
	Mesh.UseDynamicData = true;
	Mesh.DynamicVertexData = DestVertex;
	Mesh.MaterialRenderProxy = MaterialRenderProxy;

	GetRendererModule().DrawTileMesh(RHICmdList, View, Mesh, bIsHitTesting, HitProxyId);
}

void FTileRenderer::DrawRotatedTile(FRHICommandListImmediate& RHICmdList, const class FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, bool bNeedsToSwitchVerticalAxis, const FQuat& Rotation, float X, float Y, float SizeX, float SizeY, float U, float V, float SizeU, float SizeV, bool bIsHitTesting, const FHitProxyId HitProxyId, const FColor InVertexColor)
{
	// create verts
	FMaterialTileVertex DestVertex[4];
	CreateTileVerices(DestVertex, View, bNeedsToSwitchVerticalAxis, X, Y, SizeX, SizeY, U, V, SizeU, SizeV, InVertexColor);
	
	// rotate tile using view center as origin
	FIntPoint ViewRectSize = View.ViewRect.Size();
	FVector ViewRectOrigin = FVector(ViewRectSize.X*0.5f, ViewRectSize.Y*0.5f, 0.f);
	for (int32 i = 0; i < 4; ++i)
	{
		DestVertex[i].Position = Rotation.RotateVector(DestVertex[i].Position - ViewRectOrigin) + ViewRectOrigin;
		DestVertex[i].TangentX = Rotation.RotateVector(DestVertex[i].TangentX);
		DestVertex[i].TangentZ = Rotation.RotateVector(DestVertex[i].TangentZ);
	}

	// update the FMeshBatch
	FMeshBatch& Mesh = GTileMesh.MeshElement;
	Mesh.UseDynamicData = true;
	Mesh.DynamicVertexData = DestVertex;
	Mesh.MaterialRenderProxy = MaterialRenderProxy;

	GetRendererModule().DrawTileMesh(RHICmdList, View, Mesh, bIsHitTesting, HitProxyId);
}

bool FCanvasTileRendererItem::Render_RenderThread(FRHICommandListImmediate& RHICmdList, const FCanvas* Canvas)
{
	float CurrentRealTime = 0.f;
	float CurrentWorldTime = 0.f;
	float DeltaWorldTime = 0.f;

	if (!bFreezeTime)
	{
		CurrentRealTime = Canvas->GetCurrentRealTime();
		CurrentWorldTime = Canvas->GetCurrentWorldTime();
		DeltaWorldTime = Canvas->GetCurrentDeltaWorldTime();
	}

	checkSlow(Data);
	// current render target set for the canvas
	const FRenderTarget* CanvasRenderTarget = Canvas->GetRenderTarget();
	FSceneViewFamily* ViewFamily = new FSceneViewFamily(FSceneViewFamily::ConstructionValues(
		CanvasRenderTarget,
		nullptr,
		FEngineShowFlags(ESFIM_Game))
		.SetWorldTimes(CurrentWorldTime, DeltaWorldTime, CurrentRealTime)
		.SetGammaCorrection(CanvasRenderTarget->GetDisplayGamma()));

	FIntRect ViewRect(FIntPoint(0, 0), CanvasRenderTarget->GetSizeXY());

	// make a temporary view
	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = ViewFamily;
	ViewInitOptions.SetViewRectangle(ViewRect);
	ViewInitOptions.ViewOrigin = FVector::ZeroVector;
	ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
	ViewInitOptions.ProjectionMatrix = Data->Transform.GetMatrix();
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.OverlayColor = FLinearColor::White;

	FSceneView* View = new FSceneView(ViewInitOptions);
	
	bool bNeedsToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(Canvas->GetShaderPlatform()) && !Canvas->GetAllowSwitchVerticalAxis();

	for (int32 TileIdx = 0; TileIdx < Data->Tiles.Num(); TileIdx++)
	{
		const FRenderData::FTileInst& Tile = Data->Tiles[TileIdx];
		FTileRenderer::DrawTile(
			RHICmdList,
			*View,
			Data->MaterialRenderProxy,
			bNeedsToSwitchVerticalAxis,
			Tile.X, Tile.Y, Tile.SizeX, Tile.SizeY,
			Tile.U, Tile.V, Tile.SizeU, Tile.SizeV,
			Canvas->IsHitTesting(), Tile.HitProxyId,
			Tile.InColor
			);
	}

	delete View->Family;
	delete View;
	if (Canvas->GetAllowedModes() & FCanvas::Allow_DeleteOnRender)
	{
		delete Data;
	}
	if (Canvas->GetAllowedModes() & FCanvas::Allow_DeleteOnRender)
	{
		Data = NULL;
	}
	return true;
}

bool FCanvasTileRendererItem::Render_GameThread(const FCanvas* Canvas)
{
	float CurrentRealTime = 0.f;
	float CurrentWorldTime = 0.f;
	float DeltaWorldTime = 0.f;

	if (!bFreezeTime)
	{
		CurrentRealTime = Canvas->GetCurrentRealTime();
		CurrentWorldTime = Canvas->GetCurrentWorldTime();
		DeltaWorldTime = Canvas->GetCurrentDeltaWorldTime();
	}

	checkSlow(Data);
	// current render target set for the canvas
	const FRenderTarget* CanvasRenderTarget = Canvas->GetRenderTarget();
	FSceneViewFamily* ViewFamily = new FSceneViewFamily(FSceneViewFamily::ConstructionValues(
		CanvasRenderTarget,
		NULL,
		FEngineShowFlags(ESFIM_Game))
		.SetWorldTimes(CurrentWorldTime, DeltaWorldTime, CurrentRealTime)
		.SetGammaCorrection(CanvasRenderTarget->GetDisplayGamma()));

	FIntRect ViewRect(FIntPoint(0, 0), CanvasRenderTarget->GetSizeXY());

	// make a temporary view
	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = ViewFamily;
	ViewInitOptions.SetViewRectangle(ViewRect);
	ViewInitOptions.ViewOrigin = FVector::ZeroVector;
	ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
	ViewInitOptions.ProjectionMatrix = Data->Transform.GetMatrix();
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.OverlayColor = FLinearColor::White;

	FSceneView* View = new FSceneView(ViewInitOptions);

	bool bNeedsToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(Canvas->GetShaderPlatform()) && !Canvas->GetAllowSwitchVerticalAxis();
	struct FDrawTileParameters
	{
		FSceneView* View;
		FRenderData* RenderData;
		uint32 bIsHitTesting : 1;
		uint32 bNeedsToSwitchVerticalAxis : 1;
		uint32 AllowedCanvasModes;
	};
	FDrawTileParameters DrawTileParameters =
	{
		View,
		Data,
		Canvas->IsHitTesting() ? uint32(1) : uint32(0),
		bNeedsToSwitchVerticalAxis ? uint32(1) : uint32(0),
		Canvas->GetAllowedModes()
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		DrawTileCommand,
		FDrawTileParameters, Parameters, DrawTileParameters,
		{
		SCOPED_DRAW_EVENT(RHICmdList, CanvasDrawTile);
		for (int32 TileIdx = 0; TileIdx < Parameters.RenderData->Tiles.Num(); TileIdx++)
		{
			const FRenderData::FTileInst& Tile = Parameters.RenderData->Tiles[TileIdx];
			FTileRenderer::DrawTile(
				RHICmdList,
				*Parameters.View,
				Parameters.RenderData->MaterialRenderProxy,
				Parameters.bNeedsToSwitchVerticalAxis,
				Tile.X, Tile.Y, Tile.SizeX, Tile.SizeY,
				Tile.U, Tile.V, Tile.SizeU, Tile.SizeV,
				Parameters.bIsHitTesting, Tile.HitProxyId,
				Tile.InColor);
		}

		delete Parameters.View->Family;
		delete Parameters.View;
		if (Parameters.AllowedCanvasModes & FCanvas::Allow_DeleteOnRender)
		{
			delete Parameters.RenderData;
		}
		});
	if (Canvas->GetAllowedModes() & FCanvas::Allow_DeleteOnRender)
	{
		Data = NULL;
	}
	return true;
}
