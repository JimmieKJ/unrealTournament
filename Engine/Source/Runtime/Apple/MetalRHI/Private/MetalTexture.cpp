// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalVertexBuffer.cpp: Metal texture RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h" // for STAT_MetalTexturePageOffTime
#include "MetalCommandBuffer.h"

uint8 FMetalSurface::NextKey = 1; // 0 is reserved for MTLPixelFormatInvalid
int32 FMetalSurface::ActiveUploads = 0;

int32 GMetalMaxOutstandingAsyncTexUploads = 0;
FAutoConsoleVariableRef CVarMetalMaxOutstandingAsyncTexUploads(
	TEXT("rhi.Metal.MaxOutstandingAsyncTexUploads"),
	GMetalMaxOutstandingAsyncTexUploads,
	TEXT("The maximum number of outstanding asynchronous texture uploads allowed to be pending in Metal. After the limit is reached the next upload will wait for all outstanding operations to complete and purge the waiting free-lists in order to reduce peak memory consumption. Defaults to 0 (infinite), set to a value > 0 limit the number."),
	ECVF_ReadOnly|ECVF_RenderThreadSafe
	);

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
	
	virtual void* GetTextureBaseRHI() override final
	{
		return GetMetalSurfaceFromRHITexture(GetReferencedTexture());
	}
};

/** Given a pointer to a RHI texture that was created by the Metal RHI, returns a pointer to the FMetalTextureBase it encapsulates. */
FMetalSurface* GetMetalSurfaceFromRHITexture(FRHITexture* Texture)
{
    if (!Texture)
    {
        return NULL;
    }
	else
	{
		FMetalSurface* Surface = (FMetalSurface*)Texture->GetTextureBaseRHI();
		return Surface;
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
	
	if (Flags & (TexCreate_RenderTargetable|TexCreate_DepthStencilTargetable))
	{
		Usage |= MTLTextureUsageRenderTarget;
		Usage |= MTLTextureUsageShaderRead;
		Usage &= ~(MTLTextureUsagePixelFormatView);
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

static bool IsPixelFormatBCCompressed(EPixelFormat Format)
{
	switch (Format)
	{
		case PF_DXT1:
		case PF_DXT3:
		case PF_DXT5:
		case PF_BC4:
		case PF_BC5:
		case PF_BC6H:
		case PF_BC7:
		case PF_PVRTC2:
		case PF_PVRTC4:
			return true;
		default:
			return false;
	}
}

void FMetalSurface::PrepareTextureView()
{
#if PLATFORM_MAC // Recreate the texture to enable MTLTextureUsagePixelFormatView which must be off unless we definitely use this feature or we are throwing ~4% performance vs. Windows on the floor.
	MTLTextureUsage Usage = Texture.usage;
	if(!(Usage & MTLTextureUsagePixelFormatView))
	{
		Usage |= MTLTextureUsagePixelFormatView;
		MTLTextureDescriptor* Desc = [[MTLTextureDescriptor new] autorelease];
		Desc.textureType = Texture.textureType;
		Desc.pixelFormat = Texture.pixelFormat;
		Desc.width = Texture.width;
		Desc.height = Texture.height;
		Desc.depth = Texture.depth;
		Desc.mipmapLevelCount = Texture.mipmapLevelCount;
		Desc.sampleCount = Texture.sampleCount;
		Desc.arrayLength = Texture.arrayLength;
		Desc.resourceOptions = (Texture.cpuCacheMode << MTLResourceCPUCacheModeShift) | (Texture.storageMode << MTLResourceStorageModeShift);
		Desc.cpuCacheMode = Texture.cpuCacheMode;
		Desc.storageMode = Texture.storageMode;
		Desc.usage = Usage;
		
		id<MTLTexture> NewTex = [GetMetalDeviceContext().GetDevice() newTextureWithDescriptor:Desc];
		TRACK_OBJECT(STAT_MetalTextureCount, NewTex);
		
		SafeReleaseMetalResource(Texture);
		Texture = NewTex;
	}
#endif
}

FMetalSurface::FMetalSurface(FMetalSurface& Source, NSRange MipRange)
: Type(Source.Type)
, PixelFormat(Source.PixelFormat)
, Texture(nil)
, MSAATexture(nil)
, StencilTexture(nil)
, SizeX(Source.SizeX)
, SizeY(Source.SizeY)
, SizeZ(Source.SizeZ)
, bIsCubemap(Source.bIsCubemap)
, bWritten(Source.bWritten)
, Flags(Source.Flags)
, WriteLock(0)
, TotalTextureSize(0)
, Viewport(nullptr)
, CoreVideoImageRef(nullptr)
{
	MTLPixelFormat MetalFormat = (MTLPixelFormat)GPixelFormats[PixelFormat].PlatformFormat;
	
	bool const bUseSourceTex = (Source.PixelFormat != PF_DepthStencil) && MipRange.location == 0 && MipRange.length == Source.Texture.mipmapLevelCount;
	
#if PLATFORM_MAC // Recreate the texture to enable MTLTextureUsagePixelFormatView which must be off unless we definitely use this feature or we are throwing ~4% performance vs. Windows on the floor.
	MTLTextureUsage Usage = Source.Texture.usage;
	if(!(Usage & MTLTextureUsagePixelFormatView) && (Source.PixelFormat != PF_DepthStencil) && !bUseSourceTex)
	{
		Source.PrepareTextureView();
	}
#endif
	
	NSRange Slices = NSMakeRange(0, Source.Texture.arrayLength * (bIsCubemap ? 6 : 1));
	// Stencil requires a format conversion, so this will access depth only, consequently there are no mip levels other than 0, so this path requires no real special casing.
#if METAL_API_1_1
    if(Source.PixelFormat != PF_DepthStencil && !bUseSourceTex)
    {
        Texture = [Source.Texture newTextureViewWithPixelFormat:MetalFormat textureType:Source.Texture.textureType levels:MipRange slices:Slices];
		TRACK_OBJECT(STAT_MetalTextureCount, Texture);
	}
    else
#endif
    {
        Texture = [Source.Texture retain];
    }
	if(Source.MSAATexture)
	{
		MSAATexture = [Texture retain];
	}
	if(Source.StencilTexture)
	{
#if PLATFORM_IOS
		check(false); // @todo Zebra Must handle separate stencil texture SRV!
#endif
		StencilTexture = [Texture retain];
	}
	
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	SizeX = FMath::Max(SizeX >> MipRange.location, BlockSizeX);
	SizeY = FMath::Max(SizeY >> MipRange.location, BlockSizeY);
	SizeZ = (Type != RRT_Texture3D) ? SizeZ : FMath::Max(SizeZ >> MipRange.location, 1u);
	
	FMemory::Memzero(LockedMemory, sizeof(LockedMemory));
}

FMetalSurface::FMetalSurface(FMetalSurface& Source, NSRange const MipRange, EPixelFormat Format)
: Type(Source.Type)
, PixelFormat(Format)
, Texture(nil)
, MSAATexture(nil)
, StencilTexture(nil)
, SizeX(Source.SizeX)
, SizeY(Source.SizeY)
, SizeZ(Source.SizeZ)
, bIsCubemap(Source.bIsCubemap)
, bWritten(Source.bWritten)
, Flags(Source.Flags)
, WriteLock(0)
, TotalTextureSize(0)
, Viewport(nullptr)
, CoreVideoImageRef(nullptr)
{
	// You can't format convert an MSAA texture in Metal, so you can't create an SRV via this API for that.
	// Nor can you access the stencil component of an MSAA packed depth-stencil surface, but separate MSAA depth & stencil will be fine.
	check(!Source.MSAATexture || (Format == PF_X24_G8 && Source.Texture != Source.StencilTexture));

	MTLPixelFormat MetalFormat = (MTLPixelFormat)GPixelFormats[PixelFormat].PlatformFormat;
	
	bool const bUseSourceTex = (Source.PixelFormat != PF_DepthStencil) && Source.PixelFormat == Format && MipRange.location == 0 && MipRange.length == Source.Texture.mipmapLevelCount;
	
#if PLATFORM_MAC // Recreate the texture to enable MTLTextureUsagePixelFormatView which must be off unless we definitely use this feature or we are throwing ~4% performance vs. Windows on the floor.
	MTLTextureUsage Usage = Source.Texture.usage;
	if(!(Usage & MTLTextureUsagePixelFormatView) && (Source.PixelFormat == PF_DepthStencil && Format == PF_X24_G8 && GetMetalDeviceContext().SupportsFeature(EMetalFeaturesStencilView)) && !bUseSourceTex)
	{
		Source.PrepareTextureView();
	}
#endif
	
	NSRange Slices = NSMakeRange(0, Source.Texture.arrayLength * (bIsCubemap ? 6 : 1));
    // @todo Zebra Temporary workaround for absence of X24_G8 or equivalent to GL_STENCIL_INDEX so that the stencil part of a texture may be sampled
	// For now, if we find ourselves *requiring* this we lazily blit the stencil data out to a separate texture. radr://21813831
#if METAL_API_1_1
    if(Source.PixelFormat != PF_DepthStencil && !bUseSourceTex)
    {
        Texture = [Source.Texture newTextureViewWithPixelFormat:MetalFormat textureType:Source.Texture.textureType levels:MipRange slices:Slices];
		TRACK_OBJECT(STAT_MetalTextureCount, Texture);
    }
#if PLATFORM_MAC // Currently only required on Mac as iOS uses separate textures
    else if (Source.PixelFormat == PF_DepthStencil && Format == PF_X24_G8 && GetMetalDeviceContext().SupportsFeature(EMetalFeaturesStencilView))
    {
        switch (Source.Texture.pixelFormat)
        {
            case MTLPixelFormatDepth24Unorm_Stencil8:
                MetalFormat = MTLPixelFormatX24_Stencil8;
                break;
            case MTLPixelFormatDepth32Float_Stencil8:
                MetalFormat = MTLPixelFormatX32_Stencil8;
                break;
            default:
                check(false);
                break;
        }
        
        Texture = [Source.Texture newTextureViewWithPixelFormat:MetalFormat textureType:Source.Texture.textureType levels:MipRange slices:Slices];
        TRACK_OBJECT(STAT_MetalTextureCount, Texture);
    }
#endif
    else
#endif
	if(Source.PixelFormat == PF_DepthStencil && Format == PF_X24_G8)
	{
		check(Source.Type == RRT_Texture2D); // Only 2D Textures can be X24_G8 SRVs
		
		MTLPixelFormat DepthStencilFormat = Source.Texture ? Source.Texture.pixelFormat : MTLPixelFormatInvalid;
		
		switch(DepthStencilFormat)
		{
			case MTLPixelFormatStencil8:
			{
				Texture = [Source.Texture retain];
				break;
			}
#if METAL_API_1_1
			case MTLPixelFormatDepth32Float_Stencil8:
#if PLATFORM_MAC
			case MTLPixelFormatDepth24Unorm_Stencil8:
#endif
			{
				// Check for existing stencil texture copy
				if(Source.StencilTexture == Source.Texture)
				{
					// Must create a copy!
					static MTLPixelFormat StencilFormat = FParse::Param(FCommandLine::Get(),TEXT("metalstencilsample")) ? MTLPixelFormatStencil8 : MTLPixelFormatR8Uint;
					
					// Must create a copy! @todo AMD can't sample Stencil8, which must surely be a bug, so use R8Uint for now
					MTLTextureDescriptor* Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:StencilFormat width:Source.SizeX height:Source.SizeY mipmapped:false];
					
					if(GetMetalDeviceContext().SupportsFeature(EMetalFeaturesResourceOptions))
					{
						Desc.usage = ConvertFlagsToUsage(TexCreate_ShaderResource);
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
					
					Source.StencilTexture = [GetMetalDeviceContext().GetDevice() newTextureWithDescriptor:Desc];
					Source.StencilTexture.label = [NSString stringWithFormat:@"%@StencilSRV", Source.Texture.label];
					
					id<MTLBlitCommandEncoder> BlitEncoder = GetMetalDeviceContext().GetBlitContext();
					
					uint32 SizePerImage = Source.Texture.width * Source.Texture.height;
					FMetalPooledBuffer Buf = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetMetalDeviceContext().GetDevice(), SizePerImage, MTLStorageModeShared));
					[Buf.Buffer retain];
					INC_MEMORY_STAT_BY(STAT_MetalWastedPooledBufferMem, Buf.Buffer.length - SizePerImage);
					
					METAL_DEBUG_COMMAND_BUFFER_BLIT_LOG((&GetMetalDeviceContext()), @"FMetalSurface %p(copyFromBuffer: %p toTexture: %p)", this, Buf.Buffer, Source.StencilTexture);
					[BlitEncoder copyFromBuffer:Buf.Buffer sourceOffset:0 sourceBytesPerRow:Source.Texture.width sourceBytesPerImage:SizePerImage sourceSize:MTLSizeMake(Source.Texture.width, Source.Texture.height, 1) toTexture:Source.StencilTexture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
					
					bWritten = true;
					
					DEC_MEMORY_STAT_BY(STAT_MetalWastedPooledBufferMem, Buf.Buffer.length - SizePerImage);
					SafeReleasePooledBuffer(Buf.Buffer);
				}
				//fallthrough to assign the stencil texture
				StencilTexture = [Source.Texture retain];
			}
#endif
			case MTLPixelFormatDepth32Float:
			{
				check(Source.StencilTexture);
				Texture = [Source.StencilTexture retain];
				break;
			}
			default:
			{
				check(false);
				break;
			}
		}
	}
	else
    {
        Texture = [Source.Texture retain];
	}
	
	if(Source.MSAATexture)
	{
		MSAATexture = [Texture retain];
	}
	if(Source.StencilTexture && !StencilTexture)
	{
		StencilTexture = [Texture retain];
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
	, bWritten(false)
	, Flags(InFlags)
    , WriteLock(0)
	, TotalTextureSize(0)
	, Viewport(nullptr)
	, CoreVideoImageRef(nullptr)
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
        if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesDepth16))
        {
            PixelFormatKeyMap.Add(MTLPixelFormatDepth16Unorm, NextKey++);
        }
#endif
#endif
	}
	
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
			case MTLPixelFormatR8Unorm:
				// For now R8 sRGB expansion is 2D only, log other usage for later.
				if(Type == RRT_Texture2D)
				{
					MTLFormat = MTLPixelFormatRGBA8Unorm_sRGB;
				}
				else
				{
					UE_LOG(LogMetal, Error, TEXT("Attempting to use unsupported MTLPixelFormatR8Unorm_sRGB on Mac with texture type: %d, no format expansion will be provided so rendering errors may occur."), Type);
				}
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

	// Dirty, filty hacks to pass through via an CVImageBufferRef or CVMetalTextureRef
	bool const bIOSurfaceData = (BulkData && BulkData->GetResourceBulkDataSize() == ~0u);
	if(bIOSurfaceData)
	{
		checkf(NumMips == 1 && ArraySize == 1, TEXT("Only handling bulk data with 1 mip and 1 array length"));
		CoreVideoImageRef = (CFTypeRef)BulkData->GetResourceBulkData();
		CFRetain(CoreVideoImageRef);
		{
#if PLATFORM_MAC
			Texture = [GetMetalDeviceContext().GetDevice() newTextureWithDescriptor:Desc iosurface:CVPixelBufferGetIOSurface((CVPixelBufferRef)CoreVideoImageRef) plane:0];
#else
			Texture = [CVMetalTextureGetTexture((CVMetalTextureRef)CoreVideoImageRef) retain];
#endif
			
			if (Texture == nil)
			{
				UE_LOG(LogMetal, Fatal, TEXT("Failed to create texture, desc %s"), *FString([Desc description]));
			}
		}
		TRACK_OBJECT(STAT_MetalTextureCount, Texture);
		
		BulkData->Discard();
	}
	else
	{
		Texture = [GetMetalDeviceContext().GetDevice() newTextureWithDescriptor:Desc];
		if (Texture == nil)
		{
			UE_LOG(LogMetal, Fatal, TEXT("Failed to create texture, desc %s"), *FString([Desc description]));
		}
		TRACK_OBJECT(STAT_MetalTextureCount, Texture);
		
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
				Texture = [MSAATexture retain];

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
			TRACK_OBJECT(STAT_MetalTextureCount, MSAATexture);
		}
	}

	// create a stencil buffer if needed
	if (Format == PF_DepthStencil)
	{
#if PLATFORM_IOS
		Desc.pixelFormat = MTLPixelFormatStencil8;
		StencilTexture = [GetMetalDeviceContext().GetDevice() newTextureWithDescriptor:Desc];
		
		// 1 byte per texel
		TotalTextureSize += SizeX * SizeY;
#else
		StencilTexture = [Texture retain];
		
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
	bool const bIsRenderTarget = IsRenderTarget(Flags);
	
#if STATS
	if (Type == RRT_TextureCube)
	{
		if (bIsRenderTarget)
		{
			DEC_MEMORY_STAT_BY(STAT_RenderTargetMemoryCube, TotalTextureSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_TextureMemoryCube, TotalTextureSize);
		}
	}
	else if (Type == RRT_Texture3D)
	{
		if (bIsRenderTarget)
		{
			DEC_MEMORY_STAT_BY(STAT_RenderTargetMemory3D, TotalTextureSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_TextureMemory3D, TotalTextureSize);
		}
	}
	else
	{
		if (bIsRenderTarget)
		{
			DEC_MEMORY_STAT_BY(STAT_RenderTargetMemory2D, TotalTextureSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_TextureMemory2D, TotalTextureSize);
		}
	}
#endif
	
	// track memory usage
	if (bIsRenderTarget)
	{
		GCurrentRendertargetMemorySize -= Align(TotalTextureSize, 1024) / 1024;
	}
	else
	{
		GCurrentTextureMemorySize -= Align(TotalTextureSize, 1024) / 1024;
	}
	
	if (!(Flags & TexCreate_Presentable) && Texture != nil)
	{
		SafeReleaseMetalResource(Texture);
	}

	if (MSAATexture != nil)
    {
		if (Texture != MSAATexture)
		{
			SafeReleaseMetalResource(MSAATexture);
		}
		else
		{
			[MSAATexture release];
		}
	}
	
	if (StencilTexture != nil)
	{
		if (StencilTexture != Texture)
		{
			SafeReleaseMetalResource(StencilTexture);
		}
		else
		{
			[StencilTexture release];
		}
	}
	
	if(CoreVideoImageRef)
	{
		CFRelease(CoreVideoImageRef);
	}

	CoreVideoImageRef = nullptr;
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
	// Whether the device supports resource options, so we don't access invalid properties on older versions of iOS
	bool const bSupportsResourceOptions = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesResourceOptions);
	
	// get size and stride
#if METAL_API_1_1
	bool const bAlignedStride = (bSupportsResourceOptions && Texture.storageMode == MTLStorageModePrivate);
#else
	bool const bAlignedStride = false; // Always Shared memory, so won't use the blit command encoder operations.
#endif
	const uint32 MipBytes = GetMipSize(MipIndex, &DestStride, false, bAlignedStride);
	
	// allocate some temporary memory
	if(!LockedMemory[MipIndex])
	{
#if METAL_API_1_1
		NSUInteger ResMode = MTLResourceStorageModeShared | (bSupportsResourceOptions && !(PixelFormat == PF_G8 && (Flags & TexCreate_SRGB)) ? MTLResourceCPUCacheModeWriteCombined : 0);
#else
		NSUInteger ResMode = MTLStorageModeShared;
#endif
		LockedMemory[MipIndex] = [GetMetalDeviceContext().GetDevice() newBufferWithLength:MipBytes options:ResMode];
		TRACK_OBJECT(STAT_MetalBufferCount, LockedMemory[MipIndex]);
	}
	
    switch(LockMode)
    {
        case RLM_ReadOnly:
        {
			SCOPE_CYCLE_COUNTER(STAT_MetalTexturePageOffTime);

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

#if METAL_API_1_1
			id<MTLBlitCommandEncoder> Blitter = GetMetalDeviceContext().GetBlitContext();
			
			if (bSupportsResourceOptions && Texture.storageMode == MTLStorageModePrivate)
			{
				METAL_DEBUG_COMMAND_BUFFER_BLIT_LOG((&GetMetalDeviceContext()), @"FMetalSurface::Lock %p(copyFromTexture:toBuffer:)", this);
				[Blitter copyFromTexture:Texture sourceSlice:ArrayIndex sourceLevel:MipIndex sourceOrigin:Region.origin sourceSize:Region.size toBuffer:LockedMemory[MipIndex] destinationOffset:0 destinationBytesPerRow:DestStride destinationBytesPerImage:MipBytes];
				
				//kick the current command buffer.
				GetMetalDeviceContext().SubmitCommandBufferAndWait();
			}
			else
#endif
			{
#if METAL_API_1_1 && PLATFORM_MAC
				[Blitter synchronizeTexture:Texture slice:ArrayIndex level:MipIndex];
				
				//kick the current command buffer.
				GetMetalDeviceContext().SubmitCommandBufferAndWait();
#endif
				[Texture getBytes:[LockedMemory[MipIndex] contents] bytesPerRow:DestStride bytesPerImage:MipBytes fromRegion:Region mipmapLevel:MipIndex slice:ArrayIndex];
			}
			
#if PLATFORM_MAC
			// Pack RGBA8_sRGB into R8_sRGB for Mac.
			if (PixelFormat == PF_G8 && (Flags & TexCreate_SRGB) && Type == RRT_Texture2D)
			{
				TArray<uint8> Data;
				uint8* ExpandedMem = (uint8*)[LockedMemory[MipIndex] contents];
				Data.Append(ExpandedMem, MipBytes);
				uint32 SrcStride = DestStride;
				DestStride = FMath::Max<uint32>(SizeX >> MipIndex, 1);
				for(uint y = 0; y < FMath::Max<uint32>(SizeY >> MipIndex, 1); y++)
				{
					uint8* RowDest = ExpandedMem;
					for(uint x = 0; x < FMath::Max<uint32>(SizeX >> MipIndex, 1); x++)
					{
						*(RowDest++) = Data[(y * SrcStride) + (x * 4)];
					}
					ExpandedMem = (ExpandedMem + DestStride);
				}
			}
#endif
			
			break;
        }
        case RLM_WriteOnly:
        {
			WriteLock |= 1 << MipIndex;
#if PLATFORM_MAC
			// Expand R8_sRGB into RGBA8_sRGB for Mac.
			if (PixelFormat == PF_G8 && (Flags & TexCreate_SRGB) && Type == RRT_Texture2D)
			{
				DestStride = FMath::Max<uint32>(SizeX >> MipIndex, 1);
			}
#endif
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
		// Whether the device supports resource options, so we don't access invalid properties on older versions of iOS
		bool const bSupportsResourceOptions = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesResourceOptions);
		
        WriteLock &= ~(1 << MipIndex);
        uint32 Stride;
#if METAL_API_1_1
		bool const bAlignedStride = (bSupportsResourceOptions && Texture.storageMode == MTLStorageModePrivate);
#else
		bool const bAlignedStride = false; // Always Shared memory, so won't use the blit command encoder operations.
#endif
        uint32 BytesPerImage = GetMipSize(MipIndex, &Stride, true, bAlignedStride);

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
		// Expand R8_sRGB into RGBA8_sRGB for Mac.
		if (PixelFormat == PF_G8 && (Flags & TexCreate_SRGB) && Type == RRT_Texture2D)
		{
			TArray<uint8> Data;
			uint8* ExpandedMem = (uint8*)[LockedMemory[MipIndex] contents];
			Data.Append(ExpandedMem, BytesPerImage);
			uint32 SrcStride = FMath::Max<uint32>(SizeX >> MipIndex, 1);
			for(uint y = 0; y < FMath::Max<uint32>(SizeY >> MipIndex, 1); y++)
			{
				uint8* RowDest = ExpandedMem;
				for(uint x = 0; x < FMath::Max<uint32>(SizeX >> MipIndex, 1); x++)
				{
					*(RowDest++) = Data[(y * SrcStride) + x];
					*(RowDest++) = Data[(y * SrcStride) + x];
					*(RowDest++) = Data[(y * SrcStride) + x];
					*(RowDest++) = Data[(y * SrcStride) + x];
				}
				ExpandedMem = (ExpandedMem + Stride);
			}
		}
#endif

#if METAL_API_1_1
		if(bSupportsResourceOptions && Texture.storageMode == MTLStorageModePrivate)
		{
			SCOPED_AUTORELEASE_POOL;
			
			int32 Count = FPlatformAtomics::InterlockedIncrement(&ActiveUploads);
			
			bool const bWait = ((GetMetalDeviceContext().GetNumActiveContexts() == 1) && (GMetalMaxOutstandingAsyncTexUploads > 0) && (Count >= GMetalMaxOutstandingAsyncTexUploads));
			
			// @todo: gather these all up over a frame
			id<MTLCommandBuffer> CommandBuffer = GetMetalDeviceContext().CreateCommandBuffer(false/*bRetainReferences*/);
			
			// create a blitter object
			id<MTLBlitCommandEncoder> Blitter = [CommandBuffer blitCommandEncoder];
			
			METAL_DEBUG_COMMAND_BUFFER_BLIT_ASYNC_LOG((&GetMetalDeviceContext()), CommandBuffer, @"FMetalSurface::Unlock %p(copyFromBuffer:toTexture)", this);
			[Blitter copyFromBuffer:LockedMemory[MipIndex] sourceOffset:0 sourceBytesPerRow:Stride sourceBytesPerImage:BytesPerImage sourceSize:Region.size toTexture:Texture destinationSlice:ArrayIndex destinationLevel:MipIndex destinationOrigin:Region.origin];
			
			[Blitter endEncoding];
			[CommandBuffer addCompletedHandler : ^ (id <MTLCommandBuffer> CompletedBuffer)
			{
				FPlatformAtomics::InterlockedDecrement(&ActiveUploads);
			}];
			GetMetalDeviceContext().GetCommandList().Commit(CommandBuffer, bWait);
			GetMetalDeviceContext().ReleaseObject(LockedMemory[MipIndex]);
			
			if (bWait)
			{
				GetMetalDeviceContext().ClearFreeList();
			}
		}
		else
#endif
		{
#if !PLATFORM_MAC
			if (Texture.pixelFormat >= MTLPixelFormatPVRTC_RGB_2BPP && Texture.pixelFormat <= MTLPixelFormatETC2_RGB8A1_sRGB) // @todo zebra
			{
				Stride = 0;
				BytesPerImage = 0;
			}
#endif
			
			[Texture replaceRegion:Region mipmapLevel:MipIndex slice:ArrayIndex withBytes:[LockedMemory[MipIndex] contents] bytesPerRow:Stride bytesPerImage:BytesPerImage];
			[LockedMemory[MipIndex] release];
		}
		LockedMemory[MipIndex] = nullptr;
		
		bWritten = true;
    }
}

uint32 FMetalSurface::GetMipSize(uint32 MipIndex, uint32* Stride, bool bSingleLayer, bool bBlitAligned)
{
	// Calculate the dimensions of the mip-map.
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	const uint32 Alignment = (bBlitAligned && !PLATFORM_MAC) ? 64u : 1u; // Mac permits natural row alignment (tightly-packed) but iOS does not.
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
#if PLATFORM_MAC
	else if (PixelFormat == PF_G8 && (Flags & TexCreate_SRGB))
	{
		// RGBA_sRGB is the closest match - so expand the data.
		NumBlocksX *= 4;
	}
#endif

	const uint32 MipStride = NumBlocksX * BlockBytes;
	const uint32 AlignedStride = ((MipStride - 1) & ~(Alignment - 1)) + Alignment;
	
	const uint32 MipBytes = AlignedStride * NumBlocksY * MipSizeZ;

	if (Stride)
	{
		*Stride = AlignedStride;
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
		TotalSize += GetMipSize(MipIndex, NULL, false, false);
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

id<MTLTexture> FMetalSurface::GetDrawableTexture()
{
	if (!Texture && (Flags & TexCreate_Presentable))
	{
		check(Viewport);
		Texture = Viewport->GetDrawableTexture();
	}
	return Texture;
}

void FMetalSurface::UpdateSRV(FTextureRHIRef SourceTex)
{
#if METAL_API_1_1
	if (PixelFormat == PF_X24_G8 && Texture != StencilTexture && GetMetalDeviceContext().SupportsFeature(EMetalFeaturesDepthStencilBlitOptions) && !GetMetalDeviceContext().SupportsFeature(EMetalFeaturesStencilView))
	{
		// In this case StencilTexture is the source depth/stencil texture and Texture is our target Stencil-only copy
		
		id<MTLBlitCommandEncoder> BlitEncoder = FMetalContext::GetCurrentContext()->GetBlitContext();
		
		uint32 SizePerImage = Texture.width * Texture.height;
		FMetalPooledBuffer Buf = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetMetalDeviceContext().GetDevice(), SizePerImage, MTLStorageModeShared));
		[Buf.Buffer retain];
		INC_MEMORY_STAT_BY(STAT_MetalWastedPooledBufferMem, Buf.Buffer.length - SizePerImage);
		
		METAL_DEBUG_COMMAND_BUFFER_BLIT_LOG((&GetMetalDeviceContext()), @"FMetalSurface::UpdateSRV %p(Source %p)", this, SourceTex.GetReference());
		[BlitEncoder copyFromTexture:StencilTexture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(Texture.width, Texture.height, 1) toBuffer:Buf.Buffer destinationOffset:0 destinationBytesPerRow:Texture.width destinationBytesPerImage:SizePerImage options:MTLBlitOptionStencilFromDepthStencil];
		
		[BlitEncoder copyFromBuffer:Buf.Buffer sourceOffset:0 sourceBytesPerRow:Texture.width sourceBytesPerImage:SizePerImage sourceSize:MTLSizeMake(Texture.width, Texture.height, 1) toTexture:Texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
		
		bWritten = true;
		
		DEC_MEMORY_STAT_BY(STAT_MetalWastedPooledBufferMem, Buf.Buffer.length - SizePerImage);
		SafeReleasePooledBuffer(Buf.Buffer);
	}
	// Handle the case where a texture or render-target is created without PixelFormatView & then recreated with it enabled in order to create appropriate SRVs.
	// If an existing SRV simply exposed the source format & mip-levels it won't have a pixel-format-view so we need to update the texture ref we return.
	else if (PixelFormat == SourceTex->GetFormat() && Texture.mipmapLevelCount == SourceTex->GetNumMips())
	{
		FMetalSurface* Surf = GetMetalSurfaceFromRHITexture(SourceTex);
		check(Surf);
		if (Texture != Surf->Texture)
		{
			if (MSAATexture == Texture)
			{
				MSAATexture = [Surf->Texture retain];
			}
			if (StencilTexture == Texture)
			{
				StencilTexture = [Surf->Texture retain];
			}
			SafeReleaseMetalResource(Texture);
			Texture = [Surf->Texture retain];
			
			bWritten = true;
		}
	}
#endif
}


/*-----------------------------------------------------------------------------
	Texture allocator support.
-----------------------------------------------------------------------------*/

void FMetalDynamicRHI::RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats)
{
	if(MemoryStats.TotalGraphicsMemory > 0)
	{
		OutStats.DedicatedVideoMemory = MemoryStats.DedicatedVideoMemory;
		OutStats.DedicatedSystemMemory = MemoryStats.DedicatedSystemMemory;
		OutStats.SharedSystemMemory = MemoryStats.SharedSystemMemory;
		OutStats.TotalGraphicsMemory = MemoryStats.TotalGraphicsMemory;
	}
	else
	{
		OutStats.DedicatedVideoMemory = 0;
		OutStats.DedicatedSystemMemory = 0;
		OutStats.SharedSystemMemory = 0;
		OutStats.TotalGraphicsMemory = 0;
	}

	OutStats.AllocatedMemorySize = int64(GCurrentTextureMemorySize) * 1024;
	OutStats.LargestContiguousAllocation = OutStats.AllocatedMemorySize;
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
	FMetalSurface* Surf = GetMetalSurfaceFromRHITexture(SourceSurfaceRHI);
	if (Surf && Surf->Texture)
	{
		// @todo: gather these all up over a frame
		id<MTLCommandBuffer> CommandBuffer = GetMetalDeviceContext().CreateCommandBuffer(false/*bRetainReferences*/);
		
		// create a blitter object
		id<MTLBlitCommandEncoder> Blitter = [CommandBuffer blitCommandEncoder];
		
		METAL_DEBUG_COMMAND_BUFFER_BLIT_ASYNC_LOG((&GetMetalDeviceContext()), CommandBuffer, @"RHIGenerateMips %p(Source %p)", this, SourceSurfaceRHI);
		[Blitter generateMipmapsForTexture:Surf->Texture];
		
		// kick it off!
		[Blitter endEncoding];
		GetMetalDeviceContext().GetCommandList().Commit(CommandBuffer, false);
		
		if (GIsRHIInitialized)
		{
			// Queue a submit of all outstanding work to ensure serialisation order of commands
			FRHICommandListExecutor::GetImmediateCommandList().SubmitCommandsHint();
		}
	}
}

FTexture2DRHIRef FMetalDynamicRHI::AsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	FTexture2DRHIRef Result = GDynamicRHI->RHIAsyncReallocateTexture2D(Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
	
	if (GIsRHIInitialized)
	{
		// Queue a submit of all outstanding work to ensure serialisation order of commands
		RHICmdList.SubmitCommandsHint();
	}
	
	return Result;
}

ETextureReallocationStatus FMetalDynamicRHI::FinalizeAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
{
	// No need to flush - does nothing
	return GDynamicRHI->RHIFinalizeAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

ETextureReallocationStatus FMetalDynamicRHI::CancelAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
{
	// No need to flush - does nothing
	return GDynamicRHI->RHICancelAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
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
	
	const uint32 BlockSizeX = GPixelFormats[OldTexture->GetFormat()].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[OldTexture->GetFormat()].BlockSizeY;

	// only handling straight 2D textures here
	uint32 SliceIndex = 0;
	MTLOrigin Origin = MTLOriginMake(0,0,0);
	
	METAL_DEBUG_COMMAND_BUFFER_BLIT_ASYNC_LOG((&GetMetalDeviceContext()), CommandBuffer, @"RHIAsyncReallocateTexture2D(OldTextureRHI %p, NewMipCount %d, NewSizeX %d, NewSizeY %d) -> %p", OldTextureRHI, NewMipCount, NewSizeX, NewSizeY, NewTexture);
	
	id<MTLTexture> Tex = OldTexture->Surface.Texture;
	[Tex retain];

	// DXT/BC formats on Mac actually do have mip-tails that are smaller than the block size, they end up being uncompressed.
	bool const bPixelFormatBC = IsPixelFormatBCCompressed(OldTexture->GetFormat());

	for (uint32 MipIndex = 0; MipIndex < NumSharedMips; ++MipIndex)
	{
		const uint32 UnalignedMipSizeX = FMath::Max<uint32>(1, NewSizeX >> (MipIndex + DestMipOffset));
		const uint32 UnalignedMipSizeY = FMath::Max<uint32>(1, NewSizeY >> (MipIndex + DestMipOffset));
		const uint32 MipSizeX = (!bPixelFormatBC || UnalignedMipSizeX >= BlockSizeX) ? AlignArbitrary(UnalignedMipSizeX, BlockSizeX) : UnalignedMipSizeX;
		const uint32 MipSizeY = (!bPixelFormatBC || UnalignedMipSizeY >= BlockSizeY) ? AlignArbitrary(UnalignedMipSizeY, BlockSizeY) : UnalignedMipSizeY;

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
	GetMetalDeviceContext().GetCommandList().Commit(CommandBuffer, false);

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

void* FMetalDynamicRHI::LockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush)
{
	// Never flush for writing, it is unnecessary
	if (bNeedsDefaultRHIFlush && LockMode == RLM_ReadOnly)
	{
		// @todo Not all read locks need to flush either, but that'll require resource use tracking
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockTexture2D_Flush);
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	}
	return GDynamicRHI->RHILockTexture2D(Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail);
}

void FMetalDynamicRHI::UnlockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush)
{
	FMetalTexture2D* TextureMTL = ResourceCast(Texture);
	bool const bSubmit = GIsRHIInitialized && (TextureMTL->Surface.WriteLock != 0);
	
	// Unlocks require no flushing, only submission for write locks
	GDynamicRHI->RHIUnlockTexture2D(Texture, MipIndex, bLockWithinMiptail);
	
	if(bSubmit)
	{
		// Queue a submit of all outstanding work to ensure serialisation order of commands
		RHICmdList.SubmitCommandsHint();
	}
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
	
	if (GIsRHIInitialized)
	{
		// Queue a submit of all outstanding work to ensure serialisation order of commands
		FRHICommandListExecutor::GetImmediateCommandList().SubmitCommandsHint();
	}
}

void FMetalDynamicRHI::UpdateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{
	this->RHIUpdateTexture2D(Texture, MipIndex, UpdateRegion, SourcePitch, SourceData);
	
	if (GIsRHIInitialized)
	{
		// Queue a submit of all outstanding work to ensure serialisation order of commands
		RHICmdList.SubmitCommandsHint();
	}
}

void FMetalDynamicRHI::RHIUpdateTexture2D(FTexture2DRHIParamRef TextureRHI, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{	
	FMetalTexture2D* Texture = ResourceCast(TextureRHI);
	
	id<MTLTexture> Tex = Texture->Surface.Texture;
	
	MTLRegion Region = MTLRegionMake2D(UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.Width, UpdateRegion.Height);
	
#if PLATFORM_MAC
	// Expand R8_sRGB into RGBA8_sRGB for Mac.
	TArray<uint8> Data;
	if (Texture->GetFormat() == PF_G8 && (Texture->GetFlags() & TexCreate_SRGB))
	{
		uint32 BytesPerImage = SourcePitch * UpdateRegion.Height;
		Data.AddZeroed(BytesPerImage * 4);
		uint8* Dest = Data.GetData();
		
		for(uint y = 0; y < UpdateRegion.Height; y++)
		{
			uint8* RowDest = Dest;
			for(uint x = 0; x < UpdateRegion.Width; x++)
			{
				*(RowDest++) = SourceData[(y * SourcePitch) + x];
				*(RowDest++) = SourceData[(y * SourcePitch) + x];
				*(RowDest++) = SourceData[(y * SourcePitch) + x];
				*(RowDest++) = SourceData[(y * SourcePitch) + x];
			}
			Dest = (Dest + SourcePitch);
		}
		
		SourceData = Data.GetData();
		SourcePitch *= 4;
	}
	
	if(Tex.storageMode == MTLStorageModePrivate)
	{
		SCOPED_AUTORELEASE_POOL;
		
		// @todo: gather these all up over a frame
		id<MTLCommandBuffer> CommandBuffer = GetMetalDeviceContext().CreateCommandBuffer(false/*bRetainReferences*/);
		
		// create a blitter object
		id<MTLBlitCommandEncoder> Blitter = [CommandBuffer blitCommandEncoder];
		
		uint32 BytesPerImage = SourcePitch * UpdateRegion.Height;
		
		MTLStorageMode ResMode = MTLStorageModeShared;
		
		const uint32 BufferSize = UpdateRegion.Height*SourcePitch;
		
		FMetalPooledBuffer Buf = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetMetalDeviceContext().GetDevice(), BufferSize, ResMode));
		INC_MEMORY_STAT_BY(STAT_MetalWastedPooledBufferMem, Buf.Buffer.length - BufferSize);
		id<MTLBuffer> LockedMemory = [Buf.Buffer retain];
		
		FMemory::Memcpy([LockedMemory contents], SourceData, BufferSize);
		
		METAL_DEBUG_COMMAND_BUFFER_BLIT_ASYNC_LOG((&GetMetalDeviceContext()), CommandBuffer, @"RHIUpdateTexture2D(TextureRHI %p, MipIndex %d, SourcePitch %d)", TextureRHI, MipIndex, SourcePitch);
		
		[Blitter copyFromBuffer:LockedMemory sourceOffset:0 sourceBytesPerRow:SourcePitch sourceBytesPerImage:BytesPerImage sourceSize:Region.size toTexture:Tex destinationSlice:0 destinationLevel:MipIndex destinationOrigin:Region.origin];
		
		[Blitter endEncoding];
		GetMetalDeviceContext().GetCommandList().Commit(CommandBuffer, false);
		
		DEC_MEMORY_STAT_BY(STAT_MetalWastedPooledBufferMem, Buf.Buffer.length - BufferSize);
		GetMetalDeviceContext().ReleasePooledBuffer(LockedMemory);
	}
	else
#endif
	{
		[Tex replaceRegion:Region mipmapLevel:MipIndex slice:0 withBytes:SourceData bytesPerRow:SourcePitch bytesPerImage:0];
	}
	
	Texture->Surface.bWritten = true;
}

void FMetalDynamicRHI::RHIUpdateTexture3D(FTexture3DRHIParamRef TextureRHI,uint32 MipIndex,const FUpdateTextureRegion3D& UpdateRegion,uint32 SourceRowPitch,uint32 SourceDepthPitch,const uint8* SourceData)
{	
	FMetalTexture3D* Texture = ResourceCast(TextureRHI);

	id<MTLTexture> Tex = Texture->Surface.Texture;

	MTLRegion Region = MTLRegionMake3D(UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.DestZ, UpdateRegion.Width, UpdateRegion.Height, UpdateRegion.Depth);
	
#if PLATFORM_MAC
	checkf(!(Texture->GetFormat() == PF_G8 && (Texture->GetFlags() & TexCreate_SRGB)), TEXT("MetalRHI does not support PF_G8_sRGB on 3D, array or cube textures as it requires manual, CPU-side expansion to RGBA8_sRGB which is expensive!"));
	if(Tex.storageMode == MTLStorageModePrivate)
	{
		SCOPED_AUTORELEASE_POOL;
		
		// @todo: gather these all up over a frame
		id<MTLCommandBuffer> CommandBuffer = GetMetalDeviceContext().CreateCommandBuffer(false/*bRetainReferences*/);
		
		// create a blitter object
		id<MTLBlitCommandEncoder> Blitter = [CommandBuffer blitCommandEncoder];
		
		uint32 BytesPerImage = SourceRowPitch * UpdateRegion.Height;
		
		MTLStorageMode ResMode = MTLStorageModeShared;
		const uint32 BufferSize = UpdateRegion.Height*UpdateRegion.Depth*SourceRowPitch;
		
		FMetalPooledBuffer Buf = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetMetalDeviceContext().GetDevice(), BufferSize, ResMode));
		INC_MEMORY_STAT_BY(STAT_MetalWastedPooledBufferMem, Buf.Buffer.length - BufferSize);
		id<MTLBuffer> LockedMemory = [Buf.Buffer retain];

		FMemory::Memcpy([LockedMemory contents], SourceData, BufferSize);
		
		METAL_DEBUG_COMMAND_BUFFER_BLIT_ASYNC_LOG((&GetMetalDeviceContext()), CommandBuffer, @"RHIUpdateTexture3D(TextureRHI %p, MipIndex %d, SourceRowPitch %d, SourceDepthPitch %d)", TextureRHI, MipIndex, SourceRowPitch, SourceDepthPitch);
		
		[Blitter copyFromBuffer:LockedMemory sourceOffset:0 sourceBytesPerRow:SourceRowPitch sourceBytesPerImage:BytesPerImage sourceSize:Region.size toTexture:Tex destinationSlice:0 destinationLevel:MipIndex destinationOrigin:Region.origin];
		
		[Blitter endEncoding];
		GetMetalDeviceContext().GetCommandList().Commit(CommandBuffer, false);
		
		DEC_MEMORY_STAT_BY(STAT_MetalWastedPooledBufferMem, Buf.Buffer.length - BufferSize);
		GetMetalDeviceContext().ReleasePooledBuffer(LockedMemory);
	}
	else
#endif
	{
		[Tex replaceRegion:Region mipmapLevel:MipIndex slice:0 withBytes:SourceData bytesPerRow:SourceRowPitch bytesPerImage:SourceDepthPitch];
	}
	
	Texture->Surface.bWritten = true;
	
	if (GIsRHIInitialized)
	{
		// Queue a submit of all outstanding work to ensure serialisation order of commands
		FRHICommandListExecutor::GetImmediateCommandList().SubmitCommandsHint();
	}
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
	
	if (GIsRHIInitialized)
	{
		// Queue a submit of all outstanding work to ensure serialisation order of commands
		FRHICommandListExecutor::GetImmediateCommandList().SubmitCommandsHint();
	}
}


FTexture2DRHIRef FMetalDynamicRHI::RHICreateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	FTexture2DRHIRef Result = GDynamicRHI->RHICreateTexture2D(SizeX, SizeY, Format, NumMips, NumSamples, Flags, CreateInfo);
	// Queue a submit of all outstanding work to ensure serialisation order of commands if we actually wrote anything into the texture, otherwise we needn't do anything.
	if ( CreateInfo.BulkData && GIsRHIInitialized )
	{
		RHICmdList.SubmitCommandsHint();
	}
	return Result;
}

FTexture2DArrayRHIRef FMetalDynamicRHI::RHICreateTexture2DArray_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	FTexture2DArrayRHIRef Result = GDynamicRHI->RHICreateTexture2DArray(SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo);
	// Queue a submit of all outstanding work to ensure serialisation order of commands if we actually wrote anything into the texture, otherwise we needn't do anything.
	if ( CreateInfo.BulkData && GIsRHIInitialized )
	{
		RHICmdList.SubmitCommandsHint();
	}
	return Result;
}

FTexture3DRHIRef FMetalDynamicRHI::RHICreateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	FTexture3DRHIRef Result = GDynamicRHI->RHICreateTexture3D(SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo);
	// Queue a submit of all outstanding work to ensure serialisation order of commands if we actually wrote anything into the texture, otherwise we needn't do anything.
	if ( CreateInfo.BulkData && GIsRHIInitialized )
	{
		RHICmdList.SubmitCommandsHint();
	}
	return Result;
}

FTextureCubeRHIRef FMetalDynamicRHI::RHICreateTextureCube_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	FTextureCubeRHIRef Result = GDynamicRHI->RHICreateTextureCube(Size, Format, NumMips, Flags, CreateInfo);
	// Queue a submit of all outstanding work to ensure serialisation order of commands if we actually wrote anything into the texture, otherwise we needn't do anything.
	if ( CreateInfo.BulkData && GIsRHIInitialized )
	{
		RHICmdList.SubmitCommandsHint();
	}
	return Result;
}

FTextureCubeRHIRef FMetalDynamicRHI::RHICreateTextureCubeArray_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	FTextureCubeRHIRef Result = GDynamicRHI->RHICreateTextureCubeArray(Size, ArraySize, Format, NumMips, Flags, CreateInfo);
	// Queue a submit of all outstanding work to ensure serialisation order of commands if we actually wrote anything into the texture, otherwise we needn't do anything.
	if ( CreateInfo.BulkData && GIsRHIInitialized )
	{
		RHICmdList.SubmitCommandsHint();
	}
	return Result;
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
#if METAL_API_1_1
		if(Surf->StencilTexture != Surf->Texture)
		{
			Surf->StencilTexture.label = [NSString stringWithFormat:@"%@StencilSRV", FString(Name).GetNSString()];
		}
		else
		{
			Surf->StencilTexture.label = FString(Name).GetNSString();
		}
#endif
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
