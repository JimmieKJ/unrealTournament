// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MediaAssetsPrivatePCH.h"
#include "DeviceProfiles/DeviceProfile.h"


/* FMediaTextureResource structors
 *****************************************************************************/

FMediaTextureResource::FMediaTextureResource( const class UMediaTexture* InOwner, const FMediaSampleBufferRef& InVideoBuffer )
	: Cleared(false)
	, LastFrameTime(FTimespan::MinValue())
	, Owner(InOwner)
	, VideoBuffer(InVideoBuffer)
{ }


/* FTextureResource overrides
 *****************************************************************************/

void FMediaTextureResource::InitDynamicRHI()
{
	if ((Owner->GetSurfaceWidth() > 0) && (Owner->GetSurfaceHeight() > 0))
	{
		// Create the RHI texture. Only one mip is used and the texture is targetable or resolve.
		uint32 TexCreateFlags = Owner->SRGB ? TexCreate_SRGB : 0;
		TexCreateFlags |= TexCreate_Dynamic | TexCreate_NoTiling;
		FRHIResourceCreateInfo CreateInfo;

		RHICreateTargetableShaderResource2D(
			Owner->GetSurfaceWidth(),
			Owner->GetSurfaceHeight(),
			Owner->GetFormat(),
			1,
			TexCreateFlags,
			TexCreate_RenderTargetable,
			false,
			CreateInfo,
			RenderTargetTextureRHI,
			Texture2DRHI
		);

		TextureRHI = (FTextureRHIRef&)Texture2DRHI;
		RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, TextureRHI);

		// add to the list of global deferred updates (updated during scene rendering)
		// since the latest decoded movie frame is rendered to this media texture target
		AddToDeferredUpdateList(false);
	}

	// Create the sampler state RHI resource.
	FSamplerStateInitializerRHI SamplerStateInitializer(
		(ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter(Owner),
		Owner->AddressX == TA_Wrap ? AM_Wrap : (Owner->AddressX == TA_Clamp ? AM_Clamp : AM_Mirror),
		Owner->AddressY == TA_Wrap ? AM_Wrap : (Owner->AddressY == TA_Clamp ? AM_Clamp : AM_Mirror),
		AM_Wrap
	);

	SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
}


void FMediaTextureResource::ReleaseDynamicRHI()
{
	// release the FTexture RHI resources here as well
	ReleaseRHI();

	RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, FTextureRHIParamRef());
	Texture2DRHI.SafeRelease();
	RenderTargetTextureRHI.SafeRelease();

	// remove from global list of deferred updates
	RemoveFromDeferredUpdateList();
}


/* FRenderTarget overrides
 *****************************************************************************/

FIntPoint FMediaTextureResource::GetSizeXY() const
{
	return Owner->GetDimensions();
}


/* FDeferredUpdateResource overrides
 *****************************************************************************/

void FMediaTextureResource::UpdateDeferredResource(FRHICommandListImmediate& RHICmdList, bool bClearRenderTarget/*=true*/)
{
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> CurrentFrame;
	TSharedPtr<IMediaPlayer> Player = Owner->GetPlayer();

	if (Player.IsValid() && (Player->IsPlaying() || Player->IsPaused()))
	{
		CurrentFrame = VideoBuffer->GetCurrentSample();
	}

	if (CurrentFrame.IsValid())
	{
		FTimespan CurrentFrameTime = VideoBuffer->GetCurrentSampleTime();

		// draw the latest video frame
		if (CurrentFrameTime != LastFrameTime)
		{
			uint32 Stride = 0;
			FRHITexture2D* Texture2D = TextureRHI->GetTexture2D();
			uint8* TextureBuffer = (uint8*)RHILockTexture2D(Texture2D, 0, RLM_WriteOnly, Stride, false);

			FMemory::Memcpy(TextureBuffer, CurrentFrame->GetData(), CurrentFrame->Num());
			RHIUnlockTexture2D(Texture2D, 0, false);

			LastFrameTime = CurrentFrameTime;
			Cleared = false;
		}
	}
	else if (!Cleared || (LastClearColor != Owner->ClearColor))
	{
		// clear texture if video track selected
		FRHICommandListImmediate& CommandList = FRHICommandListExecutor::GetImmediateCommandList();

 		SetRenderTarget(CommandList, RenderTargetTextureRHI, FTextureRHIRef());
 		CommandList.SetViewport(0, 0, 0.0f, Owner->GetSurfaceWidth(), Owner->GetSurfaceHeight(), 1.0f);
 		CommandList.Clear(true, Owner->ClearColor, false, 0.f, false, 0, FIntRect());
		CommandList.CopyToResolveTarget(Texture2DRHI, TextureRHI, true, FResolveParams());

		LastClearColor = Owner->ClearColor;
		Cleared = true;
	}
}
