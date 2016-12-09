// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanRenderTarget.cpp: Vulkan render target implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "ScreenRendering.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "SceneUtils.h"

static int32 GSubmitOnCopyToResolve = 0;
static FAutoConsoleVariableRef CVarVulkanSubmitOnCopyToResolve(
	TEXT("r.Vulkan.SubmitOnCopyToResolve"),
	GSubmitOnCopyToResolve,
	TEXT("Submits the Queue to the GPU on every RHICopyToResolveTarget call.\n")
	TEXT(" 0: Do not submit (default)\n")
	TEXT(" 1: Submit"),
	ECVF_Default
	);

static int32 GIgnoreCPUReads = 0;
static FAutoConsoleVariableRef CVarVulkanIgnoreCPUReads(
	TEXT("r.Vulkan.IgnoreCPUReads"),
	GIgnoreCPUReads,
	TEXT("Debugging utility for GPU->CPU reads.\n")
	TEXT(" 0 will read from the GPU (default).\n")
	TEXT(" 1 will read from GPU but fill the buffer instead of copying from a texture.\n")
	TEXT(" 2 will NOT read from the GPU and fill with zeros.\n"),
	ECVF_Default
	);

void FVulkanCommandListContext::FTransitionState::Destroy(FVulkanDevice& InDevice)
{
	for (auto& Pair : RenderPasses)
	{
		delete Pair.Value;
	}
	RenderPasses.Reset();

	for (auto& Pair : Framebuffers)
	{
		FFramebufferList* List = Pair.Value;
		for (int32 Index = List->Framebuffer.Num() - 1; Index >= 0; --Index)
		{
			List->Framebuffer[Index]->Destroy(InDevice);
			delete List->Framebuffer[Index];
		}
		delete List;
	}
	Framebuffers.Reset();
}

FVulkanFramebuffer* FVulkanCommandListContext::FTransitionState::GetOrCreateFramebuffer(FVulkanDevice& InDevice, const FRHISetRenderTargetsInfo& RenderTargetsInfo, const FVulkanRenderTargetLayout& RTLayout, FVulkanRenderPass* RenderPass)
{
	uint32 RTLayoutHash = RTLayout.GetHash();

	FFramebufferList** FoundFramebufferList = Framebuffers.Find(RTLayoutHash);
	FFramebufferList* FramebufferList = nullptr;
	if (FoundFramebufferList)
	{
		FramebufferList = *FoundFramebufferList;

		for (int32 Index = 0; Index < FramebufferList->Framebuffer.Num(); ++Index)
		{
			if (FramebufferList->Framebuffer[Index]->Matches(RenderTargetsInfo))
			{
				return FramebufferList->Framebuffer[Index];
			}
		}
	}
	else
	{
		FramebufferList = new FFramebufferList;
		Framebuffers.Add(RTLayoutHash, FramebufferList);
	}

	FVulkanFramebuffer* Framebuffer = new FVulkanFramebuffer(InDevice, RenderTargetsInfo, RTLayout, *RenderPass);
	FramebufferList->Framebuffer.Add(Framebuffer);
	return Framebuffer;
}


void FVulkanCommandListContext::FTransitionState::BeginRenderPass(FVulkanCommandListContext& Context, FVulkanPipelineGraphicsKey& GfxKey, FVulkanDevice& InDevice, FVulkanCmdBuffer* CmdBuffer, const FRHISetRenderTargetsInfo& RenderTargetsInfo, const FVulkanRenderTargetLayout& RTLayout, FVulkanRenderPass* RenderPass, FVulkanFramebuffer* Framebuffer)
{
	check(!CurrentRenderPass);
	VkClearValue ClearValues[MaxSimultaneousRenderTargets + 1];
	FMemory::Memzero(ClearValues);

	int32 Index = 0;
	for (Index = 0; Index < RenderTargetsInfo.NumColorRenderTargets; ++Index)
	{
		FTextureRHIParamRef Texture = RenderTargetsInfo.ColorRenderTarget[Index].Texture;
		if (Texture)
		{
			FVulkanSurface& Surface = FVulkanTextureBase::Cast(Texture)->Surface;
			VkImage Image = Surface.Image;
			VkImageLayout* Found = CurrentLayout.Find(Image);
			if (!Found)
			{
				Found = &CurrentLayout.Add(Image, VK_IMAGE_LAYOUT_UNDEFINED);
			}

			if (*Found != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
			{
				Context.RHITransitionResources(EResourceTransitionAccess::EWritable, &Texture, 1);
			}

			const FLinearColor& ClearColor = Texture->HasClearValue() ? Texture->GetClearColor() : FLinearColor::Black;
			ClearValues[Index].color.float32[0] = ClearColor.R;
			ClearValues[Index].color.float32[1] = ClearColor.G;
			ClearValues[Index].color.float32[2] = ClearColor.B;
			ClearValues[Index].color.float32[3] = ClearColor.A;

			*Found = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			ensure(Surface.FormatKey != 255);
			SetKeyBits(GfxKey, RTFormatBitOffsets[Index], NUMBITS_RENDER_TARGET_FORMAT, Surface.FormatKey);
			SetKeyBits(GfxKey, RTLoadBitOffsets[Index], NUMBITS_LOAD_OP, (uint64)RenderTargetsInfo.ColorRenderTarget[Index].LoadAction);
			SetKeyBits(GfxKey, RTStoreBitOffsets[Index], NUMBITS_STORE_OP, (uint64)RenderTargetsInfo.ColorRenderTarget[Index].StoreAction);
		}
		else
		{
			SetKeyBits(GfxKey, RTFormatBitOffsets[Index], NUMBITS_RENDER_TARGET_FORMAT, 0);
			SetKeyBits(GfxKey, RTLoadBitOffsets[Index], NUMBITS_LOAD_OP, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
			SetKeyBits(GfxKey, RTStoreBitOffsets[Index], NUMBITS_STORE_OP, VK_ATTACHMENT_STORE_OP_DONT_CARE);
		}
	}

	for (; Index < MaxSimultaneousRenderTargets; ++Index)
	{
		SetKeyBits(GfxKey, RTFormatBitOffsets[Index], NUMBITS_RENDER_TARGET_FORMAT, 0);
		SetKeyBits(GfxKey, RTLoadBitOffsets[Index], NUMBITS_LOAD_OP, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
		SetKeyBits(GfxKey, RTStoreBitOffsets[Index], NUMBITS_STORE_OP, VK_ATTACHMENT_STORE_OP_DONT_CARE);
	}

	if (RenderTargetsInfo.DepthStencilRenderTarget.Texture)
	{
		FTextureRHIParamRef DSTexture = RenderTargetsInfo.DepthStencilRenderTarget.Texture;
		FVulkanSurface& Surface = FVulkanTextureBase::Cast(DSTexture)->Surface;
		VkImageLayout& DSLayout = CurrentLayout.FindOrAdd(Surface.Image);
		if (DSLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL || DSLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			Context.RHITransitionResources(EResourceTransitionAccess::EWritable, &DSTexture, 1);
			DSLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			ensure(DSLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		}
		if (DSTexture->HasClearValue())
		{
			float Depth = 0;
			uint32 Stencil = 0;
			DSTexture->GetDepthStencilClearValue(Depth, Stencil);
			ClearValues[RenderTargetsInfo.NumColorRenderTargets].depthStencil.depth = Depth;
			ClearValues[RenderTargetsInfo.NumColorRenderTargets].depthStencil.stencil = Stencil;
		}

		ensure(Surface.FormatKey != 255);
		SetKeyBits(GfxKey, OFFSET_DEPTH_STENCIL_FORMAT, NUMBITS_RENDER_TARGET_FORMAT, Surface.FormatKey);
		SetKeyBits(GfxKey, OFFSET_DEPTH_STENCIL_LOAD, NUMBITS_LOAD_OP, (uint64)RenderTargetsInfo.DepthStencilRenderTarget.DepthLoadAction);
		SetKeyBits(GfxKey, OFFSET_DEPTH_STENCIL_STORE, NUMBITS_STORE_OP, (uint64)RenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction);
	}
	else
	{
		SetKeyBits(GfxKey, OFFSET_DEPTH_STENCIL_FORMAT, NUMBITS_RENDER_TARGET_FORMAT, 0);
		SetKeyBits(GfxKey, OFFSET_DEPTH_STENCIL_LOAD, NUMBITS_LOAD_OP, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
		SetKeyBits(GfxKey, OFFSET_DEPTH_STENCIL_STORE, NUMBITS_STORE_OP, VK_ATTACHMENT_STORE_OP_DONT_CARE);
	}

	CmdBuffer->BeginRenderPass(RenderPass->GetLayout(), RenderPass->GetHandle(), Framebuffer->GetHandle(), ClearValues);

	{
		const VkExtent3D& Extents = RTLayout.GetExtent3D();
		Context.GetPendingGfxState()->SetViewport(0, 0, 0, Extents.width, Extents.height, 1);
	}

	CurrentFramebuffer = Framebuffer;
	CurrentRenderPass = RenderPass;
}

void FVulkanCommandListContext::FTransitionState::EndRenderPass(FVulkanCmdBuffer* CmdBuffer)
{
	check(CurrentRenderPass);
	CmdBuffer->EndRenderPass();
	PreviousRenderPass = CurrentRenderPass;
	CurrentRenderPass = nullptr;
}

void FVulkanCommandListContext::FTransitionState::NotifyDeletedRenderTarget(FVulkanDevice& InDevice, const FVulkanTextureBase* Texture)
{
	for (auto It = Framebuffers.CreateIterator(); It; ++It)
	{
		FFramebufferList* List = It->Value;
		for (int32 Index = List->Framebuffer.Num() - 1; Index >= 0; --Index)
		{
			FVulkanFramebuffer* Framebuffer = List->Framebuffer[Index];
			if (Framebuffer->ContainsRenderTarget(Texture->Surface.Image))
			{
				List->Framebuffer.RemoveAtSwap(Index, 1, false);
				Framebuffer->Destroy(InDevice);
				delete Framebuffer;
			}
		}

		if (List->Framebuffer.Num() == 0)
		{
			delete List;
			It.RemoveCurrent();
		}
	}
}

void FVulkanCommandListContext::NotifyDeletedRenderTarget(const FVulkanTextureBase* Texture)
{
	TransitionState.NotifyDeletedRenderTarget(*Device, Texture);
}

void FVulkanCommandListContext::RHISetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs)
{
	//FRCLog::Printf(FString::Printf(TEXT("RHISetRenderTargets")));

	FRHIDepthRenderTargetView DepthView;
	if (NewDepthStencilTarget)
	{
		DepthView = *NewDepthStencilTarget;
	}
	else
	{
		DepthView = FRHIDepthRenderTargetView(FTextureRHIParamRef(), ERenderTargetLoadAction::ENoAction, ERenderTargetStoreAction::ENoAction, ERenderTargetLoadAction::ENoAction, ERenderTargetStoreAction::ENoAction);
	}

	ensure(NumUAVs == 0);

	if (NumSimultaneousRenderTargets == 1 && (!NewRenderTargets || !NewRenderTargets->Texture))
	{
		--NumSimultaneousRenderTargets;
	}

	FRHISetRenderTargetsInfo RenderTargetsInfo(NumSimultaneousRenderTargets, NewRenderTargets, DepthView);

	FVulkanRenderTargetLayout RTLayout(RenderTargetsInfo);
	uint32 RTLayoutHash = RTLayout.GetHash();
	FVulkanRenderPass* RenderPass = TransitionState.GetOrCreateRenderPass(*Device, RTLayout, RTLayoutHash);
	FVulkanFramebuffer* Framebuffer = TransitionState.GetOrCreateFramebuffer(*Device, RenderTargetsInfo, RTLayout, RenderPass);
	if (Framebuffer == TransitionState.CurrentFramebuffer && RenderPass == TransitionState.CurrentRenderPass)
	{
		return;
	}

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (CmdBuffer->IsInsideRenderPass())
	{
		TransitionState.EndRenderPass(CmdBuffer);
	}

	if (SafePointSubmit())
	{
		CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	}


	// Verify we are not setting the same render targets again
	if (RenderTargetsInfo.DepthStencilRenderTarget.Texture ||
		RenderTargetsInfo.NumColorRenderTargets > 1 ||
		RenderTargetsInfo.NumColorRenderTargets == 1 && RenderTargetsInfo.ColorRenderTarget[0].Texture)
	{
		TransitionState.BeginRenderPass(*this, PendingGfxState->CurrentKey, *Device, CmdBuffer, RenderTargetsInfo, RTLayout, RenderPass, Framebuffer);
	}
}

void FVulkanCommandListContext::RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	//FRCLog::Printf(FString::Printf(TEXT("RHISetRenderTargetsAndClear")));
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (CmdBuffer->IsInsideRenderPass())
	{
		TransitionState.EndRenderPass(CmdBuffer);
	}

	if (SafePointSubmit())
	{
		CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	}

	if (RenderTargetsInfo.DepthStencilRenderTarget.Texture ||
		RenderTargetsInfo.NumColorRenderTargets > 1 ||
		RenderTargetsInfo.NumColorRenderTargets == 1 && RenderTargetsInfo.ColorRenderTarget[0].Texture)
	{
		FVulkanRenderTargetLayout RTLayout(RenderTargetsInfo);
		uint32 RTLayoutHash = RTLayout.GetHash();
		FVulkanRenderPass* RenderPass = TransitionState.GetOrCreateRenderPass(*Device, RTLayout, RTLayoutHash);
		FVulkanFramebuffer* Framebuffer = TransitionState.GetOrCreateFramebuffer(*Device, RenderTargetsInfo, RTLayout, RenderPass);

		TransitionState.BeginRenderPass(*this, PendingGfxState->CurrentKey, *Device, CmdBuffer, RenderTargetsInfo, RTLayout, RenderPass, Framebuffer);
	}
}

void FVulkanCommandListContext::RHICopyToResolveTarget(FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI, bool bKeepOriginalSurface, const FResolveParams& ResolveParams)
{
	//FRCLog::Printf(FString::Printf(TEXT("RHICopyToResolveTarget")));
	if (!SourceTextureRHI || !DestTextureRHI)
	{
		// no need to do anything (silently ignored)
		return;
	}

	RHITransitionResources(EResourceTransitionAccess::EReadable, &SourceTextureRHI, 1);

	auto CopyImage = [](FTransitionState& InRenderPassState, FVulkanCmdBuffer* InCmdBuffer, FVulkanSurface& SrcSurface, FVulkanSurface& DstSurface, uint32 NumLayers)
	{
		VkImageLayout* SrcLayoutPtr = InRenderPassState.CurrentLayout.Find(SrcSurface.Image);
		check(SrcLayoutPtr);
		VkImageLayout SrcLayout = *SrcLayoutPtr;
		VkImageLayout* DstLayoutPtr = InRenderPassState.CurrentLayout.Find(DstSurface.Image);
		VkImageLayout DstLayout = (DstSurface.GetFullAspectMask() & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0 ?
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		if (!DstLayoutPtr)
		{
			InRenderPassState.CurrentLayout.Add(DstSurface.Image, DstLayout);
		}

		check(InCmdBuffer->IsOutsideRenderPass());
		VkCommandBuffer CmdBuffer = InCmdBuffer->GetHandle();
		VkImageSubresourceRange SrcRange ={SrcSurface.GetFullAspectMask(), 0, 1, 0, NumLayers};
		VkImageSubresourceRange DstRange ={DstSurface.GetFullAspectMask(), 0, 1, 0, NumLayers};
		VulkanSetImageLayout(CmdBuffer, SrcSurface.Image, SrcLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SrcRange);
		VulkanSetImageLayout(CmdBuffer, DstSurface.Image, DstLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, DstRange);

		VkImageCopy Region;
		FMemory::Memzero(Region);
		Region.extent.width = FMath::Min(SrcSurface.Width, DstSurface.Width);
		Region.extent.height = FMath::Min(SrcSurface.Height, DstSurface.Height);
		Region.extent.depth = 1;
		Region.srcSubresource.aspectMask = SrcSurface.GetFullAspectMask();
		Region.srcSubresource.layerCount = NumLayers;
		Region.dstSubresource.aspectMask = DstSurface.GetFullAspectMask();
		Region.dstSubresource.layerCount = NumLayers;
		uint32 NumMips = FMath::Min(SrcSurface.GetNumMips(), DstSurface.GetNumMips());
		for (uint32 Index = 0; Index < NumMips; ++Index)
		{
			Region.srcSubresource.mipLevel = Index;
			Region.dstSubresource.mipLevel = Index;
			VulkanRHI::vkCmdCopyImage(CmdBuffer,
				SrcSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				DstSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &Region);
		}

		VulkanSetImageLayout(CmdBuffer, SrcSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SrcLayout, SrcRange);
		VulkanSetImageLayout(CmdBuffer, DstSurface.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, DstLayout, DstRange);
	};
	FRHITexture2D* SourceTextureRHI2D = SourceTextureRHI->GetTexture2D();
	if (SourceTextureRHI2D)
	{
		FRHITexture2D* DestTextureRHI2D = DestTextureRHI->GetTexture2D();
		FVulkanTexture2D* VulkanSrcTexture = (FVulkanTexture2D*)SourceTextureRHI2D;
		FVulkanTexture2D* VulkanDstTexture = (FVulkanTexture2D*)DestTextureRHI2D;
		if (VulkanSrcTexture->Surface.Image != VulkanDstTexture->Surface.Image)
		{
			FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
			CopyImage(TransitionState, CmdBuffer, VulkanSrcTexture->Surface, VulkanDstTexture->Surface, 1);
		}
	}
	else
	{
		FRHITexture3D* SourceTextureRHI3D = SourceTextureRHI->GetTexture3D();
		if (SourceTextureRHI3D)
		{
			FRHITexture3D* DestTextureRHI3D = DestTextureRHI->GetTexture3D();
			FVulkanTexture3D* VulkanSrcTexture = (FVulkanTexture3D*)SourceTextureRHI3D;
			FVulkanTexture3D* VulkanDstTexture = (FVulkanTexture3D*)DestTextureRHI3D;
			if (VulkanSrcTexture->Surface.Image != VulkanDstTexture->Surface.Image)
			{
				FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
				CopyImage(TransitionState, CmdBuffer, VulkanSrcTexture->Surface, VulkanDstTexture->Surface, 1);
			}
		}
		else
		{
			FRHITextureCube* SourceTextureRHICube = SourceTextureRHI->GetTextureCube();
			check(SourceTextureRHICube);
			FRHITextureCube* DestTextureRHICube = DestTextureRHI->GetTextureCube();
			FVulkanTextureCube* VulkanSrcTexture = (FVulkanTextureCube*)SourceTextureRHICube;
			FVulkanTextureCube* VulkanDstTexture = (FVulkanTextureCube*)DestTextureRHICube;
			if (VulkanSrcTexture->Surface.Image != VulkanDstTexture->Surface.Image)
			{
				FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
				CopyImage(TransitionState, CmdBuffer, VulkanSrcTexture->Surface, VulkanDstTexture->Surface, 6);
			}
		}
	}
}

void FVulkanDynamicRHI::RHIReadSurfaceData(FTextureRHIParamRef TextureRHI, FIntRect Rect, TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags)
{
	FRHITexture2D* TextureRHI2D = TextureRHI->GetTexture2D();
	check(TextureRHI2D);
	FVulkanTexture2D* Texture2D = (FVulkanTexture2D*)TextureRHI2D;
	uint32 NumPixels = TextureRHI2D->GetSizeX() * TextureRHI2D->GetSizeY();

	if (GIgnoreCPUReads == 2)
	{
		OutData.Empty(0);
		OutData.AddZeroed(NumPixels);
		return;
	}

	Device->PrepareForCPURead();

	FVulkanCommandListContext& ImmediateContext = Device->GetImmediateContext();

	FVulkanCmdBuffer* CmdBuffer = ImmediateContext.GetCommandBufferManager()->GetUploadCmdBuffer();

	ensure(Texture2D->Surface.StorageFormat == VK_FORMAT_R8G8B8A8_UNORM || Texture2D->Surface.StorageFormat == VK_FORMAT_B8G8R8A8_UNORM);
	const uint32 Size = NumPixels * sizeof(FColor);
	VulkanRHI::FStagingBuffer* StagingBuffer = Device->GetStagingManager().AcquireBuffer(Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);
	if (GIgnoreCPUReads == 0)
	{
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

		//#todo-rco: Multithreaded!
		const VkImageLayout* CurrentLayout = Device->GetImmediateContext().TransitionState.CurrentLayout.Find(Texture2D->Surface.Image);
		VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), Texture2D->Surface.Image, CurrentLayout ? *CurrentLayout : VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		VulkanRHI::vkCmdCopyImageToBuffer(CmdBuffer->GetHandle(), Texture2D->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, StagingBuffer->GetHandle(), 1, &CopyRegion);
		if (CurrentLayout)
		{
			VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), Texture2D->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *CurrentLayout);
		}
		else
		{
			Device->GetImmediateContext().TransitionState.CurrentLayout.Add(Texture2D->Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		}
	}
	else
	{
		VulkanRHI::vkCmdFillBuffer(CmdBuffer->GetHandle(), StagingBuffer->GetHandle(), 0, Size, (uint32)0xffffffff);
	}

	VkBufferMemoryBarrier Barrier;
	VulkanRHI::SetupAndZeroBufferBarrier(Barrier, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT, StagingBuffer->GetHandle(), 0/*StagingBuffer->GetOffset()*/, Size);
	VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &Barrier, 0, nullptr);

	// Force upload
	ImmediateContext.GetCommandBufferManager()->SubmitUploadCmdBuffer(true);
	Device->WaitUntilIdle();

	VkMappedMemoryRange MappedRange;
	FMemory::Memzero(MappedRange);
	MappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	MappedRange.memory = StagingBuffer->GetDeviceMemoryHandle();
	MappedRange.offset = StagingBuffer->GetAllocationOffset();
	MappedRange.size = Size;
	VulkanRHI::vkInvalidateMappedMemoryRanges(Device->GetInstanceHandle(), 1, &MappedRange);

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
	ImmediateContext.GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();
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
	auto DoCopyFloat = [](FVulkanDevice* InDevice, FVulkanCmdBuffer* InCmdBuffer, const FVulkanSurface& Surface, uint32 InMipIndex, uint32 SrcBaseArrayLayer, FIntRect InRect, TArray<FFloat16Color>& OutputData)
	{
		ensure(Surface.StorageFormat == VK_FORMAT_R16G16B16A16_SFLOAT);

		uint32 NumPixels = (Surface.Width >> InMipIndex) * (Surface.Height >> InMipIndex);
		const uint32 Size = NumPixels * sizeof(FFloat16Color);
		VulkanRHI::FStagingBuffer* StagingBuffer = InDevice->GetStagingManager().AcquireBuffer(Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true);

		if (GIgnoreCPUReads == 0)
		{
			VkBufferImageCopy CopyRegion;
			FMemory::Memzero(CopyRegion);
			//Region.bufferOffset = 0;
			CopyRegion.bufferRowLength = Surface.Width >> InMipIndex;
			CopyRegion.bufferImageHeight = Surface.Height >> InMipIndex;
			CopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			CopyRegion.imageSubresource.mipLevel = InMipIndex;
			CopyRegion.imageSubresource.baseArrayLayer = SrcBaseArrayLayer;
			CopyRegion.imageSubresource.layerCount = 1;
			CopyRegion.imageExtent.width = Surface.Width >> InMipIndex;
			CopyRegion.imageExtent.height = Surface.Height >> InMipIndex;
			CopyRegion.imageExtent.depth = 1;

			//#todo-rco: Multithreaded!
			const VkImageLayout* CurrentLayout = InDevice->GetImmediateContext().TransitionState.CurrentLayout.Find(Surface.Image);
			VulkanSetImageLayoutSimple(InCmdBuffer->GetHandle(), Surface.Image, CurrentLayout ? *CurrentLayout : VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			VulkanRHI::vkCmdCopyImageToBuffer(InCmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, StagingBuffer->GetHandle(), 1, &CopyRegion);

			if (CurrentLayout)
			{
				VulkanSetImageLayoutSimple(InCmdBuffer->GetHandle(), Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *CurrentLayout);
			}
			else
			{
				InDevice->GetImmediateContext().TransitionState.CurrentLayout.Add(Surface.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			}
		}
		else
		{
			VulkanRHI::vkCmdFillBuffer(InCmdBuffer->GetHandle(), StagingBuffer->GetHandle(), 0, Size, (FFloat16(1.0).Encoded << 16) + FFloat16(1.0).Encoded);
		}

		VkBufferMemoryBarrier Barrier;
		//#todo-rco: Change offset if reusing a buffer suballocation
		VulkanRHI::SetupAndZeroBufferBarrier(Barrier, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, StagingBuffer->GetHandle(), 0/*StagingBuffer->GetOffset()*/, StagingBuffer->GetDeviceMemoryAllocationSize());
		VulkanRHI::vkCmdPipelineBarrier(InCmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &Barrier, 0, nullptr);

		// Force upload
		InDevice->GetImmediateContext().GetCommandBufferManager()->SubmitUploadCmdBuffer(true);
		InDevice->WaitUntilIdle();

		VkMappedMemoryRange MappedRange;
		FMemory::Memzero(MappedRange);
		MappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		MappedRange.memory = StagingBuffer->GetDeviceMemoryHandle();
		MappedRange.offset = StagingBuffer->GetAllocationOffset();
		MappedRange.size = Size;
		VulkanRHI::vkInvalidateMappedMemoryRanges(InDevice->GetInstanceHandle(), 1, &MappedRange);

		OutputData.SetNum(NumPixels);
		FFloat16Color* Dest = OutputData.GetData();
		for (int32 Row = InRect.Min.Y; Row < InRect.Max.Y; ++Row)
		{
			FFloat16Color* Src = (FFloat16Color*)StagingBuffer->GetMappedPointer() + Row * (Surface.Width >> InMipIndex) + InRect.Min.X;
			for (int32 Col = InRect.Min.X; Col < InRect.Max.X; ++Col)
			{
				*Dest++ = *Src++;
			}
		}
		InDevice->GetStagingManager().ReleaseBuffer(InCmdBuffer, StagingBuffer);
	};

	if (GIgnoreCPUReads == 2)
	{
		// FIll with CPU
		uint32 NumPixels = 0;
		if (TextureRHI->GetTextureCube())
		{
			FRHITextureCube* TextureRHICube = TextureRHI->GetTextureCube();
			FVulkanTextureCube* TextureCube = (FVulkanTextureCube*)TextureRHICube;
			NumPixels = (TextureCube->Surface.Width >> MipIndex) * (TextureCube->Surface.Height >> MipIndex);
		}
		else
		{
			FRHITexture2D* TextureRHI2D = TextureRHI->GetTexture2D();
			check(TextureRHI2D);
			FVulkanTexture2D* Texture2D = (FVulkanTexture2D*)TextureRHI2D;
			NumPixels = (Texture2D->Surface.Width >> MipIndex) * (Texture2D->Surface.Height >> MipIndex);
		}

		OutData.Empty(0);
		OutData.AddZeroed(NumPixels);
	}
	else
	{
		Device->PrepareForCPURead();

		FVulkanCmdBuffer* CmdBuffer = Device->GetImmediateContext().GetCommandBufferManager()->GetUploadCmdBuffer();
		if (TextureRHI->GetTextureCube())
		{
			FRHITextureCube* TextureRHICube = TextureRHI->GetTextureCube();
			FVulkanTextureCube* TextureCube = (FVulkanTextureCube*)TextureRHICube;
			DoCopyFloat(Device, CmdBuffer, TextureCube->Surface, MipIndex, CubeFace, Rect, OutData);
		}
		else
		{
			FRHITexture2D* TextureRHI2D = TextureRHI->GetTexture2D();
			check(TextureRHI2D);
			FVulkanTexture2D* Texture2D = (FVulkanTexture2D*)TextureRHI2D;
			DoCopyFloat(Device, CmdBuffer, Texture2D->Surface, MipIndex, 1, Rect, OutData);
		}
		Device->GetImmediateContext().GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();
	}
}

void FVulkanDynamicRHI::RHIRead3DSurfaceFloatData(FTextureRHIParamRef TextureRHI,FIntRect InRect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData)
{
	Device->PrepareForCPURead();

	VULKAN_SIGNAL_UNIMPLEMENTED();

	Device->GetImmediateContext().GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();

}

void FVulkanCommandListContext::RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteComputeFence)
{
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	ensure(CmdBuffer->IsOutsideRenderPass());
	TArray<VkBufferMemoryBarrier> BufferBarriers;
	TArray<VkImageMemoryBarrier> ImageBarriers;
	for (int32 Index = 0; Index < NumUAVs; ++Index)
	{
		FVulkanUnorderedAccessView* UAV = ResourceCast(InUAVs[Index]);

		VkAccessFlags SrcAccess = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, DestAccess = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		switch (TransitionType)
		{
		case EResourceTransitionAccess::EWritable:
			SrcAccess = VK_ACCESS_SHADER_READ_BIT;
			DestAccess = VK_ACCESS_SHADER_WRITE_BIT;
			break;
		case EResourceTransitionAccess::EReadable:
			SrcAccess = VK_ACCESS_SHADER_WRITE_BIT;
			DestAccess = VK_ACCESS_SHADER_READ_BIT;
			break;
		case EResourceTransitionAccess::ERWBarrier:
			SrcAccess = VK_ACCESS_SHADER_READ_BIT;
			DestAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			break;
		default:
			ensure(0);
			break;
		}

		if (UAV->SourceVertexBuffer)
		{
			VkBufferMemoryBarrier& Barrier = BufferBarriers[BufferBarriers.AddUninitialized()];
			VulkanRHI::SetupAndZeroBufferBarrier(Barrier, SrcAccess, DestAccess, UAV->SourceVertexBuffer->GetHandle(), UAV->SourceVertexBuffer->GetOffset(), UAV->SourceVertexBuffer->GetSize());
		}
		else if (UAV->SourceTexture)
		{
			VkImageMemoryBarrier& Barrier = ImageBarriers[ImageBarriers.AddUninitialized()];
			FVulkanTextureBase* VulkanTexture = FVulkanTextureBase::Cast(UAV->SourceTexture);
			VkImageLayout Layout = TransitionState.FindOrAddLayout(VulkanTexture->Surface.Image, VK_IMAGE_LAYOUT_GENERAL);
			VulkanRHI::SetupAndZeroImageBarrier(Barrier, VulkanTexture->Surface, SrcAccess, Layout, DestAccess, Layout);
		}
		else
		{
			ensure(0);
		}
	}

	VkPipelineStageFlagBits SourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, DestStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	switch (TransitionPipeline)
	{
	case EResourceTransitionPipeline::EGfxToCompute:
		SourceStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
		DestStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		break;
	case EResourceTransitionPipeline::EComputeToGfx:
		SourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		DestStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
		break;
	default:
		ensure(0);
		break;
	}

	VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), SourceStage, DestStage, 0, 0, nullptr, BufferBarriers.Num(), BufferBarriers.GetData(), ImageBarriers.Num(), ImageBarriers.GetData());
}

void FVulkanCommandListContext::RHITransitionResources(EResourceTransitionAccess TransitionType, FTextureRHIParamRef* InTextures, int32 NumTextures)
{
	static IConsoleVariable* CVarShowTransitions = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowTransitions"));
	bool bShowTransitionEvents = CVarShowTransitions->GetInt() != 0;

	SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResources, bShowTransitionEvents, TEXT("TransitionTo: %s: %i Textures"), *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)TransitionType], NumTextures);

	//FRCLog::Printf(FString::Printf(TEXT("RHITransitionResources")));
	if (NumTextures == 0)
	{
		return;
	}

	if (TransitionType == EResourceTransitionAccess::EReadable)
	{
		TArray<VkImageMemoryBarrier> ReadBarriers;
		ReadBarriers.AddZeroed(NumTextures);
		uint32 NumBarriers = 0;

		FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();

		if (TransitionState.CurrentRenderPass)
		{
			bool bEndedRenderPass = false;
			// If any of the textures are in the current render pass, we need to end it
			for (int32 Index = 0; Index < NumTextures; ++Index)
			{
				SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResourcesLoop, bShowTransitionEvents, TEXT("To:%i - %s"), Index, *InTextures[Index]->GetName().ToString());

				FVulkanTextureBase* VulkanTexture = FVulkanTextureBase::Cast(InTextures[Index]);
				bool bIsDepthStencil = (VulkanTexture->Surface.GetFullAspectMask() & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
				VkImageLayout SrcLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				VkImageLayout DstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				if (TransitionState.CurrentFramebuffer->ContainsRenderTarget(InTextures[Index]))
				{
					if (!bEndedRenderPass)
					{
						// If at least one of the textures is inside the render pass, need to end it
						TransitionState.EndRenderPass(CmdBuffer);
						bEndedRenderPass = true;
					}

					SrcLayout = bIsDepthStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				}
				else
				{
					VkImageLayout* FoundLayout = TransitionState.CurrentLayout.Find(VulkanTexture->Surface.Image);
					if (FoundLayout)
					{
						SrcLayout = *FoundLayout;
					}
					else
					{
						ensure(0);
					}
				}

				DstLayout = bIsDepthStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				if (SrcLayout == DstLayout)
				{
					// Ignore redundant layouts
				}
				else
				{
					VkAccessFlags SrcMask = VulkanRHI::GetAccessMask(SrcLayout);
					VkAccessFlags DstMask = VulkanRHI::GetAccessMask(DstLayout);
					VulkanRHI::SetupImageBarrier(ReadBarriers[NumBarriers], VulkanTexture->Surface, SrcMask, SrcLayout, DstMask, DstLayout);
					TransitionState.CurrentLayout.FindOrAdd(VulkanTexture->Surface.Image) = DstLayout;
					++NumBarriers;
				}
			}
		}
		else
		{
			for (int32 Index = 0; Index < NumTextures; ++Index)
			{
				SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResourcesLoop, bShowTransitionEvents, TEXT("To:%i - %s"), Index, *InTextures[Index]->GetName().ToString());

				FVulkanTextureBase* VulkanTexture = FVulkanTextureBase::Cast(InTextures[Index]);
				VkImageLayout SrcLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				VkImageLayout DstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				bool bIsDepthStencil = (VulkanTexture->Surface.GetFullAspectMask() & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
				VkImageLayout* FoundLayout = TransitionState.CurrentLayout.Find(VulkanTexture->Surface.Image);
				if (FoundLayout)
				{
					SrcLayout = *FoundLayout;
				}

				DstLayout = bIsDepthStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				if (SrcLayout == DstLayout)
				{
					// Ignore redundant layouts
				}
				else
				{
					VkAccessFlags SrcMask = VulkanRHI::GetAccessMask(SrcLayout);
					VkAccessFlags DstMask = VulkanRHI::GetAccessMask(DstLayout);
					if (InTextures[Index]->GetTextureCube())
					{
						VulkanRHI::SetupImageBarrier(ReadBarriers[NumBarriers], VulkanTexture->Surface, SrcMask, SrcLayout, DstMask, DstLayout, 6);
					}
					else
					{
						ensure(InTextures[Index]->GetTexture2D() || InTextures[Index]->GetTexture3D());
						VulkanRHI::SetupImageBarrier(ReadBarriers[NumBarriers], VulkanTexture->Surface, SrcMask, SrcLayout, DstMask, DstLayout, 1);
					}
					TransitionState.CurrentLayout.FindOrAdd(VulkanTexture->Surface.Image) = DstLayout;
					++NumBarriers;
				}
			}
		}

		if (NumBarriers > 0)
		{
			VkPipelineStageFlags SourceStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			VkPipelineStageFlags DestStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
			VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), SourceStages, DestStages, 0, 0, nullptr, 0, nullptr, NumBarriers, ReadBarriers.GetData());
		}

		if (CommandBufferManager->GetActiveCmdBuffer()->IsOutsideRenderPass())
		{
			if (SafePointSubmit())
			{
				CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
			}
		}
	}
	else if (TransitionType == EResourceTransitionAccess::EWritable)
	{
		FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
		check(CmdBuffer->HasBegun());

		auto SetImageLayout = [](FTransitionState& InRenderPassState, VkCommandBuffer InCmdBuffer, FVulkanSurface& Surface, uint32 NumArraySlices)
		{
			if (InRenderPassState.CurrentRenderPass)
			{
				check(InRenderPassState.CurrentFramebuffer);
				if (InRenderPassState.CurrentFramebuffer->ContainsRenderTarget(Surface.Image))
				{
#if DO_CHECK
					VkImageLayout* Found = InRenderPassState.CurrentLayout.Find(Surface.Image);
					if (Found)
					{
						VkImageLayout FoundLayout = *Found;
						ensure(FoundLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL || FoundLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
					}
					else
					{
						ensure(Found);
					}
#endif
					// Redundant transition, so skip
					return;
				}
			}

			const VkImageAspectFlags AspectMask = Surface.GetFullAspectMask();
			VkImageSubresourceRange SubresourceRange = { AspectMask, 0, Surface.GetNumMips(), 0, NumArraySlices};

			VkImageLayout SrcLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			VkImageLayout* FoundLayout = InRenderPassState.CurrentLayout.Find(Surface.Image);
			if (FoundLayout)
			{
				SrcLayout = *FoundLayout;
			}

			if ((AspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0)
			{
				VulkanSetImageLayout(InCmdBuffer, Surface.Image, SrcLayout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, SubresourceRange);
				InRenderPassState.CurrentLayout.FindOrAdd(Surface.Image) = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}
			else
			{
				check((AspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0);
				VulkanSetImageLayout(InCmdBuffer, Surface.Image, SrcLayout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, SubresourceRange);
				InRenderPassState.CurrentLayout.FindOrAdd(Surface.Image) = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
		};

		for (int32 i = 0; i < NumTextures; ++i)
		{
			SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResourcesLoop, bShowTransitionEvents, TEXT("To:%i - %s"), i, *InTextures[i]->GetName().ToString());
			FRHITexture* RHITexture = InTextures[i];
		
			FRHITexture2D* RHITexture2D = RHITexture->GetTexture2D();
			if (RHITexture2D)
			{
				FVulkanTexture2D* Texture2D = (FVulkanTexture2D*)RHITexture2D;
				SetImageLayout(TransitionState, CmdBuffer->GetHandle(), Texture2D->Surface, 1);
			}
			else
			{
				FRHITextureCube* RHITextureCube = RHITexture->GetTextureCube();
				if (RHITextureCube)
				{
					FVulkanTextureCube* TextureCube = (FVulkanTextureCube*)RHITextureCube;
					SetImageLayout(TransitionState, CmdBuffer->GetHandle(), TextureCube->Surface, 6);
				}
				else
				{
					FRHITexture3D* RHITexture3D = RHITexture->GetTexture3D();
					if (RHITexture3D)
					{
						FVulkanTexture3D* Texture3D = (FVulkanTexture3D*)RHITexture3D;
						SetImageLayout(TransitionState, CmdBuffer->GetHandle(), Texture3D->Surface, 1);
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
	EPixelFormat TextureFormat[MaxSimultaneousRenderTargets + 1];

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
			TextureFormat[Index] = Texture[Index] ? Texture[Index]->GetFormat() : PF_Unknown;
		}

		Texture[MaxSimultaneousRenderTargets] = RTInfo.DepthStencilRenderTarget.Texture;
		TextureFormat[MaxSimultaneousRenderTargets] = Texture[MaxSimultaneousRenderTargets] ? Texture[MaxSimultaneousRenderTargets]->GetFormat() : PF_Unknown;
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
    
		    if (bSetExtent)
			{
				ensure(Extent.Extent3D.width == Texture->Surface.Width >> RTView.MipIndex);
				ensure(Extent.Extent3D.height == Texture->Surface.Height >> RTView.MipIndex);
				ensure(Extent.Extent3D.depth == Texture->Surface.Depth);
			}
			else
		    {
			    bSetExtent = true;
			    Extent.Extent3D.width = Texture->Surface.Width >> RTView.MipIndex;
			    Extent.Extent3D.height = Texture->Surface.Height >> RTView.MipIndex;
			    Extent.Extent3D.depth = Texture->Surface.Depth;
		    }
    
		    uint32 NumSamples = RTView.Texture->GetNumSamples();
	    
		    VkAttachmentDescription& CurrDesc = Desc[NumAttachments];
		    
		    //@TODO: Check this, it should be a power-of-two. Might be a VulkanConvert helper function.
		    CurrDesc.samples = static_cast<VkSampleCountFlagBits>(NumSamples);
		    CurrDesc.format = UEToVkFormat(RTView.Texture->GetFormat(), (Texture->Surface.UEFlags & TexCreate_SRGB) == TexCreate_SRGB);
		    CurrDesc.loadOp = RenderTargetLoadActionToVulkan(RTView.LoadAction);
		    CurrDesc.storeOp = RenderTargetStoreActionToVulkan(RTView.StoreAction);
		    CurrDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		    CurrDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		    CurrDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		    CurrDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		    ColorReferences[NumColorAttachments].attachment = NumAttachments;
		    ColorReferences[NumColorAttachments].layout = VK_IMAGE_LAYOUT_GENERAL;
		    NumAttachments++;
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

		if (bSetExtent)
		{
			ensure(Extent.Extent3D.width == Texture->Surface.Width);
			ensure(Extent.Extent3D.height == Texture->Surface.Height);
		}
		else
		{
			bSetExtent = true;
			Extent.Extent3D.width = Texture->Surface.Width;
			Extent.Extent3D.height = Texture->Surface.Height;
			Extent.Extent3D.depth = 1;
		}
	}

	FRenderTargetLayoutHashableStruct RTHash;
	FMemory::Memzero(RTHash);
	RTHash.Set(RTInfo);
	RTHash.Extents = Extent.Extent2D;
	Hash = FCrc::MemCrc32(&RTHash, sizeof(RTHash));
}
