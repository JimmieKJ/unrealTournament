// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRendererPrivatePCH.h"

FSlateFontTextureRHI::FSlateFontTextureRHI( uint32 InWidth, uint32 InHeight )
	: Width( InWidth )
	, Height( InHeight )
{
}

void FSlateFontTextureRHI::InitDynamicRHI()
{
	check( IsInRenderingThread() );

	// Create the texture
	if( Width > 0 && Height > 0 )
	{
		check( !IsValidRef( ShaderResource) );
		FRHIResourceCreateInfo CreateInfo;
		ShaderResource = RHICreateTexture2D( Width, Height, PF_A8, 1, 1, TexCreate_Dynamic, CreateInfo );
		check( IsValidRef( ShaderResource ) );

		// Also assign the reference to the FTextureResource variable so that the Engine can access it
		TextureRHI = ShaderResource;

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer
		(
		  SF_Bilinear,
		  AM_Clamp,
		  AM_Clamp,
		  AM_Wrap,
		  0,
		  1, // Disable anisotropic filtering, since aniso doesn't respect MaxLOD
		  0,
		  0
		);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);

		// Create a custom sampler state for using this texture in a deferred pass, where ddx / ddy are discontinuous
		FSamplerStateInitializerRHI DeferredPassSamplerStateInitializer
		(
		  SF_Bilinear,
		  AM_Clamp,
		  AM_Clamp,
		  AM_Wrap,
		  0,
		  1, // Disable anisotropic filtering, since aniso doesn't respect MaxLOD
		  0,
		  0
		);
		DeferredPassSamplerStateRHI = RHICreateSamplerState(DeferredPassSamplerStateInitializer);

		INC_MEMORY_STAT_BY(STAT_SlateTextureGPUMemory, Width*Height*GPixelFormats[PF_A8].BlockBytes);
	}

	// Restore previous data if it exists
	if( TempData.Num() > 0 )
	{
		// Set texture data back to the previous state
		uint32 Stride;
		uint8* TextureData = (uint8*)RHILockTexture2D( GetTypedResource(), 0, RLM_WriteOnly, Stride, false );
		FMemory::Memcpy( TextureData, TempData.GetData(), GPixelFormats[PF_A8].BlockBytes*Width*Height );
		RHIUnlockTexture2D( GetTypedResource(), 0, false );

		TempData.Empty();
	}
}

void FSlateFontTextureRHI::ReleaseDynamicRHI()
{
	check( IsInRenderingThread() );

	// Copy the data to temporary storage until InitDynamicRHI is called
	uint32 Stride;
	TempData.Empty();
	TempData.AddUninitialized( GPixelFormats[PF_A8].BlockBytes*Width*Height );

	uint8* TextureData = (uint8*)RHILockTexture2D( GetTypedResource(), 0, RLM_ReadOnly, Stride, false );
	FMemory::Memcpy( TempData.GetData(), TextureData, GPixelFormats[PF_A8].BlockBytes*Width*Height );
	RHIUnlockTexture2D( GetTypedResource(), 0, false );

	// Release the texture
	if( IsValidRef(ShaderResource) )
	{
		DEC_MEMORY_STAT_BY(STAT_SlateTextureGPUMemory, Width*Height*GPixelFormats[PF_A8].BlockBytes);
	}

	ShaderResource.SafeRelease();
}

FSlateFontAtlasRHI::FSlateFontAtlasRHI( uint32 Width, uint32 Height )
	: FSlateFontAtlas( Width, Height ) 
	, FontTexture( new FSlateFontTextureRHI( Width, Height ) )
{
}

FSlateFontAtlasRHI::~FSlateFontAtlasRHI()
{
}

void FSlateFontAtlasRHI::ReleaseResources()
{
	check( IsThreadSafeForSlateRendering() );

	BeginReleaseResource( FontTexture.Get() );
}

void FSlateFontAtlasRHI::ConditionalUpdateTexture()
{
	if( bNeedsUpdate )
	{
		if (IsInRenderingThread())
		{
			FontTexture->InitResource();

			uint32 DestStride;
			uint8* TempData = (uint8*)RHILockTexture2D( FontTexture->GetTypedResource(), 0, RLM_WriteOnly, /*out*/ DestStride, false );
			// check( DestStride == Atlas.BytesPerPixel * Atlas.AtlasWidth ); // Temporarily disabling check
			FMemory::Memcpy( TempData, AtlasData.GetData(), BytesPerPixel*AtlasWidth*AtlasHeight );
			RHIUnlockTexture2D( FontTexture->GetTypedResource(),0,false );
		}
		else
		{
			check( IsThreadSafeForSlateRendering() );

			BeginInitResource( FontTexture.Get() );

			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER( SlateUpdateFontTextureCommand,
				FSlateFontAtlasRHI&, Atlas, *this,
			{
				uint32 DestStride;
				uint8* TempData = (uint8*)RHILockTexture2D( Atlas.FontTexture->GetTypedResource(), 0, RLM_WriteOnly, /*out*/ DestStride, false );
				// check( DestStride == Atlas.BytesPerPixel * Atlas.AtlasWidth ); // Temporarily disabling check
				FMemory::Memcpy( TempData, Atlas.AtlasData.GetData(), Atlas.BytesPerPixel*Atlas.AtlasWidth*Atlas.AtlasHeight );
				RHIUnlockTexture2D( Atlas.FontTexture->GetTypedResource(),0,false );
			});
		}

		bNeedsUpdate = false;
	}
}
