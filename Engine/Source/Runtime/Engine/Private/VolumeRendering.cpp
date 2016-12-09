// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VolumeRendering.cpp: Volume rendering implementation.
=============================================================================*/

#include "VolumeRendering.h"
#include "ScreenRendering.h"
#include "RHIStaticStates.h"

IMPLEMENT_SHADER_TYPE(,FWriteToSliceGS,TEXT("TranslucentLightingShaders"),TEXT("WriteToSliceMainGS"),SF_Geometry);
IMPLEMENT_SHADER_TYPE(,FWriteToSliceVS,TEXT("TranslucentLightingShaders"),TEXT("WriteToSliceMainVS"),SF_Vertex);

/** Vertex buffer used for rendering into a volume texture. */
class FVolumeRasterizeVertexBuffer : public FVertexBuffer
{
public:

	virtual void InitRHI() override
	{
		// Used as a non-indexed triangle strip, so 4 vertices per quad
		const uint32 Size = 4 * sizeof(FScreenVertex);
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(Size, BUF_Static, CreateInfo, Buffer);		
		FScreenVertex* DestVertex = (FScreenVertex*)Buffer;

		// Setup a full - render target quad
		// A viewport and UVScaleBias will be used to implement rendering to a sub region
		DestVertex[0].Position = FVector2D(1, -GProjectionSignY);
		DestVertex[0].UV = FVector2D(1, 1);
		DestVertex[1].Position = FVector2D(1, GProjectionSignY);
		DestVertex[1].UV = FVector2D(1, 0);
		DestVertex[2].Position = FVector2D(-1, -GProjectionSignY);
		DestVertex[2].UV = FVector2D(0, 1);
		DestVertex[3].Position = FVector2D(-1, GProjectionSignY);
		DestVertex[3].UV = FVector2D(0, 0);

		RHIUnlockVertexBuffer(VertexBufferRHI);      
	}
};

TGlobalResource<FVolumeRasterizeVertexBuffer> GVolumeRasterizeVertexBuffer;

/** Draws a quad per volume texture slice to the subregion of the volume texture specified. */
ENGINE_API void RasterizeToVolumeTexture(FRHICommandList& RHICmdList, FVolumeBounds VolumeBounds)
{
	// Setup the viewport to only render to the given bounds
	RHICmdList.SetViewport(VolumeBounds.MinX, VolumeBounds.MinY, 0, VolumeBounds.MaxX, VolumeBounds.MaxY, 0);
	RHICmdList.SetStreamSource(0, GVolumeRasterizeVertexBuffer.VertexBufferRHI, sizeof(FScreenVertex), 0);
	const int32 NumInstances = VolumeBounds.MaxZ - VolumeBounds.MinZ;
	// Render a quad per slice affected by the given bounds
	RHICmdList.DrawPrimitive(PT_TriangleStrip, 0, 2, NumInstances);
}
