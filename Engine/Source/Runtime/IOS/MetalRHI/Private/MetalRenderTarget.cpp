// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRenderTarget.cpp: Metal render target implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "ScreenRendering.h"


void FMetalDynamicRHI::RHICopyToResolveTarget(FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI, bool bKeepOriginalSurface, const FResolveParams& ResolveParams)
{

}

void FMetalDynamicRHI::RHIReadSurfaceData(FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags)
{

}

void FMetalDynamicRHI::RHIMapStagingSurface(FTextureRHIParamRef TextureRHI,void*& OutData,int32& OutWidth,int32& OutHeight)
{

}

void FMetalDynamicRHI::RHIUnmapStagingSurface(FTextureRHIParamRef TextureRHI)
{

}

void FMetalDynamicRHI::RHIReadSurfaceFloatData(FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<FFloat16Color>& OutData, ECubeFace CubeFace,int32 ArrayIndex,int32 MipIndex)
{
	//kick the current command buffer.
	FMetalManager::Get()->SubmitCommandBufferAndWait();

	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(TextureRHI);
	
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
	const uint32 Stride = 0; //can set 0 to indicate tightly packed.
	const uint32 BytesPerImage = 0;
	OutData.Empty();
	OutData.AddUninitialized(SizeX * SizeY);
	
	FFloat16Color* OutDataPtr = OutData.GetData();
	MTLRegion Region = MTLRegionMake2D(Rect.Min.X, Rect.Min.Y, Rect.Max.X, Rect.Max.Y);
	
	// function wants details about the destination, not the source
	[Surface->Texture getBytes: OutDataPtr bytesPerRow:Stride bytesPerImage:BytesPerImage fromRegion:Region mipmapLevel:MipIndex slice:ArrayIndex];
}

void FMetalDynamicRHI::RHIRead3DSurfaceFloatData(FTextureRHIParamRef TextureRHI,FIntRect InRect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData)
{

}
