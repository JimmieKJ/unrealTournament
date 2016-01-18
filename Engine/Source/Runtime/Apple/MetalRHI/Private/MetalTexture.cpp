// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalVertexBuffer.cpp: Metal texture RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h" // for STAT_MetalTexturePageOffTime

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

#if METAL_API_1_1
static MTLTextureUsage ConvertFlagsToUsage(uint32 Flags)
{
	MTLTextureUsage Usage = MTLTextureUsageUnknown;
#if PLATFORM_MAC
	if (Flags & (TexCreate_RenderTargetable|TexCreate_DepthStencilTargetable))
	{
		Usage |= MTLTextureUsageRenderTarget;
		Usage |= MTLTextureUsageShaderRead;
		Usage |= MTLTextureUsagePixelFormatView;
	}
	
	if(Flags & (TexCreate_ShaderResource|TexCreate_ResolveTargetable))
	{
		Usage |= MTLTextureUsageShaderRead;
		Usage |= MTLTextureUsagePixelFormatView;
	}
	
	if (Flags & TexCreate_UAV)
	{
		Usage |= MTLTextureUsageShaderRead;
		Usage |= MTLTextureUsageShaderWrite;
		Usage |= MTLTextureUsagePixelFormatView;
	}
#endif
	return Usage;
}
#endif

static bool IsPixelFormatCompressed(EPixelFormat Format)
{
	switch (Format)
	{
		case PF_DXT1:
		case PF_DXT3:
		case PF_DXT5:
		case PF_PVRTC2:
		case PF_PVRTC4:
		case PF_BC4:
		case PF_BC5:
		case PF_ATC_RGB:
		case PF_ATC_RGBA_E:
		case PF_ATC_RGBA_I:
		case PF_ETC1:
		case PF_ETC2_RGB:
		case PF_ETC2_RGBA:
		case PF_ASTC_4x4:
		case PF_ASTC_6x6:
		case PF_ASTC_8x8:
		case PF_ASTC_10x10:
		case PF_ASTC_12x12:
		case PF_BC6H:
		case PF_BC7:
			return true;
		default:
			return false;
	}
}

FMetalSurface::FMetalSurface(FMetalSurface const& Source, NSRange MipRange)
: Type(Source.Type)
, PixelFormat(Source.PixelFormat)
, Texture(nil)
, MSAATexture(nil)
, StencilTexture(nil)
, SizeX(Source.SizeX)
, SizeY(Source.SizeY)
, SizeZ(Source.SizeZ)
, bIsCubemap(Source.bIsCubemap)
, Flags(Source.Flags)
, WriteLock(0)
, TotalTextureSize(0)
, IB(nullptr)
{
	MTLPixelFormat MetalFormat = (MTLPixelFormat)GPixelFormats[PixelFormat].PlatformFormat;

    NSRange Slices = NSMakeRange(0, FMath::Min(SizeZ, 1u));
    // @todo Zebra Temporary workaround for absence of X24_G8 or equivalent to GL_STENCIL_INDEX so that the stencil part of a texture may be sampled
	// For now, if we find ourselves *requiring* this we'd need to lazily blit the stencil data out to a separate texture. radr://21813831
#if METAL_API_1_1
    if(Source.PixelFormat != PF_DepthStencil)
    {
        Texture = [Source.Texture newTextureViewWithPixelFormat:MetalFormat textureType:Source.Texture.textureType levels:MipRange slices:Slices];
    }
    else
#endif
    {
        Texture = [Source.Texture retain];
    }
	if(Source.MSAATexture)
	{
		MSAATexture = Texture;
	}
	if(Source.StencilTexture)
	{
#if PLATFORM_IOS
		check(false); // @todo Zebra Must handle separate stencil texture SRV!
#endif
		StencilTexture = Texture;
	}
	
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	SizeX = FMath::Max(SizeX >> MipRange.location, BlockSizeX);
	SizeY = FMath::Max(SizeY >> MipRange.location, BlockSizeY);
	SizeZ = (Type != RRT_Texture3D) ? SizeZ : FMath::Max(SizeZ >> MipRange.location, 1u);
	
	FMemory::Memzero(LockedMemory, sizeof(LockedMemory));
}

FMetalSurface::FMetalSurface(FMetalSurface const& Source, NSRange const MipRange, EPixelFormat Format)
: Type(Source.Type)
, PixelFormat(Format)
, Texture(nil)
, MSAATexture(nil)
, StencilTexture(nil)
, SizeX(Source.SizeX)
, SizeY(Source.SizeY)
, SizeZ(Source.SizeZ)
, bIsCubemap(Source.bIsCubemap)
, Flags(Source.Flags)
, WriteLock(0)
, TotalTextureSize(0)
, IB(nullptr)
{
	check(!Source.MSAATexture);
	
	MTLPixelFormat MetalFormat = (MTLPixelFormat)GPixelFormats[PixelFormat].PlatformFormat;
	
	NSRange Slices = NSMakeRange(0, FMath::Min(SizeZ, 1u));
    // @todo Zebra Temporary workaround for absence of X24_G8 or equivalent to GL_STENCIL_INDEX so that the stencil part of a texture may be sampled
	// For now, if we find ourselves *requiring* this we'd need to lazily blit the stencil data out to a separate texture. radr://21813831
#if METAL_API_1_1
    if(Source.PixelFormat != PF_DepthStencil)
    {
        Texture = [Source.Texture newTextureViewWithPixelFormat:MetalFormat textureType:Source.Texture.textureType levels:MipRange slices:Slices];
    }
    else
#endif
    {
        Texture = [Source.Texture retain];
    }
	if(Source.MSAATexture)
	{
		MSAATexture = Texture;
	}
	if(Source.StencilTexture)
	{
#if PLATFORM_IOS
		check(false); // @todo Zebra Must handle separate stencil texture SRV!
#endif
		StencilTexture = Texture;
	}
	
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	SizeX = FMath::Max(SizeX >> MipRange.location, BlockSizeX);
	SizeY = FMath::Max(SizeY >> MipRange.location, BlockSizeY);
	SizeZ = (Type != RRT_Texture3D) ? SizeZ : FMath::Max(SizeZ >> MipRange.location, 1u);
	
	FMemory::Memzero(LockedMemory, sizeof(LockedMemory));
}


FMetalSurface::FMetalSurface(ERHIResourceType ResourceType, EPixelFormat Format, uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, uint32 NumSamples, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 InFlags, FResourceBulkDataInterface* BulkData)
	: Type(ResourceType)
	, PixelFormat(Format)
	, Texture(nil)
    , MSAATexture(nil)
	, StencilTexture(nil)
	, SizeX(InSizeX)
	, SizeY(InSizeY)
	, SizeZ(InSizeZ)
	, bIsCubemap(false)
	, Flags(InFlags)
    , WriteLock(0)
	, TotalTextureSize(0)
	, IB(nullptr)
{
	// get a unique key for this surface's format
	static TMap<uint32, uint8> PixelFormatKeyMap;
	if (PixelFormatKeyMap.Num() == 0)
	{
		// Add depth stencil formats fist, so we don't have to use 5 bits for them in the pipeline hash
		PixelFormatKeyMap.Add(MTLPixelFormatDepth32Float, NextKey++);
		PixelFormatKeyMap.Add(MTLPixelFormatStencil8, NextKey++);
#if METAL_API_1_1
		PixelFormatKeyMap.Add(MTLPixelFormatDepth32Float_Stencil8, NextKey++);
#if PLATFORM_MAC
		PixelFormatKeyMap.Add(MTLPixelFormatDepth24Unorm_Stencil8, NextKey++);
#endif
#endif
	}
	
	// Dirty, filty hacks to pass through via an IOSurfaceRef which is required if in-game movie playback is to render at more than 5fps
#if PLATFORM_MAC
	bool const bIOSurfaceData = (BulkData && BulkData->GetResourceBulkDataSize() == ~0u);
	if(bIOSurfaceData)
	{
		// This won't ever be SRGB as IOSurface doesn't support it...
		Flags &= ~TexCreate_SRGB;
	}
#endif

	MTLPixelFormat MTLFormat = (MTLPixelFormat)GPixelFormats[Format].PlatformFormat;
#if PLATFORM_MAC
	if (Flags & TexCreate_SRGB)
	{
		switch (MTLFormat)
		{
			case MTLPixelFormatBC1_RGBA:
				MTLFormat = MTLPixelFormatBC1_RGBA_sRGB;
				break;
			case MTLPixelFormatBC2_RGBA:
				MTLFormat = MTLPixelFormatBC2_RGBA_sRGB;
				break;
			case MTLPixelFormatBC3_RGBA:
				MTLFormat = MTLPixelFormatBC3_RGBA_sRGB;
				break;
			case MTLPixelFormatRGBA8Unorm:
				MTLFormat = MTLPixelFormatRGBA8Unorm_sRGB;
				break;
			case MTLPixelFormatBGRA8Unorm:
				MTLFormat = MTLPixelFormatBGRA8Unorm_sRGB;
				break;
			default:
				break;
		}
	}
#endif
	uint8* Key = PixelFormatKeyMap.Find(MTLFormat);
	if (Key == NULL)
	{
		Key = &PixelFormatKeyMap.Add(MTLFormat, NextKey++);

#if PLATFORM_MAC
		// only giving 5 bits to the key
		checkf(NextKey < 32, TEXT("Too many unique pixel formats to fit into the PipelineStateHash"));
#else
		// only giving 4 bits to the key
		checkf(NextKey < 16, TEXT("Too many unique pixel formats to fit into the PipelineStateHash"));
#endif
	}
	// set the key
	FormatKey = *Key;

	FMemory::Memzero(LockedMemory, sizeof(LockedMemory));


	// the special back buffer surface will be updated in GetMetalDeviceContext().BeginDrawingViewport - no need to set the texture here
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
		if (bIsCubemap)
		{
#if PLATFORM_MAC
			Desc.textureType = MTLTextureTypeCubeArray;
#else
			check(0);
#endif
		}
	}
	Desc.mipmapLevelCount = NumMips;
	
#if METAL_API_1_1 // @todo
	if(GetMetalDeviceContext().SupportsFeature(EMetalFeaturesResourceOptions))
	{
		Desc.usage = ConvertFlagsToUsage(Flags);
		
		if((Flags & TexCreate_CPUReadback) && !(Flags & (TexCreate_RenderTargetable|TexCreate_DepthStencilTargetable|TexCreate_FastVRAM)))
		{
			Desc.cpuCacheMode = MTLCPUCacheModeDefaultCache;
	#if PLATFORM_MAC
			Desc.storageMode = MTLStorageModeManaged;
			Desc.resourceOptions = MTLResourceCPUCacheModeDefaultCache|MTLResourceStorageModeManaged;
	#else
			Desc.storageMode = MTLStorageModeShared;
			Desc.resourceOptions = MTLResourceCPUCacheModeDefaultCache|MTLResourceStorageModeShared;
	#endif
		}
		else if(((Flags & (TexCreate_NoTiling)) && !(Flags & (TexCreate_FastVRAM|TexCreate_DepthStencilTargetable|TexCreate_RenderTargetable))))
		{
	#if PLATFORM_MAC
			Desc.cpuCacheMode = MTLCPUCacheModeWriteCombined;
			Desc.storageMode = MTLStorageModeManaged;
			Desc.resourceOptions = MTLResourceCPUCacheModeWriteCombined|MTLResourceStorageModeManaged;
	#else
			Desc.cpuCacheMode = MTLCPUCacheModeDefaultCache;
			Desc.storageMode = MTLStorageModeShared;
			Desc.resourceOptions = MTLResourceCPUCacheModeDefaultCache|MTLResourceStorageModeShared;
	#endif
		}
		else
		{
			check(!(Flags & TexCreate_CPUReadback));
	#if PLATFORM_MAC
			Desc.cpuCacheMode = MTLCPUCacheModeWriteCombined;
			Desc.storageMode = MTLStorageModePrivate;
			Desc.resourceOptions = MTLResourceCPUCacheModeWriteCombined|MTLResourceStorageModePrivate;
	#else
			Desc.cpuCacheMode = MTLCPUCacheModeDefaultCache;
			Desc.storageMode = MTLStorageModeShared;
			Desc.resourceOptions = MTLResourceCPUCacheModeDefaultCache|MTLResourceStorageModeShared;
	#endif
		}
	}
#endif

#if PLATFORM_MAC
	// Dirty, filty hacks to pass through via an IOSurfaceRef
	if(bIOSurfaceData)
	{
		checkf(NumMips == 1&& ArraySize == 1, TEXT("Only handling bulk data with 1 mip and 1 array length"));
		IB = (CVImageBufferRef)BulkData->GetResourceBulkData();
		CVPixelBufferRetain(IB);
		{
			Texture = [GetMetalDeviceContext().GetDevice() newTextureWithDescriptor:Desc iosurface:CVPixelBufferGetIOSurface(IB) plane:0];
			
			if (Texture == nil)
			{
				NSLog(@"Failed to create texture, desc  %@", Desc);
			}
		}
		TRACK_OBJECT(Texture);
		
		BulkData->Discard();
	}
	else
#endif
	{
		Texture = [GetMetalDeviceContext().GetDevice() newTextureWithDescriptor:Desc];
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

			MSAATexture = [GetMetalDeviceContext().GetDevice() newTextureWithDescriptor:Desc];
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
	if (Format == PF_DepthStencil)
	{
#if PLATFORM_IOS
		Desc.textureType = MTLTextureType2D;
		Desc.pixelFormat = MTLPixelFormatStencil8;
		StencilTexture = [GetMetalDeviceContext().GetDevice() newTextureWithDescriptor:Desc];

		// 1 byte per texel
		TotalTextureSize += SizeX * SizeY;
#else
        StencilTexture = Texture;
        
        // 1 byte per texel
        TotalTextureSize += SizeX * SizeY;
#endif
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
		SafeReleaseMetalResource(MSAATexture);
    }
    
	if (!(Flags & TexCreate_Presentable) && MSAATexture != Texture)
	{
		SafeReleaseMetalResource(Texture);
	}
	
	if(IB)
	{
		CVPixelBufferRelease(IB);
	}

	IB = nullptr;
	MSAATexture = nil;
	Texture = nil;
	StencilTexture = nil;
	for(uint32 i = 0; i < 16; i++)
	{
		if(LockedMemory[i])
		{
			SafeReleaseMetalResource(LockedMemory[i]);
			LockedMemory[i] = nullptr;
		}
	}
}


void* FMetalSurface::Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride)
{
	// get size and stride
	const uint32 MipBytes = GetMipSize(MipIndex, &DestStride, false);
	
	// allocate some temporary memory
	if(!LockedMemory[MipIndex])
	{
#if METAL_API_1_1
		NSUInteger ResMode = MTLResourceStorageModeShared | (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesResourceOptions) ? MTLResourceCPUCacheModeWriteCombined : 0);
#else
		NSUInteger ResMode = MTLStorageModeShared;
#endif
		LockedMemory[MipIndex] = [GetMetalDeviceContext().GetDevice() newBufferWithLength:MipBytes options:ResMode];
	}
	
    switch(LockMode)
    {
        case RLM_ReadOnly:
        {
			SCOPE_CYCLE_COUNTER(STAT_MetalTexturePageOffTime);
			const uint32 MipSizeX = SizeX >> MipIndex;
			const uint32 MipSizeY = SizeY >> MipIndex;
			const uint32 MipSizeZ = FMath::Max(SizeZ >> MipIndex, 1u);

			MTLRegion Region;
			if (SizeZ <= 1 || bIsCubemap)
			{
				// upload the texture to the texture slice
				Region = MTLRegionMake2D(0, 0, FMath::Max<uint32>(SizeX >> MipIndex, 1), FMath::Max<uint32>(SizeY >> MipIndex, 1));
			}
			else
			{
				// upload the texture to the texture slice
				Region = MTLRegionMake3D(0, 0, 0, FMath::Max<uint32>(SizeX >> MipIndex, 1), FMath::Max<uint32>(SizeY >> MipIndex, 1), FMath::Max<uint32>(SizeZ >> MipIndex, 1));
			}

			id<MTLBlitCommandEncoder> Blitter = GetMetalDeviceContext().GetBlitContext();
			[Blitter copyFromTexture:Texture sourceSlice:ArrayIndex sourceLevel:MipIndex sourceOrigin:Region.origin sourceSize:Region.size toBuffer:LockedMemory[MipIndex] destinationOffset:0 destinationBytesPerRow:DestStride destinationBytesPerImage:MipBytes];
			
			//kick the current command buffer.
			GetMetalDeviceContext().SubmitCommandBufferAndWait();
			
            break;
        }
        case RLM_WriteOnly:
        {
            WriteLock |= 1 << MipIndex;
            break;
        }
        default:
            check(false);
            break;
    }

	return [LockedMemory[MipIndex] contents];
}

void FMetalSurface::Unlock(uint32 MipIndex, uint32 ArrayIndex)
{
    if(WriteLock & (1 << MipIndex))
    {
        WriteLock &= ~(1 << MipIndex);
        uint32 Stride;
        uint32 BytesPerImage = GetMipSize(MipIndex, &Stride, true);

		MTLRegion Region;
        if (SizeZ <= 1 || bIsCubemap)
		{
            // upload the texture to the texture slice
            Region = MTLRegionMake2D(0, 0, FMath::Max<uint32>(SizeX >> MipIndex, 1), FMath::Max<uint32>(SizeY >> MipIndex, 1));
        }
        else
        {
            // upload the texture to the texture slice
            Region = MTLRegionMake3D(0, 0, 0, FMath::Max<uint32>(SizeX >> MipIndex, 1), FMath::Max<uint32>(SizeY >> MipIndex, 1), FMath::Max<uint32>(SizeZ >> MipIndex, 1));
        }
#if PLATFORM_MAC
		if(Texture.storageMode == MTLStorageModePrivate)
		{
			id<MTLBlitCommandEncoder> Blitter = GetMetalDeviceContext().GetBlitContext();
			
			[Blitter copyFromBuffer:LockedMemory[MipIndex] sourceOffset:0 sourceBytesPerRow:Stride sourceBytesPerImage:BytesPerImage sourceSize:Region.size toTexture:Texture destinationSlice:ArrayIndex destinationLevel:MipIndex destinationOrigin:Region.origin];
		}
		else
#else
		if (Texture.pixelFormat >= MTLPixelFormatPVRTC_RGB_2BPP && Texture.pixelFormat <= MTLPixelFormatETC2_RGB8A1_sRGB) // @todo zebra
		{
			Stride = 0;
			BytesPerImage = 0;
		}
#endif
		{
			[Texture replaceRegion:Region mipmapLevel:MipIndex slice:ArrayIndex withBytes:[LockedMemory[MipIndex] contents] bytesPerRow:Stride bytesPerImage:BytesPerImage];
		}
		
		GetMetalDeviceContext().ReleaseObject(LockedMemory[MipIndex]);
		LockedMemory[MipIndex] = nullptr;
    }
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

uint32 FMetalSurface::GetNumFaces()
{
	switch (Type)
	{
		case RRT_Texture2DArray:
		case RRT_Texture3D:
		case RRT_TextureCube:
			return SizeZ * Texture.arrayLength;
			
		case RRT_Texture:
		case RRT_Texture2D:
		default:
			return 1;
	}
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
	NOT_SUPPORTED("RHIGetTextureMemoryVisualizeData");
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
	return new FMetalTexture2D((EPixelFormat)Format, SizeX, SizeY, NumMips, NumSamples, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

FTexture2DRHIRef FMetalDynamicRHI::RHIAsyncCreateTexture2D(uint32 SizeX,uint32 SizeY,uint8 Format,uint32 NumMips,uint32 Flags,void** InitialMipData,uint32 NumInitialMips)
{
	UE_LOG(LogMetal, Fatal, TEXT("RHIAsyncCreateTexture2D is not supported"));
	return FTexture2DRHIRef();
}

void FMetalDynamicRHI::RHICopySharedMips(FTexture2DRHIParamRef DestTexture2D,FTexture2DRHIParamRef SrcTexture2D)
{
	NOT_SUPPORTED("RHICopySharedMips");
}

FTexture2DArrayRHIRef FMetalDynamicRHI::RHICreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FMetalTexture2DArray((EPixelFormat)Format, SizeX, SizeY, SizeZ, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

FTexture3DRHIRef FMetalDynamicRHI::RHICreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FMetalTexture3D((EPixelFormat)Format, SizeX, SizeY, SizeZ, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

void FMetalDynamicRHI::RHIGetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo)
{
    // @todo Needed for visualisation!!
	// NOT_SUPPORTED("RHIGetResourceInfo");
}

void FMetalDynamicRHI::RHIGenerateMips(FTextureRHIParamRef SourceSurfaceRHI)
{
	
	NOT_SUPPORTED("RHIGenerateMips");
}

FTexture2DRHIRef FMetalDynamicRHI::RHIAsyncReallocateTexture2D(FTexture2DRHIParamRef OldTextureRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	FMetalTexture2D* OldTexture = ResourceCast(OldTextureRHI);

	FMetalTexture2D* NewTexture = new FMetalTexture2D(OldTexture->GetFormat(), NewSizeX, NewSizeY, NewMipCount, OldTexture->GetNumSamples(), OldTexture->GetFlags(), NULL, OldTextureRHI->GetClearBinding());

	// @todo: gather these all up over a frame
	id<MTLCommandBuffer> CommandBuffer = GetMetalDeviceContext().CreateCommandBuffer(false/*bRetainReferences*/);

	// create a blitter object
	id<MTLBlitCommandEncoder> Blitter = [CommandBuffer blitCommandEncoder];


	// figure out what mips to schedule
	const uint32 NumSharedMips = FMath::Min(OldTexture->GetNumMips(), NewTexture->GetNumMips());
	const uint32 SourceMipOffset = OldTexture->GetNumMips() - NumSharedMips;
	const uint32 DestMipOffset = NewTexture->GetNumMips() - NumSharedMips;

	// only handling straight 2D textures here
	uint32 SliceIndex = 0;
	MTLOrigin Origin = MTLOriginMake(0,0,0);
	
	id<MTLTexture> Tex = OldTexture->Surface.Texture;
	[Tex retain];

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
		[Tex release];
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
	
	id<MTLTexture> Tex = Texture->Surface.Texture;
	
	MTLRegion Region = MTLRegionMake2D(UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.Width, UpdateRegion.Height);
	
	[Tex replaceRegion:Region mipmapLevel:MipIndex slice:0 withBytes:SourceData bytesPerRow:SourcePitch bytesPerImage:0];
}

void FMetalDynamicRHI::RHIUpdateTexture3D(FTexture3DRHIParamRef TextureRHI,uint32 MipIndex,const FUpdateTextureRegion3D& UpdateRegion,uint32 SourceRowPitch,uint32 SourceDepthPitch,const uint8* SourceData)
{	
	FMetalTexture3D* Texture = ResourceCast(TextureRHI);

	id<MTLTexture> Tex = Texture->Surface.Texture;

	MTLRegion Region = MTLRegionMake3D(UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.DestZ, UpdateRegion.Width, UpdateRegion.Height, UpdateRegion.Depth);
	
	[Tex replaceRegion:Region mipmapLevel:MipIndex slice:0 withBytes:SourceData bytesPerRow:SourceRowPitch bytesPerImage:SourceDepthPitch];
}


/*-----------------------------------------------------------------------------
	Cubemap texture support.
-----------------------------------------------------------------------------*/
FTextureCubeRHIRef FMetalDynamicRHI::RHICreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FMetalTextureCube((EPixelFormat)Format, Size, false, 1, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

FTextureCubeRHIRef FMetalDynamicRHI::RHICreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return new FMetalTextureCube((EPixelFormat)Format, Size, true, ArraySize, NumMips, Flags, CreateInfo.BulkData, CreateInfo.ClearValueBinding);
}

void* FMetalDynamicRHI::RHILockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FMetalTextureCube* TextureCube = ResourceCast(TextureCubeRHI);
	uint32 MetalFace = GetMetalCubeFace((ECubeFace)FaceIndex);
	return TextureCube->Surface.Lock(MipIndex, MetalFace + (6 * ArrayIndex), LockMode, DestStride);
}

void FMetalDynamicRHI::RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	FMetalTextureCube* TextureCube = ResourceCast(TextureCubeRHI);
	uint32 MetalFace = GetMetalCubeFace((ECubeFace)FaceIndex);
	TextureCube->Surface.Unlock(MipIndex, MetalFace + (ArrayIndex * 6));
}




FTextureReferenceRHIRef FMetalDynamicRHI::RHICreateTextureReference(FLastRenderTimeContainer* InLastRenderTime)
{
	return new FMetalTextureReference(InLastRenderTime);
}

void FMetalRHICommandContext::RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRefRHI, FTextureRHIParamRef NewTextureRHI)
{
	FMetalTextureReference* TextureRef = (FMetalTextureReference*)TextureRefRHI;
	if (TextureRef)
	{
		TextureRef->SetReferencedTexture(NewTextureRHI);
	}
}


void FMetalDynamicRHI::RHIBindDebugLabelName(FTextureRHIParamRef TextureRHI, const TCHAR* Name)
{
	FMetalSurface* Surf = GetMetalSurfaceFromRHITexture(TextureRHI);
	if(Surf->Texture)
	{
		Surf->Texture.label = FString(Name).GetNSString();
	}
	if(Surf->MSAATexture)
	{
		Surf->MSAATexture.label = FString(Name).GetNSString();
	}
	if(Surf->StencilTexture)
	{
		Surf->StencilTexture.label = FString(Name).GetNSString();
	}
}

void FMetalDynamicRHI::RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
	NOT_SUPPORTED("RHIVirtualTextureSetFirstMipInMemory");
}

void FMetalDynamicRHI::RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
	NOT_SUPPORTED("RHIVirtualTextureSetFirstMipVisible");
}
