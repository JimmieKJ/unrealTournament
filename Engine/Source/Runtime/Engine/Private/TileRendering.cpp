// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TranslucentRendering.cpp: Translucent rendering implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "TileRendering.h"
#include "EngineModule.h"
#include "LocalVertexFactory.h"
#include "MeshBatch.h"
#include "RendererInterface.h"

/** 
* vertex data for a screen quad 
*/
struct FMaterialTileVertex
{
	FVector			Position;
	FPackedNormal	TangentX,
					TangentZ;
	uint32			Color;
	float			U,
					V;

	inline void Initialize(float InX, float InY, float InU, float InV)
	{
		Position.X = InX; Position.Y = InY; Position.Z = 0.0f;
		TangentX = FVector(1, 0, 0); 
		//TangentY = FVector(0, 1, 0); 
		TangentZ = FVector(0, 0, 1);
		// TangentZ.w contains the sign of the tangent basis determinant. Assume +1
		TangentZ.Vector.W = 255;
		Color = FColor(255,255,255,255).DWColor();
		U = InU; V = InV;
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
		VertexBufferRHI = RHICreateVertexBuffer(Size,BUF_Static,CreateInfo);
		// lock it
		void* Buffer = RHILockVertexBuffer(VertexBufferRHI,0,Size,RLM_WriteOnly);
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

void FTileRenderer::DrawTile(FRHICommandListImmediate& RHICmdList, const class FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, bool bNeedsToSwitchVerticalAxis, float X, float Y, float SizeX, float SizeY, float U, float V, float SizeU, float SizeV, bool bIsHitTesting, const FHitProxyId HitProxyId, const FColor InVertexColor)
{
	FMaterialTileVertex DestVertex[4];

	// create verts
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

	// update the FMeshBatch
	FMeshBatch& Mesh = GTileMesh.MeshElement;
	Mesh.UseDynamicData = true;
	Mesh.DynamicVertexData = DestVertex;
	Mesh.MaterialRenderProxy = MaterialRenderProxy;

	GetRendererModule().DrawTileMesh(RHICmdList, View, Mesh, bIsHitTesting, HitProxyId);
}
