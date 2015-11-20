// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRenderTarget.cpp: Metal render target implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "ScreenRendering.h"
#include "MetalProfiler.h"


void FMetalRHICommandContext::RHICopyToResolveTarget(FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI, bool bKeepOriginalSurface, const FResolveParams& ResolveParams)
{
	if(SourceTextureRHI != DestTextureRHI)
	{
		id<MTLBlitCommandEncoder> Blitter = Context->GetBlitContext();
		
		FMetalSurface* Source = GetMetalSurfaceFromRHITexture(SourceTextureRHI);
		FMetalSurface* Destination = GetMetalSurfaceFromRHITexture(DestTextureRHI);
		
		switch (Source->Type)
		{
			case RRT_Texture2D:
				break;
			case RRT_TextureCube:
				check(Source->SizeZ == 6); // Arrays might not work yet.
				break;
			default:
				check(false); // Only Tex2D & Cube are tested to work so far!
				break;
		}
		switch (Destination->Type)
		{
			case RRT_Texture2D:
				break;
			case RRT_TextureCube:
				check(Destination->SizeZ == 6); // Arrays might not work yet.
				break;
			default:
				check(false); // Only Tex2D & Cube are tested to work so far!
				break;
		}
		
		MTLOrigin Origin = MTLOriginMake(0, 0, 0);
		MTLSize Size = MTLSizeMake(0, 0, 1);
		if (ResolveParams.Rect.IsValid())
		{
			// Partial copy
			Origin.x = ResolveParams.Rect.X1;
			Origin.y = ResolveParams.Rect.Y1;
			Size.width = ResolveParams.Rect.X2 - ResolveParams.Rect.X1;
			Size.height = ResolveParams.Rect.Y2 - ResolveParams.Rect.Y1;
		}
		else
		{
			// Whole of source copy
			Origin.x = 0;
			Origin.y = 0;
			
			Size.width = FMath::Max<uint32>(1, Source->SizeX >> ResolveParams.MipIndex);
			Size.height = FMath::Max<uint32>(1, Source->SizeY >> ResolveParams.MipIndex);
		}
		
		const bool bSrcCubemap  = Source->bIsCubemap;
		const bool bDestCubemap = Destination->bIsCubemap;
		
		uint32 DestIndex = ResolveParams.DestArrayIndex * (bDestCubemap ? 6 : 1) + (bDestCubemap ? uint32(ResolveParams.CubeFace) : 0);
		uint32 SrcIndex  = ResolveParams.SourceArrayIndex * (bSrcCubemap ? 6 : 1) + (bSrcCubemap ? uint32(ResolveParams.CubeFace) : 0);
		
		if(Profiler)
		{
			Profiler->RegisterGPUWork();
		}
		
		[Blitter copyFromTexture:Source->Texture sourceSlice:SrcIndex sourceLevel:ResolveParams.MipIndex sourceOrigin:Origin sourceSize:Size toTexture:Destination->Texture destinationSlice:DestIndex destinationLevel:ResolveParams.MipIndex destinationOrigin:Origin];
	}
}

void FMetalDynamicRHI::RHIReadSurfaceData(FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags)
{
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);

	// verify the input image format (but don't crash)
	if (Surface->PixelFormat != PF_B8G8R8A8 && Surface->PixelFormat != PF_FloatRGBA)
	{
		UE_LOG(LogRHI, Log, TEXT("Trying to read an unsupported surface format."));
	}
	
	// allocate output space
	const uint32 SizeX = Rect.Width();
	const uint32 SizeY = Rect.Height();
	OutData.Empty();
	OutData.AddUninitialized(SizeX * SizeY);

	FColor* OutDataPtr = OutData.GetData();
	MTLRegion Region = MTLRegionMake2D(Rect.Min.X, Rect.Min.Y, SizeX, SizeY);
    
    id<MTLTexture> Texture = Surface->Texture;
    if(!Texture && (Surface->Flags & TexCreate_Presentable))
    {
        id<CAMetalDrawable> Drawable = (id<CAMetalDrawable>)GetMetalDeviceContext().GetCurrentState().GetCurrentDrawable();
        Texture = Drawable ? Drawable.texture : nil;
    }
    if(!Texture)
    {
        UE_LOG(LogRHI, Error, TEXT("Trying to read from an uninitialised texture."));
        return;
    }

	// function wants details about the destination, not the source
	if(Surface->PixelFormat == PF_B8G8R8A8)
	{
		check(sizeof(FColor) == GPixelFormats[PF_B8G8R8A8].BlockBytes);
		const uint32 Stride = sizeof(FColor) * SizeX;
		const uint32 BytesPerImage = Stride  * SizeY;
#if PLATFORM_MAC // @todo zebra
		if(Texture.storageMode == MTLStorageModeManaged)
		{
			SCOPE_CYCLE_COUNTER(STAT_MetalTexturePageOffTime);
			
			// Synchronise the texture with the CPU
			id<MTLBlitCommandEncoder> Blitter = Context->GetBlitContext();
			[Blitter synchronizeTexture:Texture slice:0 level:0];
			
			//kick the current command buffer.
			Context->SubmitCommandBufferAndWait();
			
			[Texture getBytes:OutDataPtr bytesPerRow:Stride bytesPerImage:BytesPerImage fromRegion:Region mipmapLevel:0 slice:0];
		}
		else
#endif
		{
			FMetalPooledBuffer Buffer = Context->CreatePooledBuffer(FMetalPooledBufferArgs(Context->GetDevice(), BytesPerImage, MTLStorageModeShared));
			{
				// Synchronise the texture with the CPU
				SCOPE_CYCLE_COUNTER(STAT_MetalTexturePageOffTime);
				
				id<MTLBlitCommandEncoder> Blitter = Context->GetBlitContext();
				[Blitter copyFromTexture:Texture sourceSlice:0 sourceLevel:0 sourceOrigin:Region.origin sourceSize:Region.size toBuffer:Buffer.Buffer destinationOffset:0 destinationBytesPerRow:Stride destinationBytesPerImage:BytesPerImage];
				
				//kick the current command buffer.
				Context->SubmitCommandBufferAndWait();
				
				FMemory::Memcpy(OutDataPtr, [Buffer.Buffer contents], BytesPerImage);
			}
			Context->ReleasePooledBuffer(Buffer);
		}
	}
	else if(Surface->PixelFormat == PF_FloatRGBA)
	{
		bool bLinearToGamma = InFlags.GetLinearToGamma();
		
		// Determine minimal and maximal float value present in received data. Treat alpha separately.
        // Metal uses 16-bit float for FloatRGBA, not 32

		const uint32 Stride = GPixelFormats[Surface->PixelFormat].BlockBytes * SizeX;
		const uint32 BytesPerImage = Stride  * SizeY;
		int32 PixelComponentCount = 4 * SizeX * SizeY;
		int32 FloatBGRADataSize = Stride * SizeY;
		FMetalPooledBuffer Buffer = Context->CreatePooledBuffer(FMetalPooledBufferArgs(Context->GetDevice(), FloatBGRADataSize, MTLStorageModeShared));
		{
			// Synchronise the texture with the CPU
			SCOPE_CYCLE_COUNTER(STAT_MetalTexturePageOffTime);
			
			id<MTLBlitCommandEncoder> Blitter = Context->GetBlitContext();
			[Blitter copyFromTexture:Texture sourceSlice:0 sourceLevel:0 sourceOrigin:Region.origin sourceSize:Region.size toBuffer:Buffer.Buffer destinationOffset:0 destinationBytesPerRow:Stride destinationBytesPerImage:BytesPerImage];
			
			//kick the current command buffer.
			Context->SubmitCommandBufferAndWait();
		}
		
		FFloat16* FloatBGRAData = (FFloat16*)[Buffer.Buffer contents];

		// Determine minimal and maximal float values present in received data. Treat each component separately.
		float MinValue[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		float MaxValue[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		FFloat16* DataPtr = FloatBGRAData;
		for( int32 PixelComponentIndex = 0; PixelComponentIndex < PixelComponentCount; ++PixelComponentIndex, ++DataPtr )
		{
			int32 ComponentIndex = PixelComponentIndex % 4;
			MinValue[ComponentIndex] = FMath::Min<float>(*DataPtr,MinValue[ComponentIndex]);
			MaxValue[ComponentIndex] = FMath::Max<float>(*DataPtr,MaxValue[ComponentIndex]);
		}
		
		// Convert the data into BGRA8 buffer
		DataPtr = FloatBGRAData;
		float RescaleFactor[4] = { MaxValue[0] - MinValue[0], MaxValue[1] - MinValue[1], MaxValue[2] - MinValue[2], MaxValue[3] - MinValue[3] };
		for( int32 PixelIndex = 0; PixelIndex < PixelComponentCount / 4; ++PixelIndex )
		{
			float B = (DataPtr[2] - MinValue[2]) / RescaleFactor[2];
			float G = (DataPtr[1] - MinValue[1]) / RescaleFactor[1];
			float R = (DataPtr[0] - MinValue[0]) / RescaleFactor[0];
			float A = (DataPtr[3] - MinValue[3]) / RescaleFactor[3];
			
			FColor NormalizedColor = FLinearColor( R,G,B,A ).ToFColor(bLinearToGamma);
			FMemory::Memcpy(OutDataPtr,&NormalizedColor,sizeof(FColor));
			DataPtr += 4;
			OutDataPtr++;
		}
		Context->ReleasePooledBuffer(Buffer);
	}
	else
	{
		NOT_SUPPORTED("RHIReadSurfaceData Format");
	}
}

void FMetalDynamicRHI::RHIMapStagingSurface(FTextureRHIParamRef TextureRHI,void*& OutData,int32& OutWidth,int32& OutHeight)
{
    FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);
    FMetalTexture2D* Texture = (FMetalTexture2D*)TextureRHI->GetTexture2D();
    
    uint32 Stride = 0;
    OutWidth = Texture->GetSizeX();
    OutHeight = Texture->GetSizeY();
    OutData = Surface->Lock(0, 0, RLM_ReadOnly, Stride);
}

void FMetalDynamicRHI::RHIUnmapStagingSurface(FTextureRHIParamRef TextureRHI)
{
    FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);
    
    Surface->Unlock(0, 0);
}

void FMetalDynamicRHI::RHIReadSurfaceFloatData(FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<FFloat16Color>& OutData, ECubeFace CubeFace,int32 ArrayIndex,int32 MipIndex)
{
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);
	
    id<MTLTexture> Texture = Surface->Texture;
    if(!Texture && (Surface->Flags & TexCreate_Presentable))
    {
        id<CAMetalDrawable> Drawable = (id<CAMetalDrawable>)GetMetalDeviceContext().GetCurrentState().GetCurrentDrawable();
        Texture = Drawable ? Drawable.texture : nil;
    }
    if(!Texture)
    {
        UE_LOG(LogRHI, Error, TEXT("Trying to read from an uninitialised texture."));
        return;
    }
    
	// verify the input image format (but don't crash)
	if (Surface->PixelFormat != PF_FloatRGBA)
	{
		UE_LOG(LogRHI, Log, TEXT("Trying to read non-FloatRGBA surface."));
	}

	if (TextureRHI->GetTextureCube())
	{
		// adjust index to account for cubemaps as texture arrays
		ArrayIndex *= CubeFace_MAX;
		ArrayIndex += GetMetalCubeFace(CubeFace);
	}
	
	// allocate output space
	const uint32 SizeX = Rect.Width();
	const uint32 SizeY = Rect.Height();
	OutData.Empty();
	OutData.AddUninitialized(SizeX * SizeY);
	
	MTLRegion Region = MTLRegionMake2D(Rect.Min.X, Rect.Min.Y, SizeX, SizeY);
	
	// function wants details about the destination, not the source
	const uint32 Stride = GPixelFormats[Surface->PixelFormat].BlockBytes * SizeX;
	const uint32 BytesPerImage = Stride  * SizeY;
	int32 FloatBGRADataSize = Stride * SizeY;
	FMetalPooledBuffer Buffer = Context->CreatePooledBuffer(FMetalPooledBufferArgs(Context->GetDevice(), FloatBGRADataSize, MTLStorageModeShared));
	{
		// Synchronise the texture with the CPU
		SCOPE_CYCLE_COUNTER(STAT_MetalTexturePageOffTime);
		
		id<MTLBlitCommandEncoder> Blitter = Context->GetBlitContext();
		[Blitter copyFromTexture:Texture sourceSlice:ArrayIndex sourceLevel:MipIndex sourceOrigin:Region.origin sourceSize:Region.size toBuffer:Buffer.Buffer destinationOffset:0 destinationBytesPerRow:Stride destinationBytesPerImage:BytesPerImage];
		
		//kick the current command buffer.
		Context->SubmitCommandBufferAndWait();
	}
	
	FFloat16Color* FloatBGRAData = (FFloat16Color*)[Buffer.Buffer contents];
	FFloat16Color* OutDataPtr = OutData.GetData();
	FMemory::Memcpy(OutDataPtr, FloatBGRAData, FloatBGRADataSize);
	
	Context->ReleasePooledBuffer(Buffer);
}

void FMetalDynamicRHI::RHIRead3DSurfaceFloatData(FTextureRHIParamRef TextureRHI,FIntRect InRect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData)
{
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);
	
	id<MTLTexture> Texture = Surface->Texture;
	if(!Texture)
	{
		UE_LOG(LogRHI, Error, TEXT("Trying to read from an uninitialised texture."));
		return;
	}
	
	// verify the input image format (but don't crash)
	if (Surface->PixelFormat != PF_FloatRGBA)
	{
		UE_LOG(LogRHI, Log, TEXT("Trying to read non-FloatRGBA surface."));
	}
	
	// allocate output space
	const uint32 SizeX = InRect.Width();
	const uint32 SizeY = InRect.Height();
	const uint32 SizeZ = ZMinMax.Y - ZMinMax.X;
	OutData.Empty();
	OutData.AddUninitialized(SizeX * SizeY * SizeZ);
	
	MTLRegion Region = MTLRegionMake3D(InRect.Min.X, InRect.Min.Y, ZMinMax.X, SizeX, SizeY, SizeZ);
	
	// function wants details about the destination, not the source
	const uint32 Stride = GPixelFormats[Surface->PixelFormat].BlockBytes * SizeX;
	const uint32 BytesPerImage = Stride  * SizeY;
	int32 FloatBGRADataSize = Stride * SizeY * SizeZ;
	FMetalPooledBuffer Buffer = Context->CreatePooledBuffer(FMetalPooledBufferArgs(Context->GetDevice(), FloatBGRADataSize, MTLStorageModeShared));
	{
		// Synchronise the texture with the CPU
		SCOPE_CYCLE_COUNTER(STAT_MetalTexturePageOffTime);
		
		id<MTLBlitCommandEncoder> Blitter = Context->GetBlitContext();
		[Blitter copyFromTexture:Texture sourceSlice:0 sourceLevel:0 sourceOrigin:Region.origin sourceSize:Region.size toBuffer:Buffer.Buffer destinationOffset:0 destinationBytesPerRow:Stride destinationBytesPerImage:BytesPerImage];
		
		//kick the current command buffer.
		Context->SubmitCommandBufferAndWait();
	}
	
	FFloat16Color* FloatBGRAData = (FFloat16Color*)[Buffer.Buffer contents];
	FFloat16Color* OutDataPtr = OutData.GetData();
	FMemory::Memcpy(OutDataPtr, FloatBGRAData, FloatBGRADataSize);
	
	Context->ReleasePooledBuffer(Buffer);
}
