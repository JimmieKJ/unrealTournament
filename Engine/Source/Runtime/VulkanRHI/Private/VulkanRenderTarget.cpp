// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanRenderTarget.cpp: Vulkan render target implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "ScreenRendering.h"
#include "VulkanPendingState.h"

void FVulkanCommandListContext::RHISetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs)
{
	//@TODO: call SetRenderTargetsAndClear?
	FRHIDepthRenderTargetView DepthView;
	if (NewDepthStencilTarget)
	{
		DepthView = *NewDepthStencilTarget;
	}
	else
	{
		DepthView = FRHIDepthRenderTargetView(FTextureRHIParamRef(), ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::ENoAction);
	}

	FRHISetRenderTargetsInfo Info(NumSimultaneousRenderTargets, NewRenderTargets, DepthView);
	RHISetRenderTargetsAndClear(Info);

	checkf(NumUAVs == 0, TEXT("Calling SetRenderTargets with UAVs is not supported in Vulkan yet"));
}

void FVulkanDynamicRHI::RHIDiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask)
{
}

void FVulkanCommandListContext::RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	FVulkanPendingState& PendingState = Device->GetPendingState();
	PendingState.SetRenderTargetsInfo(RenderTargetsInfo);

	if (!PendingState.GetIsCommandBufferEmpty())
	{
		FVulkanFramebuffer* Framebuffer = PendingState.GetFrameBuffer();
		if (Framebuffer)
		{
			Framebuffer->InsertWriteBarrier(PendingState.GetCommandBuffer());
		}
	}
}

void FVulkanCommandListContext::RHICopyToResolveTarget(FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI, bool bKeepOriginalSurface, const FResolveParams& ResolveParams)
{
	// If we're using the pResoveSurface property of the subpass, so we don't need manual command buffer resolves and this function is a NoOp.
#if !VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	if (SourceTextureRHI->GetNumSamples() < 2)
	{
		return;
	}

	FVulkanPendingState& State = Device->GetPendingState();
	const bool bRenderPassIsActive = State.IsRenderPassActive();

	if (bRenderPassIsActive)
	{
		State.RenderPassEnd();
	}

	VulkanResolveImage(State.GetCommandBuffer(), SourceTextureRHI, DestTextureRHI);

	if (bRenderPassIsActive)
	{
		State.RenderPassBegin();
	}
#endif
}

void FVulkanDynamicRHI::RHIReadSurfaceData(FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanDynamicRHI::RHIMapStagingSurface(FTextureRHIParamRef TextureRHI,void*& OutData,int32& OutWidth,int32& OutHeight)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanDynamicRHI::RHIUnmapStagingSurface(FTextureRHIParamRef TextureRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanDynamicRHI::RHIReadSurfaceFloatData(FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<FFloat16Color>& OutData, ECubeFace CubeFace,int32 ArrayIndex,int32 MipIndex)
{
	FVulkanTextureBase* Texture = FVulkanTextureBase::Cast(TextureRHI);
	FVulkanSurface& Surface = Texture->Surface;

	// By pass for now
	#if 1
		for(uint32 Index=0; Index<Surface.Width*Surface.Height; Index++)
		{
			OutData.Add(FFloat16Color(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)));
		}
	#endif
}

void FVulkanDynamicRHI::RHIRead3DSurfaceFloatData(FTextureRHIParamRef TextureRHI,FIntRect InRect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}
