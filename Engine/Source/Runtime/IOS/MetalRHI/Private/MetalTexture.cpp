// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalVertexBuffer.cpp: Metal texture RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"

uint8 FMetalSurface::NextKey = 1; // 0 is reserved for MTLPixelFormatInvalid

/** Texture reference class. */
class FMetalTextureReference : public FRHITextureReference
{
public:
	explicit FMetalTextureReference(FLastRenderTimeContainer* InLastRenderTime)
		: FRHITextureReference(InLastRenderTime)
	{}

	// IRefCountedObject interface.
	virtual uint32 AddRef() const
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const
	{
		return FRHIResource::GetRefCount();
	}

	void SetReferencedTexture(FRHITexture* InTexture)
	{
		FRHITextureReference::SetReferencedTexture(InTexture);
	}
};

/** Given a pointer to a RHI texture that was created by the Metal RHI, returns a pointer to the FMetalTextureBase it encapsulates. */
FMetalSurface* GetMetalSurfaceFromRHITexture(FRHITexture* Texture)
{
    if (!Texture)
    {
        return NULL;
    }
	if(Texture->GetTexture2D())
	{
		return &((FMetalTexture2D*)Texture)->Surface;
	}
	else if(Texture->GetTexture2DArray())
	{
		return &((FMetalTexture2DArray*)Texture)->Surface;
	}
	else if(Texture->GetTexture3D())
	{
		return &((FMetalTexture3D*)Texture)->Surface;
	}
	else if(Texture->GetTextureCube())
	{
		return &((FMetalTextureCube*)Texture)->Surface;
	}
	else if(Texture->GetTextureReference())
	{
		return GetMetalSurfaceFromRHITexture(static_cast<FMetalTextureReference*>(Texture)->GetReferencedTexture());
	}
	else
	{
		UE_LOG(LogMetal, Fatal, TEXT("Unknown RHI texture type"));
		return &((FMetalTexture2D*)Texture)->Surface;
	}
}

static bool IsRenderTarget(uint32 Flags)
{
	return (Flags & (TexCreate_RenderTargetable | TexCreate_ResolveTargetable | TexCreate_DepthStencilTargetable)) != 0;
}

FMetalSurface::FMetalSurface(ERHIResourceType ResourceType, EPixelFormat Format, uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 NumSamples, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 InFlags, FResourceBulkDataInterface* BulkData)
	: PixelFormat(Format)
    , MSAATexture(nil)
	, StencilTexture(nil)
	, SizeX(InSizeX)
	, SizeY(InSizeY)
	, SizeZ(InSizeZ)
	, bIsCubemap(false)
	, Flags(InFlags)
	, TotalTextureSize(0)
{
	// get a unique key for this surface's format
	static TMap<uint32, uint8> PixelFormatKeyMap;
	MTLPixelFormat MTLFormat = (MTLPixelFormat)GPixelFormats[Format].PlatformFormat;
	uint8* Key = PixelFormatKeyMap.Find(MTLFormat);
	if (Key == NULL)
	{
		Key = &PixelFormatKeyMap.Add(MTLFormat, NextKey++);

		// only giving 4 bits to the key
		checkf(NextKey < 16, TEXT("Too many unique pixel formats to fit into the PipelineStateHash"));
	}
	// set the key
	FormatKey = *Key;

	FMemory::Memzero(LockedMemory, sizeof(LockedMemory));


	// the special back buffer surface will be updated in FMetalManager::BeginFrame - no need to set the texture here
	if (Flags & TexCreate_Presentable)
	{
		return;
	}

	bool bIsRenderTarget = IsRenderTarget(Flags);
	MTLTextureDescriptor* Desc;
	
	if (ResourceType == RRT_TextureCube)
	{
		Desc = [MTLTextureDescriptor textureCubeDescriptorWithPixelFormat:MTLFormat
																	size:SizeX
															   mipmapped:(NumMips > 1)];
		bIsCubemap = true;
	}
	else if (ResourceType == RRT_Texture3D)
	{
		Desc = [MTLTextureDescriptor new];
		Desc.textureType = MTLTextureType3D;
		Desc.width = SizeX;
		Desc.height = SizeY;
		Desc.depth = SizeZ;
		Desc.pixelFormat = MTLFormat;
		Desc.arrayLength = 1;
		Desc.mipmapLevelCount = 1;
		Desc.sampleCount = 1;
	}
	else
	{
		Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLFormat
																 width:SizeX
																height:SizeY
															 mipmapped:(NumMips > 1)];
		Desc.depth = SizeZ;
	}

	// flesh out the descriptor
	if (bArray)
	{
		Desc.arrayLength = ArraySize;
	}
	Desc.mipmapLevelCount = NumMips;
	
	
	Texture = [FMetalManager::GetDevice() newTextureWithDescriptor:Desc];
	if (Texture == nil)
	{
		NSLog(@"Failed to create texture, desc  %@", Desc);
	}
	TRACK_OBJECT(Texture);
	
	// upload existing bulkdata
	if (BulkData)
	{
		UE_LOG(LogIOS, Display, TEXT("Got a bulk data texture, with %d mips"), NumMips);
		checkf(NumMips == 1&& ArraySize == 1, TEXT("Only handling bulk data with 1 mip and 1 array length"));
		uint32 Stride;

		// lock, copy, unlock
		void* LockedData = Lock(0, 0, RLM_WriteOnly, Stride);
		FMemory::Memcpy(LockedData, BulkData->GetResourceBulkData(), BulkData->GetResourceBulkDataSize());
		Unlock(0, 0);

		// bulk data can be unloaded now
		BulkData->Discard();
	}

	// calculate size of the texture
	TotalTextureSize = GetMemorySize();

	if (!FParse::Param(FCommandLine::Get(), TEXT("nomsaa")))
	{
		if (NumSamples > 1)
		{
			check(bIsRenderTarget);
			Desc.textureType = MTLTextureType2DMultisample;
	
			// allow commandline to override
			FParse::Value(FCommandLine::Get(), TEXT("msaa="), NumSamples);
			Desc.sampleCount = NumSamples;

			MSAATexture = [FMetalManager::GetDevice() newTextureWithDescriptor:Desc];
			if (Format == PF_DepthStencil)
			{
				[Texture release];
				Texture = MSAATexture;

				// we don't have the resolve texture, so we just update the memory size with the MSAA size
				TotalTextureSize = TotalTextureSize * NumSamples;
			}
			else
			{
				// an MSAA render target takes NumSamples more space, in addition to the resolve texture
				TotalTextureSize += TotalTextureSize * NumSamples;
			}
        
			NSLog(@"Creating %dx MSAA %d x %d %s surface", (int32)Desc.sampleCount, SizeX, SizeY, (Flags & TexCreate_RenderTargetable) ? "Color" : "Depth");
			if (MSAATexture == nil)
			{
				NSLog(@"Failed to create texture, desc  %@", Desc);
			}
			TRACK_OBJECT(MSAATexture);
		}
	}

	// create a stencil buffer if needed
	if (GMaxRHIShaderPlatform == SP_METAL_MRT && Format == PF_DepthStencil)
	{
		Desc.textureType = MTLTextureType2D;
		Desc.pixelFormat = MTLPixelFormatStencil8;
		StencilTexture = [FMetalManager::GetDevice() newTextureWithDescriptor:Desc];

		// 1 byte per texel
		TotalTextureSize += SizeX * SizeY;
	}

	// track memory usage
	if (bIsRenderTarget)
	{
		GCurrentRendertargetMemorySize += Align(TotalTextureSize, 1024) / 1024;
	}
	else
	{
		GCurrentTextureMemorySize += Align(TotalTextureSize, 1024) / 1024;
	}

#if STATS
	if (ResourceType == RRT_TextureCube)
	{
		if (bIsRenderTarget)
		{
			INC_MEMORY_STAT_BY(STAT_RenderTargetMemoryCube, TotalTextureSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_TextureMemoryCube, TotalTextureSize);
		}
	}
	else if (ResourceType == RRT_Texture3D)
	{
		if (bIsRenderTarget)
		{
			INC_MEMORY_STAT_BY(STAT_RenderTargetMemory3D, TotalTextureSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_TextureMemory3D, TotalTextureSize);
		}
	}
	else
	{
		if (bIsRenderTarget)
		{
			INC_MEMORY_STAT_BY(STAT_RenderTargetMemory2D, TotalTextureSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_TextureMemory2D, TotalTextureSize);
		}
	}
#endif
}


FMetalSurface::~FMetalSurface()
{
	// track memory usage
	if (IsRenderTarget(Flags))
	{
		GCurrentRendertargetMemorySize -= Align(TotalTextureSize, 1024) / 1024;
	}
	else
	{
		GCurrentTextureMemorySize -= Align(TotalTextureSize, 1024) / 1024;
	}

	if (MSAATexture != nil)
    {
		FMetalManager::ReleaseObject(MSAATexture);
    }
    
	if (!(Flags & TexCreate_Presentable))
	{
		FMetalManager::ReleaseObject(Texture);
	}
}


void* FMetalSurface::Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride)
{
	// get size and stride
	const uint32 MipBytes = GetMipSize(MipIndex, &DestStride, false);
	
	// allocate some temporary memory
	check(LockedMemory[MipIndex] == NULL);
	LockedMemory[MipIndex] = FMemory::Malloc(MipBytes);
	if (LockMode != RLM_WriteOnly)
	{
		// [Texture readPixels ...];
	}

	return LockedMemory[MipIndex];
}

void FMetalSurface::Unlock(uint32 MipIndex, uint32 ArrayIndex)
{
	uint32 Stride;
	uint32 BytesPerImage = GetMipSize(MipIndex, &Stride, true);
	if (PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4)
	{
		// compressed textures want zero here for whatever reason
		BytesPerImage = 0;
		Stride = 0;
	}

	if (SizeZ <= 1 || bIsCubemap)
	{
		// upload the texture to the texture slice
		MTLRegion Region = MTLRegionMake2D(0, 0, FMath::Max<uint32>(SizeX >> MipIndex, 1), FMath::Max<uint32>(SizeY >> MipIndex, 1));
		[Texture replaceRegion : Region mipmapLevel : MipIndex slice : ArrayIndex withBytes : LockedMemory[MipIndex] bytesPerRow:Stride bytesPerImage:0];
	}
	else
	{
		// upload the texture to the texture slice
		MTLRegion Region = MTLRegionMake3D(0, 0, 0, FMath::Max<uint32>(SizeX >> MipIndex, 1), FMath::Max<uint32>(SizeY >> MipIndex, 1), FMath::Max<uint32>(SizeZ >> MipIndex, 1));
		[Texture replaceRegion:Region mipmapLevel:MipIndex slice:ArrayIndex withBytes:LockedMemory[MipIndex] bytesPerRow:Stride bytesPerImage:BytesPerImage];
	}
	
	FMemory::Free(LockedMemory[MipIndex]);
	LockedMemory[MipIndex] = NULL;
}

uint32 FMetalSurface::GetMipSize(uint32 MipIndex, uint32* Stride, bool bSingleLayer)
{
	// Calculate the dimensions of the mip-map.
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	const uint32 MipSizeX = FMath::Max(SizeX >> MipIndex, BlockSizeX);
	const uint32 MipSizeY = FMath::Max(SizeY >> MipIndex, BlockSizeY);
	const uint32 MipSizeZ = bSingleLayer ? 1 : FMath::Max(SizeZ >> MipIndex, 1u);
	uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	uint32 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;
	if (PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4)
	{
		// PVRTC has minimum 2 blocks width and height
		NumBlocksX = FMath::Max<uint32>(NumBlocksX, 2);
		NumBlocksY = FMath::Max<uint32>(NumBlocksY, 2);
	}

	const uint32 MipBytes = NumBlocksX * NumBlocksY * BlockBytes * MipSizeZ;

	if (Stride)
	{
		*Stride = NumBlocksX * BlockBytes;
	}

	return MipBytes;
}

uint32 FMetalSurface::GetMemorySize()
{
	// if already calculated, no need to do it again
	if (TotalTextureSize != 0)
	{
		return TotalTextureSize;
	}

	if (Texture == nil)
	{
		return 0;
	}

	uint32 TotalSize = 0;
	for (uint32 MipIndex = 0; MipIndex < [Texture mipmapLevelCount]; MipIndex++)
	{
		TotalSize += GetMipSize(MipIndex, NULL, false);
	}

	return TotalSize;
}

/*-----------------------------------------------------------------------------
	Texture allocator support.
-----------------------------------------------------------------------------*/

void FMetalDynamicRHI::RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats)
{
	OutStats.DedicatedVideoMemory = 0;
	OutStats.DedicatedSystemMemory = 0;
	OutStats.SharedSystemMemory = 0;
	OutStats.TotalGraphicsMemory = 0;

	OutStats.AllocatedMemorySize = int64(GCurrentTextureMemorySize) * 1024;
	OutStats.TexturePoolSize = GTexturePoolSize;
	OutStats.PendingMemoryAdjustment = 0;
}

bool FMetalDynamicRHI::RHIGetTextureMemoryVisualizeData( FColor* /*TextureData*/, int32 /*SizeX*/, int32 /*SizeY*/, int32 /*Pitch*/, int32 /*PixelSize*/ )
{
	return false;
}

uint32 FMetalDynamicRHI::RHIComputeMemorySize(FTextureRHIParamRef TextureRHI)
{
	if(!TextureRHI)
	{
		return 0;
	}

	return GetMetalSurfaceFromRHITexture(TextureRHI)->GetMemorySize();
}

/*-----------------------------------------------------------------------------
	2D texture support.
-----------------------------------------------------------------------------*/

FTexture2DRHIRef FMetalDynamicRHI::RHICreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FMetalTexture2D((EPixelFormat)Format, SizeX, SizeY, NumMips, NumSamples, Flags, CreateInfo.BulkData);
}

FTexture2DRHIRef FMetalDynamicRHI::RHIAsyncCreateTexture2D(uint32 SizeX,uint32 SizeY,uint8 Format,uint32 NumMips,uint32 Flags,void** InitialMipData,uint32 NumInitialMips)
{
	UE_LOG(LogMetal, Fatal, TEXT("RHIAsyncCreateTexture2D is not supported"));
	return FTexture2DRHIRef();
}

void FMetalDynamicRHI::RHICopySharedMips(FTexture2DRHIParamRef DestTexture2D,FTexture2DRHIParamRef SrcTexture2D)
{

}

FTexture2DArrayRHIRef FMetalDynamicRHI::RHICreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FMetalTexture2DArray((EPixelFormat)Format, SizeX, SizeY, SizeZ, NumMips, Flags, CreateInfo.BulkData);
}

FTexture3DRHIRef FMetalDynamicRHI::RHICreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FMetalTexture3D((EPixelFormat)Format, SizeX, SizeY, SizeZ, NumMips, Flags, CreateInfo.BulkData);
}

void FMetalDynamicRHI::RHIGetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo)
{
}

void FMetalDynamicRHI::RHIGenerateMips(FTextureRHIParamRef SourceSurfaceRHI)
{

}

FTexture2DRHIRef FMetalDynamicRHI::RHIAsyncReallocateTexture2D(FTexture2DRHIParamRef OldTextureRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	FMetalTexture2D* OldTexture = ResourceCast(OldTextureRHI);

	FMetalTexture2D* NewTexture = new FMetalTexture2D(OldTexture->GetFormat(), NewSizeX, NewSizeY, NewMipCount, OldTexture->GetNumSamples(), OldTexture->GetFlags(), NULL);

	// @todo: gather these all up over a frame
	id<MTLCommandBuffer> CommandBuffer = FMetalManager::Get()->CreateTempCommandBuffer(false/*bRetainReferences*/);

	// create a blitter object
	id<MTLBlitCommandEncoder> Blitter = [CommandBuffer blitCommandEncoder];


	// figure out what mips to schedule
	const uint32 NumSharedMips = FMath::Min(OldTexture->GetNumMips(), NewTexture->GetNumMips());
	const uint32 SourceMipOffset = OldTexture->GetNumMips() - NumSharedMips;
	const uint32 DestMipOffset = NewTexture->GetNumMips() - NumSharedMips;

	// only handling straight 2D textures here
	uint32 SliceIndex = 0;
	MTLOrigin Origin = MTLOriginMake(0,0,0);

	for (uint32 MipIndex = 0; MipIndex < NumSharedMips; ++MipIndex)
	{
		const uint32 MipSizeX = FMath::Max<uint32>(1, NewSizeX >> (MipIndex + DestMipOffset));
		const uint32 MipSizeY = FMath::Max<uint32>(1, NewSizeY >> (MipIndex + DestMipOffset));

		// set up the copy
		[Blitter copyFromTexture:OldTexture->Surface.Texture 
					 sourceSlice:SliceIndex
					 sourceLevel:MipIndex + SourceMipOffset
					sourceOrigin:Origin
					  sourceSize:MTLSizeMake(MipSizeX, MipSizeY, 1)
					   toTexture:NewTexture->Surface.Texture
			    destinationSlice:SliceIndex
			    destinationLevel:MipIndex + DestMipOffset
			   destinationOrigin:Origin];
	}

	// when done, decrement the counter to indicate it's safe
	[CommandBuffer addCompletedHandler:^(id <MTLCommandBuffer> Buffer)
	{
		RequestStatus->Decrement();
	}];

	// kick it off!
	[Blitter endEncoding];
	[CommandBuffer commit];

	return NewTexture;
}

ETextureReallocationStatus FMetalDynamicRHI::RHIFinalizeAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Succeeded;
}

ETextureReallocationStatus FMetalDynamicRHI::RHICancelAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Failed;
}


void* FMetalDynamicRHI::RHILockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FMetalTexture2D* Texture = ResourceCast(TextureRHI);
	return Texture->Surface.Lock(MipIndex, 0, LockMode, DestStride);
}

void FMetalDynamicRHI::RHIUnlockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,bool bLockWithinMiptail)
{
	FMetalTexture2D* Texture = ResourceCast(TextureRHI);
	Texture->Surface.Unlock(MipIndex, 0);
}

void* FMetalDynamicRHI::RHILockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FMetalTexture2DArray* Texture = ResourceCast(TextureRHI);
	return Texture->Surface.Lock(MipIndex, TextureIndex, LockMode, DestStride);
}

void FMetalDynamicRHI::RHIUnlockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	FMetalTexture2DArray* Texture = ResourceCast(TextureRHI);
	Texture->Surface.Unlock(MipIndex, TextureIndex);
}

void FMetalDynamicRHI::RHIUpdateTexture2D(FTexture2DRHIParamRef TextureRHI, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{	
	FMetalTexture2D* Texture = ResourceCast(TextureRHI);

}

void FMetalDynamicRHI::RHIUpdateTexture3D(FTexture3DRHIParamRef TextureRHI,uint32 MipIndex,const FUpdateTextureRegion3D& UpdateRegion,uint32 SourceRowPitch,uint32 SourceDepthPitch,const uint8* SourceData)
{	
	FMetalTexture3D* Texture = ResourceCast(TextureRHI);

}


/*-----------------------------------------------------------------------------
	Cubemap texture support.
-----------------------------------------------------------------------------*/
FTextureCubeRHIRef FMetalDynamicRHI::RHICreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FMetalTextureCube((EPixelFormat)Format, Size, false, 1, NumMips, Flags, CreateInfo.BulkData);
}

FTextureCubeRHIRef FMetalDynamicRHI::RHICreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FMetalTextureCube((EPixelFormat)Format, Size, true, ArraySize, NumMips, Flags, CreateInfo.BulkData);
}

void* FMetalDynamicRHI::RHILockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FMetalTextureCube* TextureCube = ResourceCast(TextureCubeRHI);
	uint32 MetalFace = GetMetalCubeFace((ECubeFace)FaceIndex);
	return TextureCube->Surface.Lock(MipIndex, FaceIndex + 6 * ArrayIndex, LockMode, DestStride);
}

void FMetalDynamicRHI::RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	FMetalTextureCube* TextureCube = ResourceCast(TextureCubeRHI);
	uint32 MetalFace = GetMetalCubeFace((ECubeFace)FaceIndex);
	TextureCube->Surface.Unlock(MipIndex, FaceIndex + ArrayIndex * 6);
}




FTextureReferenceRHIRef FMetalDynamicRHI::RHICreateTextureReference(FLastRenderTimeContainer* InLastRenderTime)
{
	return new FMetalTextureReference(InLastRenderTime);
}

void FMetalDynamicRHI::RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRefRHI, FTextureRHIParamRef NewTextureRHI)
{
	FMetalTextureReference* TextureRef = (FMetalTextureReference*)TextureRefRHI;
	if (TextureRef)
	{
		TextureRef->SetReferencedTexture(NewTextureRHI);
	}
}


void FMetalDynamicRHI::RHIBindDebugLabelName(FTextureRHIParamRef TextureRHI, const TCHAR* Name)
{
}

void FMetalDynamicRHI::RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
	NOT_SUPPORTED("RHIVirtualTextureSetFirstMipInMemory");
}

void FMetalDynamicRHI::RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
	NOT_SUPPORTED("RHIVirtualTextureSetFirstMipVisible");
}
