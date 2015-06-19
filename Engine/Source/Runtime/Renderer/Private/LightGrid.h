// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightGrid.h: screen space light culling
=============================================================================*/

#ifndef __LIGHTGRID_H__
#define __LIGHTGRID_H__

#pragma once

/**
 * Vertex buffer used to hold light information for screen space light culling
 */
class FLightGridVertexBuffer : public FVertexBuffer
{
public:
	const static uint32 MaxSize = 512 * 1024;
	TArray<uint32> CPUData;

	/**
	 * Initializes the vertex buffer from the input
	 */
	void UpdateGPUFromCPUData()
	{
		check( IsInRenderingThread() );
		InitResource();
		if(CPUData.Num())
		{
			check( IsInRenderingThread() );

			const uint32 Size = CPUData.Num();

			check(Size <= MaxSize);

			const uint32 BufferSize = Size * sizeof(uint32);

			uint32* RESTRICT Src = &CPUData[0];
			uint32* RESTRICT Dest = (uint32*)RHILockVertexBuffer( VertexBufferRHI, 0, BufferSize, RLM_WriteOnly );

			for(uint32 i = 0; i < Size; ++i)
			{
				*Dest++ = *Src++;
			}

			RHIUnlockVertexBuffer(VertexBufferRHI);
		}
	}

	/** Shader resource view of the vertex buffer. */
	FShaderResourceViewRHIRef VertexBufferSRV;

private:

	/** Release RHI resources. */
	virtual void ReleaseRHI() override
	{
		VertexBufferSRV.SafeRelease();
		FVertexBuffer::ReleaseRHI();
	}

	/** Initialize RHI resources. */
	virtual void InitRHI() override
	{
		const int32 BufferStride = sizeof(uint32);
		const int32 BufferSize = MaxSize * BufferStride;
		uint32 Flags = BUF_Volatile | /*BUF_KeepCPUAccessible | */BUF_ShaderResource;
		FRHIResourceCreateInfo CreateInfo;
		VertexBufferRHI = RHICreateVertexBuffer(BufferSize, Flags, CreateInfo);
		VertexBufferSRV = RHICreateShaderResourceView(VertexBufferRHI, BufferStride, PF_R32_UINT);
	}
};

extern TGlobalResource<FLightGridVertexBuffer> GLightGridVertexBuffer;

#endif // __LIGHTGRID_H__