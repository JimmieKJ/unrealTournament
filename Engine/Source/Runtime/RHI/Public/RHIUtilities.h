// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "RHICommandList.h"

/** Encapsulates a GPU read/write buffer with its UAV and SRV. */
struct FRWBuffer
{
	FVertexBufferRHIRef Buffer;
	FUnorderedAccessViewRHIRef UAV;
	FShaderResourceViewRHIRef SRV;
	uint32 NumBytes;

	FRWBuffer(): NumBytes(0) {}

	// @param AdditionalUsage passed down to RHICreateVertexBuffer(), get combined with "BUF_UnorderedAccess | BUF_ShaderResource" e.g. BUF_Static
	void Initialize(uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, uint32 AdditionalUsage = 0)
	{
		check(GMaxRHIFeatureLevel == ERHIFeatureLevel::SM5);
		NumBytes = BytesPerElement * NumElements;
		FRHIResourceCreateInfo CreateInfo;
		Buffer = RHICreateVertexBuffer(NumBytes, BUF_UnorderedAccess | BUF_ShaderResource | AdditionalUsage, CreateInfo);
		UAV = RHICreateUnorderedAccessView(Buffer, Format);
		SRV = RHICreateShaderResourceView(Buffer, BytesPerElement, Format);
	}

	void Release()
	{
		NumBytes = 0;
		Buffer.SafeRelease();
		UAV.SafeRelease();
		SRV.SafeRelease();
	}
};

/** Encapsulates a GPU read buffer with its SRV. */
struct FReadBuffer
{
	FVertexBufferRHIRef Buffer;
	FShaderResourceViewRHIRef SRV;
	uint32 NumBytes;

	FReadBuffer(): NumBytes(0) {}

	void Initialize(uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, uint32 AdditionalUsage = 0)
	{
		check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4);
		NumBytes = BytesPerElement * NumElements;
		FRHIResourceCreateInfo CreateInfo;
		Buffer = RHICreateVertexBuffer(NumBytes, BUF_ShaderResource | AdditionalUsage, CreateInfo);
		SRV = RHICreateShaderResourceView(Buffer, BytesPerElement, Format);
	}

	void Release()
	{
		NumBytes = 0;
		Buffer.SafeRelease();
		SRV.SafeRelease();
	}
};

/** Encapsulates a GPU read/write structured buffer with its UAV and SRV. */
struct FRWBufferStructured
{
	FStructuredBufferRHIRef Buffer;
	FUnorderedAccessViewRHIRef UAV;
	FShaderResourceViewRHIRef SRV;
	uint32 NumBytes;

	FRWBufferStructured(): NumBytes(0) {}

	void Initialize(uint32 BytesPerElement, uint32 NumElements, uint32 AdditionalUsage = 0, bool bUseUavCounter = false, bool bAppendBuffer = false)
	{
		check(GMaxRHIFeatureLevel == ERHIFeatureLevel::SM5);
		NumBytes = BytesPerElement * NumElements;
		FRHIResourceCreateInfo CreateInfo;
		Buffer = RHICreateStructuredBuffer(BytesPerElement, NumBytes, BUF_UnorderedAccess | BUF_ShaderResource | AdditionalUsage, CreateInfo);
		UAV = RHICreateUnorderedAccessView(Buffer, bUseUavCounter, bAppendBuffer);
		SRV = RHICreateShaderResourceView(Buffer);
	}

	void Release()
	{
		NumBytes = 0;
		Buffer.SafeRelease();
		UAV.SafeRelease();
		SRV.SafeRelease();
	}
};

/** Encapsulates a GPU read/write ByteAddress buffer with its UAV and SRV. */
struct FRWBufferByteAddress
{
	FStructuredBufferRHIRef Buffer;
	FUnorderedAccessViewRHIRef UAV;
	FShaderResourceViewRHIRef SRV;
	uint32 NumBytes;

	FRWBufferByteAddress(): NumBytes(0) {}

	void Initialize(uint32 InNumBytes, uint32 AdditionalUsage = 0)
	{
		NumBytes = InNumBytes;
		check(GMaxRHIFeatureLevel == ERHIFeatureLevel::SM5);
		check( NumBytes % 4 == 0 );
		FRHIResourceCreateInfo CreateInfo;
		Buffer = RHICreateStructuredBuffer(4, NumBytes, BUF_UnorderedAccess | BUF_ShaderResource | BUF_ByteAddressBuffer | AdditionalUsage, CreateInfo);
		UAV = RHICreateUnorderedAccessView(Buffer, false, false);
		SRV = RHICreateShaderResourceView(Buffer);
	}

	void Release()
	{
		NumBytes = 0;
		Buffer.SafeRelease();
		UAV.SafeRelease();
		SRV.SafeRelease();
	}
};

struct FDynamicReadBuffer : public FReadBuffer
{
	/** Pointer to the vertex buffer mapped in main memory. */
	uint8* MappedBuffer;

	/** Default constructor. */
	FDynamicReadBuffer()
		: MappedBuffer(nullptr)
	{
	}

	~FDynamicReadBuffer()
	{
		Release();
	}

	virtual void Initialize(uint32 BytesPerElement, uint32 NumElements, EPixelFormat Format, uint32 AdditionalUsage = 0)
	{
		ensure(
			AdditionalUsage & (BUF_Dynamic | BUF_Volatile) &&								// buffer should be Dynamic or Volatile
			(AdditionalUsage & (BUF_Dynamic | BUF_Volatile)) ^ (BUF_Dynamic | BUF_Volatile) // buffer should not be both
			);

		FReadBuffer::Initialize(BytesPerElement, NumElements, Format, AdditionalUsage);
	}

	/**
	* Locks the vertex buffer so it may be written to.
	*/
	void Lock()
	{
		check(MappedBuffer == nullptr);
		check(IsValidRef(Buffer));
		MappedBuffer = (uint8*)RHILockVertexBuffer(Buffer, 0, NumBytes, RLM_WriteOnly);
	}

	/**
	* Unocks the buffer so the GPU may read from it.
	*/
	void Unlock()
	{
		check(MappedBuffer);
		check(IsValidRef(Buffer));
		RHIUnlockVertexBuffer(Buffer);
		MappedBuffer = nullptr;
	}
};

/**
 * Convert the ESimpleRenderTargetMode into usable values 
 * @todo: Can we easily put this into a .cpp somewhere?
 */
inline void DecodeRenderTargetMode(ESimpleRenderTargetMode Mode, ERenderTargetLoadAction& ColorLoadAction, ERenderTargetStoreAction& ColorStoreAction, ERenderTargetLoadAction& DepthLoadAction, ERenderTargetStoreAction& DepthStoreAction, FExclusiveDepthStencil DepthStencilUsage)
{
	// set defaults
	ColorStoreAction = ERenderTargetStoreAction::EStore;
	DepthStoreAction = ERenderTargetStoreAction::EStore;	

	switch (Mode)
	{
	case ESimpleRenderTargetMode::EExistingColorAndDepth:
		ColorLoadAction = ERenderTargetLoadAction::ELoad;
		DepthLoadAction = ERenderTargetLoadAction::ELoad;
		break;
	case ESimpleRenderTargetMode::EUninitializedColorAndDepth:
		ColorLoadAction = ERenderTargetLoadAction::ENoAction;
		DepthLoadAction = ERenderTargetLoadAction::ENoAction;
		break;
	case ESimpleRenderTargetMode::EUninitializedColorExistingDepth:
		ColorLoadAction = ERenderTargetLoadAction::ENoAction;
		DepthLoadAction = ERenderTargetLoadAction::ELoad;
		break;
	case ESimpleRenderTargetMode::EUninitializedColorClearDepth:
		ColorLoadAction = ERenderTargetLoadAction::ENoAction;
		DepthLoadAction = ERenderTargetLoadAction::EClear;
		break;
	case ESimpleRenderTargetMode::EClearColorExistingDepth:
		ColorLoadAction = ERenderTargetLoadAction::EClear;
		DepthLoadAction = ERenderTargetLoadAction::ELoad;
		break;
	case ESimpleRenderTargetMode::EClearColorAndDepth:
		ColorLoadAction = ERenderTargetLoadAction::EClear;
		DepthLoadAction = ERenderTargetLoadAction::EClear;
		break;
	case ESimpleRenderTargetMode::EExistingContents_NoDepthStore:
		ColorLoadAction = ERenderTargetLoadAction::ELoad;
		DepthLoadAction = ERenderTargetLoadAction::ELoad;
		DepthStoreAction = ERenderTargetStoreAction::ENoAction;
		break;
	case ESimpleRenderTargetMode::EExistingColorAndClearDepth:
		ColorLoadAction = ERenderTargetLoadAction::ELoad;
		DepthLoadAction = ERenderTargetLoadAction::EClear;
		break;
	default:
		UE_LOG(LogRHI, Fatal, TEXT("Using a ESimpleRenderTargetMode that wasn't decoded in DecodeRenderTargetMode [value = %d]"), (int32)Mode);
	}

	//if we aren't writing to depth, there's no reason to store it back out again.  Should save some bandwidth on mobile platforms.
	if (!DepthStencilUsage.IsDepthWrite())
	{
		DepthStoreAction = ERenderTargetStoreAction::ENoAction;
	}
}

inline void TransitionSetRenderTargetsHelper(FRHICommandList& RHICmdList, FTextureRHIParamRef NewRenderTarget, FTextureRHIParamRef NewDepthStencilTarget, FExclusiveDepthStencil DepthStencilAccess)
{
	int32 TransitionIndex = 0;
	FTextureRHIParamRef Transitions[2];
	if (NewRenderTarget)
	{
		Transitions[TransitionIndex] = NewRenderTarget;
		++TransitionIndex;
	}
	if (NewDepthStencilTarget && DepthStencilAccess.IsDepthWrite())
	{
		Transitions[TransitionIndex] = NewDepthStencilTarget;
		++TransitionIndex;
	}
	RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, Transitions, TransitionIndex);
}

inline void TransitionSetRenderTargetsHelper(FRHICommandList& RHICmdList, uint32 NumRenderTargets, const FTextureRHIParamRef* NewRenderTargetsRHI, const FTextureRHIParamRef NewDepthStencilTargetRHI, FExclusiveDepthStencil DepthStencilAccess)
{
	FTextureRHIParamRef Transitions[MaxSimultaneousRenderTargets + 1];
	int32 TransitionIndex = 0;
	for (uint32 Index = 0; Index < NumRenderTargets; Index++)
	{
		if (NewRenderTargetsRHI[Index])
		{
			Transitions[TransitionIndex] = NewRenderTargetsRHI[Index];
			++TransitionIndex;
		}
	}

	if (NewDepthStencilTargetRHI && DepthStencilAccess.IsDepthWrite())
	{
		Transitions[TransitionIndex] = NewDepthStencilTargetRHI;
		++TransitionIndex;
	}	
	RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, Transitions, TransitionIndex);
}

/** Helper for the common case of using a single color and depth render target. */
inline void SetRenderTarget(FRHICommandList& RHICmdList, FTextureRHIParamRef NewRenderTarget, FTextureRHIParamRef NewDepthStencilTarget, bool bWritableBarrier = false)
{
	FRHIRenderTargetView RTV(NewRenderTarget);
	FRHIDepthRenderTargetView DepthRTV(NewDepthStencilTarget);

	//make these rendertargets safely writable
	if (bWritableBarrier)
	{
		TransitionSetRenderTargetsHelper(RHICmdList, NewRenderTarget, NewDepthStencilTarget, FExclusiveDepthStencil::DepthWrite_StencilWrite);
	}
	RHICmdList.SetRenderTargets(1, &RTV, &DepthRTV, 0, NULL);
}

/** Helper for the common case of using a single color and depth render target. */
inline void SetRenderTarget(FRHICommandList& RHICmdList, FTextureRHIParamRef NewRenderTarget, FTextureRHIParamRef NewDepthStencilTarget, ESimpleRenderTargetMode Mode, FExclusiveDepthStencil DepthStencilAccess = FExclusiveDepthStencil::DepthWrite_StencilWrite, bool bWritableBarrier = false)
{
	ERenderTargetLoadAction ColorLoadAction, DepthLoadAction;
	ERenderTargetStoreAction ColorStoreAction, DepthStoreAction;	
	DecodeRenderTargetMode(Mode, ColorLoadAction, ColorStoreAction, DepthLoadAction, DepthStoreAction, DepthStencilAccess);

	//make these rendertargets safely writable
	if (bWritableBarrier)
	{
		TransitionSetRenderTargetsHelper(RHICmdList, NewRenderTarget, NewDepthStencilTarget, DepthStencilAccess);
	}

	// now make the FRHISetRenderTargetsInfo that encapsulates all of the info
	FRHIRenderTargetView ColorView(NewRenderTarget, 0, -1, ColorLoadAction, ColorStoreAction);
	FRHISetRenderTargetsInfo Info(1, &ColorView, FRHIDepthRenderTargetView(NewDepthStencilTarget, DepthLoadAction, DepthStoreAction, DepthStencilAccess));	
	RHICmdList.SetRenderTargetsAndClear(Info);
}

/** Helper for the common case of using a single color and depth render target, with a mip index for the color target. */
inline void SetRenderTarget(FRHICommandList& RHICmdList, FTextureRHIParamRef NewRenderTarget, int32 MipIndex, FTextureRHIParamRef NewDepthStencilTarget, bool bWritableBarrier = false)
{
	FRHIRenderTargetView RTV(NewRenderTarget, MipIndex, -1);
	FRHIDepthRenderTargetView DepthRTV(NewDepthStencilTarget);
	
	//make these rendertargets safely writable
	if (bWritableBarrier)
	{
		TransitionSetRenderTargetsHelper(RHICmdList, NewRenderTarget, NewDepthStencilTarget, FExclusiveDepthStencil::DepthWrite_StencilWrite);
	}
	RHICmdList.SetRenderTargets(1, &RTV, &DepthRTV, 0, nullptr);
}

/** Helper for the common case of using a single color and depth render target, with a mip index for the color target. */
inline void SetRenderTarget(FRHICommandList& RHICmdList, FTextureRHIParamRef NewRenderTarget, int32 MipIndex, int32 ArraySliceIndex, FTextureRHIParamRef NewDepthStencilTarget, bool bWritableBarrier = false)
{
	FRHIRenderTargetView RTV(NewRenderTarget, MipIndex, ArraySliceIndex);
	FRHIDepthRenderTargetView DepthRTV(NewDepthStencilTarget);

	//make these rendertargets safely writable
	if (bWritableBarrier)
	{
		TransitionSetRenderTargetsHelper(RHICmdList, NewRenderTarget, NewDepthStencilTarget, FExclusiveDepthStencil::DepthWrite_StencilWrite);
	}
	RHICmdList.SetRenderTargets(1, &RTV, &DepthRTV, 0, nullptr);
}

/** Helper that converts FTextureRHIParamRef's into FRHIRenderTargetView's. */
inline void SetRenderTargets(
	FRHICommandList& RHICmdList,
	uint32 NewNumSimultaneousRenderTargets, 
	const FTextureRHIParamRef* NewRenderTargetsRHI,
	FTextureRHIParamRef NewDepthStencilTargetRHI,
	uint32 NewNumUAVs,
	const FUnorderedAccessViewRHIParamRef* UAVs,
	bool bWritableBarrier = false
	)
{
	FRHIRenderTargetView RTVs[MaxSimultaneousRenderTargets];
	for (uint32 Index = 0; Index < NewNumSimultaneousRenderTargets; Index++)
	{
		RTVs[Index] = FRHIRenderTargetView(NewRenderTargetsRHI[Index]);
	
	}

	//make these rendertargets safely writable
	if (bWritableBarrier)
	{
		TransitionSetRenderTargetsHelper(RHICmdList, NewNumSimultaneousRenderTargets, NewRenderTargetsRHI, NewDepthStencilTargetRHI, FExclusiveDepthStencil::DepthWrite_StencilWrite);
	}

	FRHIDepthRenderTargetView DepthRTV(NewDepthStencilTargetRHI);
	RHICmdList.SetRenderTargets(NewNumSimultaneousRenderTargets, RTVs, &DepthRTV, NewNumUAVs, UAVs);
}

/** Helper that converts FTextureRHIParamRef's into FRHIRenderTargetView's. */
inline void SetRenderTargets(
	FRHICommandList& RHICmdList,
	uint32 NewNumSimultaneousRenderTargets,
	const FTextureRHIParamRef* NewRenderTargetsRHI,
	FTextureRHIParamRef NewDepthStencilTargetRHI,
	ESimpleRenderTargetMode Mode,
	FExclusiveDepthStencil DepthStencilAccess,
	bool bWritableBarrier = false
	)
{
	ERenderTargetLoadAction ColorLoadAction, DepthLoadAction;
	ERenderTargetStoreAction ColorStoreAction, DepthStoreAction;	
	DecodeRenderTargetMode(Mode, ColorLoadAction, ColorStoreAction, DepthLoadAction, DepthStoreAction, DepthStencilAccess);

	FRHIRenderTargetView RTVs[MaxSimultaneousRenderTargets];
		
	for (uint32 Index = 0; Index < NewNumSimultaneousRenderTargets; Index++)
	{
		RTVs[Index] = FRHIRenderTargetView(NewRenderTargetsRHI[Index], 0, -1, ColorLoadAction, ColorStoreAction);	
	}

	//make these rendertargets safely writable
	if (bWritableBarrier)
	{
		TransitionSetRenderTargetsHelper(RHICmdList, NewNumSimultaneousRenderTargets, NewRenderTargetsRHI, NewDepthStencilTargetRHI, DepthStencilAccess);
	}

	FRHIDepthRenderTargetView DepthRTV(NewDepthStencilTargetRHI, DepthLoadAction, DepthStoreAction, DepthStencilAccess);
	RHICmdList.SetRenderTargets(NewNumSimultaneousRenderTargets, RTVs, &DepthRTV, 0, nullptr);
}

/**
 * Creates 1 or 2 textures with the same dimensions/format.
 * If the RHI supports textures that can be used as both shader resources and render targets,
 * and bForceSeparateTargetAndShaderResource=false, then a single texture is created.
 * Otherwise two textures are created, one of them usable as a shader resource and resolve target, and one of them usable as a render target.
 * Two texture references are always returned, but they may reference the same texture.
 * If two different textures are returned, the render-target texture must be manually copied to the shader-resource texture.
 */
inline void RHICreateTargetableShaderResource2D(
	uint32 SizeX,
	uint32 SizeY,
	uint8 Format,	
	uint32 NumMips,
	uint32 Flags,
	uint32 TargetableTextureFlags,
	bool bForceSeparateTargetAndShaderResource,
	FRHIResourceCreateInfo& CreateInfo,
	FTexture2DRHIRef& OutTargetableTexture,
	FTexture2DRHIRef& OutShaderResourceTexture,
	uint32 NumSamples=1
	)
{
	// Ensure none of the usage flags are passed in.
	check(!(Flags & TexCreate_RenderTargetable));
	check(!(Flags & TexCreate_ResolveTargetable));
	check(!(Flags & TexCreate_ShaderResource));

	// Ensure that all of the flags provided for the targetable texture are not already passed in Flags.
	check(!(Flags & TargetableTextureFlags));

	// Ensure that the targetable texture is either render or depth-stencil targetable.
	check(TargetableTextureFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable | TexCreate_UAV));

	if (NumSamples > 1)
	{
		bForceSeparateTargetAndShaderResource = RHISupportsSeparateMSAAAndResolveTextures(GMaxRHIShaderPlatform);
	}

	if (!bForceSeparateTargetAndShaderResource/* && GSupportsRenderDepthTargetableShaderResources*/)
	{
		// Create a single texture that has both TargetableTextureFlags and TexCreate_ShaderResource set.
		OutTargetableTexture = OutShaderResourceTexture = RHICreateTexture2D(SizeX, SizeY, Format, NumMips, NumSamples, Flags | TargetableTextureFlags | TexCreate_ShaderResource, CreateInfo);
	}
	else
	{
		uint32 ResolveTargetableTextureFlags = TexCreate_ResolveTargetable;
		if (TargetableTextureFlags & TexCreate_DepthStencilTargetable)
		{
			ResolveTargetableTextureFlags |= TexCreate_DepthStencilResolveTarget;
		}
		// Create a texture that has TargetableTextureFlags set, and a second texture that has TexCreate_ResolveTargetable and TexCreate_ShaderResource set.
		OutTargetableTexture = RHICreateTexture2D(SizeX, SizeY, Format, NumMips, NumSamples, Flags | TargetableTextureFlags, CreateInfo);
		OutShaderResourceTexture = RHICreateTexture2D(SizeX, SizeY, Format, NumMips, 1, Flags | ResolveTargetableTextureFlags | TexCreate_ShaderResource, CreateInfo);
	}
}

/**
 * Creates 1 or 2 textures with the same dimensions/format.
 * If the RHI supports textures that can be used as both shader resources and render targets,
 * and bForceSeparateTargetAndShaderResource=false, then a single texture is created.
 * Otherwise two textures are created, one of them usable as a shader resource and resolve target, and one of them usable as a render target.
 * Two texture references are always returned, but they may reference the same texture.
 * If two different textures are returned, the render-target texture must be manually copied to the shader-resource texture.
 */
inline void RHICreateTargetableShaderResourceCube(
	uint32 LinearSize,
	uint8 Format,
	uint32 NumMips,
	uint32 Flags,
	uint32 TargetableTextureFlags,
	bool bForceSeparateTargetAndShaderResource,
	FRHIResourceCreateInfo& CreateInfo,
	FTextureCubeRHIRef& OutTargetableTexture,
	FTextureCubeRHIRef& OutShaderResourceTexture
	)
{
	// Ensure none of the usage flags are passed in.
	check(!(Flags & TexCreate_RenderTargetable));
	check(!(Flags & TexCreate_ResolveTargetable));
	check(!(Flags & TexCreate_ShaderResource));

	// Ensure that all of the flags provided for the targetable texture are not already passed in Flags.
	check(!(Flags & TargetableTextureFlags));

	// Ensure that the targetable texture is either render or depth-stencil targetable.
	check(TargetableTextureFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable));

	// ES2 doesn't support resolve operations.
	bForceSeparateTargetAndShaderResource &= (GMaxRHIFeatureLevel > ERHIFeatureLevel::ES2);

	if(!bForceSeparateTargetAndShaderResource/* && GSupportsRenderDepthTargetableShaderResources*/)
	{
		// Create a single texture that has both TargetableTextureFlags and TexCreate_ShaderResource set.
		OutTargetableTexture = OutShaderResourceTexture = RHICreateTextureCube(LinearSize, Format, NumMips, Flags | TargetableTextureFlags | TexCreate_ShaderResource, CreateInfo);
	}
	else
	{
		// Create a texture that has TargetableTextureFlags set, and a second texture that has TexCreate_ResolveTargetable and TexCreate_ShaderResource set.
		OutTargetableTexture = RHICreateTextureCube(LinearSize, Format, NumMips, Flags | TargetableTextureFlags, CreateInfo);
		OutShaderResourceTexture = RHICreateTextureCube(LinearSize, Format, NumMips, Flags | TexCreate_ResolveTargetable | TexCreate_ShaderResource, CreateInfo);
	}
}

/**
 * Creates 1 or 2 textures with the same dimensions/format.
 * If the RHI supports textures that can be used as both shader resources and render targets,
 * and bForceSeparateTargetAndShaderResource=false, then a single texture is created.
 * Otherwise two textures are created, one of them usable as a shader resource and resolve target, and one of them usable as a render target.
 * Two texture references are always returned, but they may reference the same texture.
 * If two different textures are returned, the render-target texture must be manually copied to the shader-resource texture.
 */
inline void RHICreateTargetableShaderResourceCubeArray(
	uint32 LinearSize,
	uint32 ArraySize,
	uint8 Format,
	uint32 NumMips,
	uint32 Flags,
	uint32 TargetableTextureFlags,
	bool bForceSeparateTargetAndShaderResource,
	FRHIResourceCreateInfo& CreateInfo,
	FTextureCubeRHIRef& OutTargetableTexture,
	FTextureCubeRHIRef& OutShaderResourceTexture
	)
{
	// Ensure none of the usage flags are passed in.
	check(!(Flags & TexCreate_RenderTargetable));
	check(!(Flags & TexCreate_ResolveTargetable));
	check(!(Flags & TexCreate_ShaderResource));

	// Ensure that all of the flags provided for the targetable texture are not already passed in Flags.
	check(!(Flags & TargetableTextureFlags));

	// Ensure that the targetable texture is either render or depth-stencil targetable.
	check(TargetableTextureFlags & (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable));

	if(!bForceSeparateTargetAndShaderResource/* && GSupportsRenderDepthTargetableShaderResources*/)
	{
		// Create a single texture that has both TargetableTextureFlags and TexCreate_ShaderResource set.
		OutTargetableTexture = OutShaderResourceTexture = RHICreateTextureCubeArray(LinearSize, ArraySize, Format, NumMips, Flags | TargetableTextureFlags | TexCreate_ShaderResource, CreateInfo);
	}
	else
	{
		// Create a texture that has TargetableTextureFlags set, and a second texture that has TexCreate_ResolveTargetable and TexCreate_ShaderResource set.
		OutTargetableTexture = RHICreateTextureCubeArray(LinearSize, ArraySize, Format, NumMips, Flags | TargetableTextureFlags, CreateInfo);
		OutShaderResourceTexture = RHICreateTextureCubeArray(LinearSize, ArraySize, Format, NumMips, Flags | TexCreate_ResolveTargetable | TexCreate_ShaderResource, CreateInfo);
	}
}
/**
 * Computes the vertex count for a given number of primitives of the specified type.
 * @param NumPrimitives The number of primitives.
 * @param PrimitiveType The type of primitives.
 * @returns The number of vertices.
 */
inline uint32 GetVertexCountForPrimitiveCount(uint32 NumPrimitives, uint32 PrimitiveType)
{
	uint32 VertexCount = 0;
	switch(PrimitiveType)
	{
	case PT_TriangleList: VertexCount = NumPrimitives*3; break;
	case PT_TriangleStrip: VertexCount = NumPrimitives+2; break;
	case PT_LineList: VertexCount = NumPrimitives*2; break;
	case PT_PointList: VertexCount = NumPrimitives; break;
	case PT_1_ControlPointPatchList:
	case PT_2_ControlPointPatchList:
	case PT_3_ControlPointPatchList: 
	case PT_4_ControlPointPatchList: 
	case PT_5_ControlPointPatchList:
	case PT_6_ControlPointPatchList:
	case PT_7_ControlPointPatchList: 
	case PT_8_ControlPointPatchList: 
	case PT_9_ControlPointPatchList: 
	case PT_10_ControlPointPatchList: 
	case PT_11_ControlPointPatchList: 
	case PT_12_ControlPointPatchList: 
	case PT_13_ControlPointPatchList: 
	case PT_14_ControlPointPatchList: 
	case PT_15_ControlPointPatchList: 
	case PT_16_ControlPointPatchList: 
	case PT_17_ControlPointPatchList: 
	case PT_18_ControlPointPatchList: 
	case PT_19_ControlPointPatchList: 
	case PT_20_ControlPointPatchList: 
	case PT_21_ControlPointPatchList: 
	case PT_22_ControlPointPatchList: 
	case PT_23_ControlPointPatchList: 
	case PT_24_ControlPointPatchList: 
	case PT_25_ControlPointPatchList: 
	case PT_26_ControlPointPatchList: 
	case PT_27_ControlPointPatchList: 
	case PT_28_ControlPointPatchList: 
	case PT_29_ControlPointPatchList: 
	case PT_30_ControlPointPatchList: 
	case PT_31_ControlPointPatchList: 
	case PT_32_ControlPointPatchList: 
		VertexCount = (PrimitiveType - PT_1_ControlPointPatchList + 1) * NumPrimitives;
		break;
	default: UE_LOG(LogRHI, Fatal,TEXT("Unknown primitive type: %u"),PrimitiveType);
	};

	return VertexCount;
}

/**
 * Draw a primitive using the vertices passed in.
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param NumPrimitives The number of primitives in the VertexData buffer
 * @param VertexData A reference to memory preallocate in RHIBeginDrawPrimitiveUP
 * @param VertexDataStride Size of each vertex
 */
inline void DrawPrimitiveUP(FRHICommandList& RHICmdList, uint32 PrimitiveType, uint32 NumPrimitives, const void* VertexData, uint32 VertexDataStride)
{
	void* Buffer = NULL;
	const uint32 VertexCount = GetVertexCountForPrimitiveCount( NumPrimitives, PrimitiveType );
	RHICmdList.BeginDrawPrimitiveUP(PrimitiveType, NumPrimitives, VertexCount, VertexDataStride, Buffer);
	FMemory::Memcpy( Buffer, VertexData, VertexCount * VertexDataStride );
	RHICmdList.EndDrawPrimitiveUP();
}

/**
 * Draw a primitive using the vertices passed in as described the passed in indices. 
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param MinVertexIndex The lowest vertex index used by the index buffer
 * @param NumVertices The number of vertices in the vertex buffer
 * @param NumPrimitives THe number of primitives described by the index buffer
 * @param IndexData The memory preallocated in RHIBeginDrawIndexedPrimitiveUP
 * @param IndexDataStride The size of one index
 * @param VertexData The memory preallocate in RHIBeginDrawIndexedPrimitiveUP
 * @param VertexDataStride The size of one vertex
 */
inline void DrawIndexedPrimitiveUP(
	FRHICommandList& RHICmdList,
	uint32 PrimitiveType,
	uint32 MinVertexIndex,
	uint32 NumVertices,
	uint32 NumPrimitives,
	const void* IndexData,
	uint32 IndexDataStride,
	const void* VertexData,
	uint32 VertexDataStride )
{
	void* VertexBuffer = NULL;
	void* IndexBuffer = NULL;
	const uint32 NumIndices = GetVertexCountForPrimitiveCount( NumPrimitives, PrimitiveType );
	RHICmdList.BeginDrawIndexedPrimitiveUP(
		PrimitiveType,
		NumPrimitives,
		NumVertices,
		VertexDataStride,
		VertexBuffer,
		MinVertexIndex,
		NumIndices,
		IndexDataStride,
		IndexBuffer );
	FMemory::Memcpy( VertexBuffer, VertexData, NumVertices * VertexDataStride );
	FMemory::Memcpy( IndexBuffer, IndexData, NumIndices * IndexDataStride );
	RHICmdList.EndDrawIndexedPrimitiveUP();
}

inline uint32 ComputeAnisotropyRT(int32 InitializerMaxAnisotropy)
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MaxAnisotropy"));
	int32 CVarValue = CVar->GetValueOnRenderThread();

	return FMath::Clamp(InitializerMaxAnisotropy > 0 ? InitializerMaxAnisotropy : CVarValue, 1, 16);
}

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
#define ENABLE_TRANSITION_DUMP 0
#else
#define ENABLE_TRANSITION_DUMP 1
#endif

class RHI_API FDumpTransitionsHelper
{
public:
	static void DumpResourceTransition(const FName& ResourceName, const EResourceTransitionAccess TransitionType);
	
private:
	static void DumpTransitionForResourceHandler();

	static TAutoConsoleVariable<FString> CVarDumpTransitionsForResource;
	static FAutoConsoleVariableSink CVarDumpTransitionsForResourceSink;
	static FName DumpTransitionForResource;
};

#if ENABLE_TRANSITION_DUMP
#define DUMP_TRANSITION(ResourceName, TransitionType) FDumpTransitionsHelper::DumpResourceTransition(ResourceName, TransitionType);
#else
#define DUMP_TRANSITION(ResourceName, TransitionType)
#endif