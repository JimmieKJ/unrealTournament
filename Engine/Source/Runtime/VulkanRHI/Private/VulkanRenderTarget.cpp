// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanRenderTarget.cpp: Vulkan render target implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "ScreenRendering.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"

static int32 GSubmitOnCopyToResolve = 0;
static FAutoConsoleVariableRef CVarVulkanSubmitOnCopyToResolve(
	TEXT("r.Vulkan.SubmitOnCopyToResolve"),
	GSubmitOnCopyToResolve,
	TEXT("Submits the Queue to the GPU on every RHICopyToResolveTarget call.\n")
	TEXT(" 0: Do not submit (default)\n")
	TEXT(" 1: Submit"),
	ECVF_Default
	);

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

void FVulkanCommandListContext::RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	//FRCLog::Printf(FString::Printf(TEXT("RHISetRenderTargetsAndClear\n")));
	PendingState->SetRenderTargetsInfo(RenderTargetsInfo);

#if 0//VULKAN_USE_NEW_RESOURCE_MANAGEMENT
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (CmdBuffer->IsInsideRenderPass())
	{
		if (
			(RenderTargetsInfo.NumColorRenderTargets == 0 || (RenderTargetsInfo.NumColorRenderTargets == 1 && !RenderTargetsInfo.ColorRenderTarget[0].Texture)) &&
			!RenderTargetsInfo.DepthStencilRenderTarget.Texture)
		{
			PendingState->RenderPassEnd(CmdBuffer);
			PendingState->PrevRenderTargetsInfo = FRHISetRenderTargetsInfo();
		}
	}
#endif
}

void FVulkanCommandListContext::RHICopyToResolveTarget(FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI, bool bKeepOriginalSurface, const FResolveParams& ResolveParams)
{
	//FRCLog::Printf(FString::Printf(TEXT("RHICopyToResolveTarget\n")));
#if 1//VULKAN_USE_NEW_COMMAND_BUFFERS
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	// Verify if we need to do some work (for the case of SetRT(), CopyToResolve() with no draw calls in between)
	PendingState->UpdateRenderPass(CmdBuffer);

	const bool bRenderPassIsActive = PendingState->IsRenderPassActive();

	if (bRenderPassIsActive)
	{
		PendingState->RenderPassEnd(CmdBuffer);
	}

	check(!SourceTextureRHI || SourceTextureRHI->GetNumSamples() < 2);

	FVulkanFramebuffer* Framebuffer = PendingState->GetFrameBuffer();
	if (Framebuffer)
	{
		Framebuffer->InsertWriteBarriers(GetCommandBufferManager()->GetActiveCmdBuffer());
	}

	if (GSubmitOnCopyToResolve)
	{
		check(0);
#if 0
		CmdBuffer->End();

		FVulkanSemaphore* BackBufferAcquiredSemaphore = Framebuffer->GetBackBuffer() ? Framebuffer->GetBackBuffer()->AcquiredSemaphore : nullptr;

		Device->GetQueue()->Submit(CmdBuffer, BackBufferAcquiredSemaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, nullptr);
		// No need to acquire it anymore
		Framebuffer->GetBackBuffer()->AcquiredSemaphore = nullptr;
		CommandBufferManager->PrepareForNewActiveCommandBuffer();
#endif
	}
#else
	// If we're using the pResolveAttachments property of the subpass, so we don't need manual command buffer resolves and this function is a NoOp.
#if !VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
	if (SourceTextureRHI->GetNumSamples() < 2)
	{
		return;
	}

	const bool bRenderPassIsActive = PendingState->IsRenderPassActive();

	if (bRenderPassIsActive)
	{
		PendingState->RenderPassEnd();
	}

	VulkanResolveImage(PendingState->GetCommandBuffer(), SourceTextureRHI, DestTextureRHI);

	if (bRenderPassIsActive)
	{
		PendingState->RenderPassBegin();
	}
#endif
#endif
}

void FVulkanDynamicRHI::RHIReadSurfaceData(FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags)
{
	FRHITexture2D* TextureRHI2D = TextureRHI->GetTexture2D();
	check(TextureRHI2D);
	FVulkanTexture2D* Texture2D = (FVulkanTexture2D*)TextureRHI2D;
	uint32 NumPixels = TextureRHI2D->GetSizeX() * TextureRHI2D->GetSizeY();
	FVulkanCmdBuffer* CmdBuffer = Device->GetImmediateContext().GetCommandBufferManager()->GetUploadCmdBuffer();

	ensure(Texture2D->Surface.InternalFormat == VK_FORMAT_R8G8B8A8_UNORM || Texture2D->Surface.InternalFormat == VK_FORMAT_B8G8R8A8_UNORM);
	VulkanRHI::FStagingBuffer* StagingBuffer = Device->GetStagingManager().AcquireBuffer(NumPixels * sizeof(FColor), VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
	VkBufferImageCopy CopyRegion;
	FMemory::Memzero(CopyRegion);
	//Region.bufferOffset = 0;
	CopyRegion.bufferRowLength = TextureRHI2D->GetSizeX();
	CopyRegion.bufferImageHeight = TextureRHI2D->GetSizeY();
	CopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//Region.imageSubresource.mipLevel = 0;
	//Region.imageSubresource.baseArrayLayer = 0;
	CopyRegion.imageSubresource.layerCount = 1;
	CopyRegion.imageExtent.width = TextureRHI2D->GetSizeX();
	CopyRegion.imageExtent.height = TextureRHI2D->GetSizeY();
	CopyRegion.imageExtent.depth = 1;
	VulkanRHI::vkCmdCopyImageToBuffer(CmdBuffer->GetHandle(), Texture2D->Surface.Image, VK_IMAGE_LAYOUT_GENERAL, StagingBuffer->GetHandle(), 1, &CopyRegion);
	// Force upload
	Device->GetImmediateContext().GetCommandBufferManager()->SubmitUploadCmdBuffer(true);
	Device->WaitUntilIdle();

/*
	VkMappedMemoryRange MemRange;
	FMemory::Memzero(MemRange);
	VulkanRHI::vkInvalidateMappedMemoryRanges(Device->GetInstanceHandle(), 1, &MemRange);
*/

	OutData.SetNum(NumPixels);
	FColor* Dest = OutData.GetData();
	for (int32 Row = Rect.Min.Y; Row < Rect.Max.Y; ++Row)
	{
		FColor* Src = (FColor*)StagingBuffer->GetMappedPointer() + Row * TextureRHI2D->GetSizeX() + Rect.Min.X;
		for (int32 Col = Rect.Min.X; Col < Rect.Max.X; ++Col)
		{
			*Dest++ = *Src++;
		}
	}
	Device->GetStagingManager().ReleaseBuffer(CmdBuffer, StagingBuffer);
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

	//#todo-rco: Properly fill in
	for(uint32 Index = 0; Index < (Surface.Width >> MipIndex) * (Surface.Height >> MipIndex); Index++)
	{
		OutData.Add(FFloat16Color(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)));
	}
}

void FVulkanDynamicRHI::RHIRead3DSurfaceFloatData(FTextureRHIParamRef TextureRHI,FIntRect InRect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteComputeFence)
{
	IRHICommandContext::RHITransitionResources(TransitionType, TransitionPipeline, InUAVs, NumUAVs, WriteComputeFence);
	ensure(NumUAVs == 0);
}

void FVulkanCommandListContext::RHITransitionResources(EResourceTransitionAccess TransitionType, FTextureRHIParamRef* InTextures, int32 NumTextures)
{
	if (TransitionType == EResourceTransitionAccess::EReadable)
	{
		const FResolveParams ResolveParams;
		for (int32 i = 0; i < NumTextures; ++i)
		{
			RHICopyToResolveTarget(InTextures[i], InTextures[i], true, ResolveParams);
		}
	}
	else if (TransitionType == EResourceTransitionAccess::EWritable)
	{
		for (int32 i = 0; i < NumTextures; ++i)
		{
			FRHITexture* RHITexture = InTextures[i];
			FRHITexture2D* RHITexture2D = RHITexture->GetTexture2D();
			if (RHITexture2D)
			{
				//FVulkanTexture2D* Texture = (FVulkanTexture2D*)RHITexture2D;
				//check(Texture->Surface.GetAspectMask() == VK_IMAGE_ASPECT_COLOR_BIT);
				//VulkanSetImageLayoutSimple(PendingState->GetCommandBuffer(), Texture->Surface.Image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			}
			else
			{
				FRHITextureCube* RHITextureCube = RHITexture->GetTextureCube();
				if (RHITextureCube)
				{
				}
				else
				{
					FRHITexture3D* RHITexture3D = RHITexture->GetTexture3D();
					if (RHITexture3D)
					{
					}
					else
					{
						ensure(0);
					}
				}
			}
		}
	}
	else if (TransitionType == EResourceTransitionAccess::ERWSubResBarrier)
	{
	}
	else
	{
		ensure(0);
	}
}

// Need a separate struct so we can memzero/remove dependencies on reference counts
struct FRenderTargetLayoutHashableStruct
{
	// Depth goes in the last slot
	FRHITexture* Texture[MaxSimultaneousRenderTargets + 1];
	uint32 MipIndex[MaxSimultaneousRenderTargets];
	uint32 ArraySliceIndex[MaxSimultaneousRenderTargets];
	ERenderTargetLoadAction LoadAction[MaxSimultaneousRenderTargets];
	ERenderTargetStoreAction StoreAction[MaxSimultaneousRenderTargets];

	ERenderTargetLoadAction		DepthLoadAction;
	ERenderTargetStoreAction	DepthStoreAction;
	ERenderTargetLoadAction		StencilLoadAction;
	ERenderTargetStoreAction	StencilStoreAction;
	FExclusiveDepthStencil		DepthStencilAccess;

	// Fill this outside Set() function
	VkExtent2D					Extents;

	bool bClearDepth;
	bool bClearStencil;
	bool bClearColor;

	void Set(const FRHISetRenderTargetsInfo& RTInfo)
	{
		FMemory::Memzero(*this);
		for (int32 Index = 0; Index < RTInfo.NumColorRenderTargets; ++Index)
		{
			Texture[Index] = RTInfo.ColorRenderTarget[Index].Texture;
			MipIndex[Index] = RTInfo.ColorRenderTarget[Index].MipIndex;
			ArraySliceIndex[Index] = RTInfo.ColorRenderTarget[Index].ArraySliceIndex;
			LoadAction[Index] = RTInfo.ColorRenderTarget[Index].LoadAction;
			StoreAction[Index] = RTInfo.ColorRenderTarget[Index].StoreAction;
		}

		Texture[MaxSimultaneousRenderTargets] = RTInfo.DepthStencilRenderTarget.Texture;
		DepthLoadAction = RTInfo.DepthStencilRenderTarget.DepthLoadAction;
		DepthStoreAction = RTInfo.DepthStencilRenderTarget.DepthStoreAction;
		StencilLoadAction = RTInfo.DepthStencilRenderTarget.StencilLoadAction;
		StencilStoreAction = RTInfo.DepthStencilRenderTarget.GetStencilStoreAction();
		DepthStencilAccess = RTInfo.DepthStencilRenderTarget.GetDepthStencilAccess();

		bClearDepth = RTInfo.bClearDepth;
		bClearStencil = RTInfo.bClearStencil;
		bClearColor = RTInfo.bClearColor;
	}
};

FVulkanRenderTargetLayout::FVulkanRenderTargetLayout(const FRHISetRenderTargetsInfo& RTInfo)
	: NumAttachments(0)
	, NumColorAttachments(0)
	, bHasDepthStencil(false)
	, bHasResolveAttachments(false)
	, Hash(0)
{
	FMemory::Memzero(ColorReferences);
	FMemory::Memzero(ResolveReferences);
	FMemory::Memzero(DepthStencilReference);
	FMemory::Memzero(Desc);
	FMemory::Memzero(Extent);

	bool bSetExtent = false;
		
	for (int32 Index = 0; Index < RTInfo.NumColorRenderTargets; ++Index)
	{
		const FRHIRenderTargetView& RTView = RTInfo.ColorRenderTarget[Index];
		if (RTView.Texture)
		{
		    FVulkanTextureBase* Texture = FVulkanTextureBase::Cast(RTView.Texture);
		    check(Texture);
    
		    if (!bSetExtent)
		    {
			    bSetExtent = true;
			    Extent.Extent3D.width = Texture->Surface.Width >> RTView.MipIndex;
			    Extent.Extent3D.height = Texture->Surface.Height >> RTView.MipIndex;
			    Extent.Extent3D.depth = 1;
		    }
    
		    uint32 NumSamples = RTView.Texture->GetNumSamples();
	    
		    VkAttachmentDescription& CurrDesc = Desc[NumAttachments];
		    
		    //@TODO: Check this, it should be a power-of-two. Might be a VulkanConvert helper function.
		    CurrDesc.samples = static_cast<VkSampleCountFlagBits>(NumSamples);
		    CurrDesc.format = UEToVkFormat(RTView.Texture->GetFormat(), (Texture->Surface.UEFlags & TexCreate_SRGB) == TexCreate_SRGB);
		    CurrDesc.loadOp = RenderTargetLoadActionToVulkan(RTView.LoadAction);
		    CurrDesc.storeOp = RenderTargetStoreActionToVulkan(RTView.StoreAction);
		    CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#if !VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
		    CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
#endif
		    CurrDesc.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
		    CurrDesc.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    
		    ColorReferences[NumColorAttachments].attachment = NumAttachments;
		    ColorReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_GENERAL;
		    NumAttachments++;
    
#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
			if (NumSamples == 1)
			{
			    CurrDesc.storeOp = RenderTargetStoreActionToVulkan(RTView.StoreAction);
    
			    ResolveReferences[NumColorAttachments].attachment = VK_ATTACHMENT_UNUSED;
			    ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_UNDEFINED;
		    }
			else
		    {
			    // discard MSAA color target after resolve to 1x surface
			    CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    
			    // Resolve attachment
			    VkAttachmentDescription& ResolveDesc = Desc[NumAttachments];
			    ResolveDesc.samples = VK_SAMPLE_COUNT_1_BIT;
			    ResolveDesc.format = (VkFormat)GPixelFormats[RTView.Texture->GetFormat()].PlatformFormat;
			    ResolveDesc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			    ResolveDesc.storeOp = RenderTargetStoreActionToVulkan(RTView.StoreAction);
			    ResolveDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			    ResolveDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			    ResolveDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			    ResolveDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
			    ResolveReferences[NumColorAttachments].attachment = NumAttachments;
			    ResolveReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			    NumAttachments++;
			    bHasResolveAttachments = true;
		    }
#endif
	    
		    NumColorAttachments++;
		}
	}

	if (RTInfo.DepthStencilRenderTarget.Texture)
	{
		VkAttachmentDescription& CurrDesc = Desc[NumAttachments];
		FMemory::Memzero(CurrDesc);
		FVulkanTextureBase* Texture = FVulkanTextureBase::Cast(RTInfo.DepthStencilRenderTarget.Texture);
		check(Texture);

		//@TODO: Check this, it should be a power-of-two. Might be a VulkanConvert helper function.
		CurrDesc.samples = static_cast<VkSampleCountFlagBits>(RTInfo.DepthStencilRenderTarget.Texture->GetNumSamples());
		CurrDesc.format = UEToVkFormat(RTInfo.DepthStencilRenderTarget.Texture->GetFormat(), false);
		CurrDesc.loadOp = RenderTargetLoadActionToVulkan(RTInfo.DepthStencilRenderTarget.DepthLoadAction);
		CurrDesc.stencilLoadOp = RenderTargetLoadActionToVulkan(RTInfo.DepthStencilRenderTarget.StencilLoadAction);
		if (CurrDesc.samples == VK_SAMPLE_COUNT_1_BIT)
		{
			CurrDesc.storeOp = RenderTargetStoreActionToVulkan(RTInfo.DepthStencilRenderTarget.DepthStoreAction);
			CurrDesc.stencilStoreOp = RenderTargetStoreActionToVulkan(RTInfo.DepthStencilRenderTarget.GetStencilStoreAction());
		}
		else
		{
			// Never want to store MSAA depth/stencil
			CurrDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}
		CurrDesc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		CurrDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		DepthStencilReference.attachment = NumAttachments;
		DepthStencilReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		bHasDepthStencil = true;
		NumAttachments++;

		if (!bSetExtent)
		{
			bSetExtent = true;
			Extent.Extent3D.width = Texture->Surface.Width;
			Extent.Extent3D.height = Texture->Surface.Height;
			Extent.Extent3D.depth = 1;
		}
	}

	check(bSetExtent);

	FRenderTargetLayoutHashableStruct RTHash;
	FMemory::Memzero(RTHash);
	RTHash.Set(RTInfo);
	RTHash.Extents = Extent.Extent2D;
	Hash = FCrc::MemCrc32(&RTHash, sizeof(RTHash));
}
