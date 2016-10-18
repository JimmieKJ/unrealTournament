// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanViewport.cpp: Vulkan viewport RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanManager.h"
#include "VulkanSwapchain.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"


struct FRHICommandAcquireBackBuffer : public FRHICommand<FRHICommandAcquireBackBuffer>
{
	FVulkanViewport* Viewport;
	FVulkanBackBuffer* NewBackBuffer;
	FORCEINLINE_DEBUGGABLE FRHICommandAcquireBackBuffer(FVulkanViewport* InViewport, FVulkanBackBuffer* InNewBackBuffer)
		: Viewport(InViewport)
		, NewBackBuffer(InNewBackBuffer)
	{
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		Viewport->AcquireBackBuffer(CmdList, NewBackBuffer);
	}
};


FVulkanViewport::FVulkanViewport(FVulkanDynamicRHI* InRHI, FVulkanDevice* InDevice, void* InWindowHandle, uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen, EPixelFormat InPreferredPixelFormat)
	: VulkanRHI::FDeviceChild(InDevice)
	, RHI(InRHI)
	, SizeX(InSizeX)
	, SizeY(InSizeY)
	, bIsFullscreen(bInIsFullscreen)
	, PixelFormat(InPreferredPixelFormat)
	, AcquiredImageIndex(-1)
	, SwapChain(nullptr)
	, WindowHandle(InWindowHandle)
	, PresentCount(0)
	, AcquiredSemaphore(nullptr)
{
	check(IsInGameThread());
	FMemory::Memzero(BackBufferImages);
	RHI->Viewports.Add(this);

	// Make sure Instance is created
	RHI->InitInstance();

	CreateSwapchain();

	for (int32 Index = 0; Index < NUM_BUFFERS; ++Index)
	{
		RenderingDoneSemaphores[Index] = new FVulkanSemaphore(*InDevice);
	}
}

FVulkanViewport::~FVulkanViewport()
{
	RenderingBackBuffer = nullptr;
	RHIBackBuffer = nullptr;

	for (int32 Index = 0; Index < NUM_BUFFERS; ++Index)
	{
		delete RenderingDoneSemaphores[Index];

		TextureViews[Index].Destroy(*Device);
	}

	SwapChain->Destroy();
	delete SwapChain;

	RHI->Viewports.Remove(this);
}

void FVulkanViewport::AcquireBackBuffer(FRHICommandListBase& CmdList, FVulkanBackBuffer* NewBackBuffer)
{
	check(NewBackBuffer);
	RHIBackBuffer = NewBackBuffer;

	int32 PrevImageIndex = AcquiredImageIndex;
	AcquiredImageIndex = SwapChain->AcquireImageIndex(&AcquiredSemaphore);
	check(AcquiredImageIndex != -1);
	//FRCLog::Printf(FString::Printf(TEXT("FVulkanViewport::AcquireBackBuffer(), Prev=%d, AcquiredImageIndex => %d"), PrevImageIndex, AcquiredImageIndex));
	RHIBackBuffer->Surface.Image = BackBufferImages[AcquiredImageIndex];
	RHIBackBuffer->DefaultView.View = TextureViews[AcquiredImageIndex].View;
	FVulkanCommandListContext& Context = (FVulkanCommandListContext&)CmdList.GetContext();

	FVulkanCommandBufferManager* CmdBufferManager = Context.GetCommandBufferManager();
	FVulkanCmdBuffer* CmdBuffer = CmdBufferManager->GetActiveCmdBuffer();
	check(CmdBuffer->IsOutsideRenderPass());
	VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), BackBufferImages[AcquiredImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	// Submit here so we can add a dependency with the acquired semaphore
	CmdBuffer->End();
	Device->GetQueue()->Submit(CmdBuffer, AcquiredSemaphore, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, nullptr);
	CmdBufferManager->PrepareForNewActiveCommandBuffer();
}

FVulkanTexture2D* FVulkanViewport::GetBackBuffer(FRHICommandList& RHICmdList)
{
	check(IsInRenderingThread());
	//FRCLog::Printf(FString::Printf(TEXT("FVulkanViewport::GetBackBuffer(), AcquiredImageIndex=%d %s"), AcquiredImageIndex, RenderingBackBuffer ? TEXT("BB") : TEXT("NullBB")));

	if (!RenderingBackBuffer)
	{
		RenderingBackBuffer = new FVulkanBackBuffer(*Device, PixelFormat, SizeX, SizeY, VK_NULL_HANDLE, TexCreate_Presentable | TexCreate_RenderTargetable);
		check(RHICmdList.IsImmediate());

		if (RHICmdList.Bypass() || !GRHIThread)
		{
			FRHICommandAcquireBackBuffer Cmd(this, RenderingBackBuffer);
			Cmd.Execute(RHICmdList);
		}
		else
		{
			new (RHICmdList.AllocCommand<FRHICommandAcquireBackBuffer>()) FRHICommandAcquireBackBuffer(this, RenderingBackBuffer);
		}
	}

	return RenderingBackBuffer;
}

void FVulkanViewport::AdvanceBackBufferFrame()
{
	check(IsInRenderingThread());

	//FRCLog::Printf(FString::Printf(TEXT("FVulkanViewport::AdvanceBackBufferFrame(), AcquiredImageIndex = %d"), AcquiredImageIndex));
	RenderingBackBuffer = nullptr;
}

void FVulkanViewport::WaitForFrameEventCompletion()
{
}


FVulkanFramebuffer::FVulkanFramebuffer(FVulkanDevice& Device, const FRHISetRenderTargetsInfo& InRTInfo, const FVulkanRenderTargetLayout& RTLayout, const FVulkanRenderPass& RenderPass)
	: Framebuffer(VK_NULL_HANDLE)
	, RTInfo(InRTInfo)
	, NumColorAttachments(0)
	, BackBuffer(0)
{
	AttachmentViews.Empty(RTLayout.GetNumAttachments());
	uint32 MipIndex = 0;

	for (int32 Index = 0; Index < InRTInfo.NumColorRenderTargets; ++Index)
	{
		FRHITexture* RHITexture = InRTInfo.ColorRenderTarget[Index].Texture;
		if(!RHITexture)
		{
			continue;
		}

		FVulkanTextureBase* Texture = FVulkanTextureBase::Cast(RHITexture);
	#if VULKAN_USE_MSAA_RESOLVE_ATTACHMENTS
		if (Texture->MSAASurface)
		{
#if 1//VULKAN_USE_NEW_COMMAND_BUFFERS
			check(0);
#endif
			AttachmentViews.Add(Texture->MSAAView.View);

		    // Create a write-barrier
		    WriteBarriers.AddZeroed();
		    VkImageMemoryBarrier& Barrier = WriteBarriers.Last();
		    Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		    //Barrier.pNext = NULL;
		    Barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		    Barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		    Barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		    Barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		    Barrier.image = Texture->Surface.Image;
		    Barrier.subresourceRange.aspectMask = Texture->MSAASurface->GetAspectMask();
		    //Barrier.subresourceRange.baseMipLevel = 0;
		    Barrier.subresourceRange.levelCount = 1;
		    //Barrier.subresourceRange.baseArrayLayer = 0;
		    Barrier.subresourceRange.layerCount = 1;
			Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		}
	#endif
		MipIndex = InRTInfo.ColorRenderTarget[Index].MipIndex;
		VkImageView RTView = Texture->CreateRenderTargetView(MipIndex, FMath::Max(0, (int32)InRTInfo.ColorRenderTarget[Index].ArraySliceIndex));
		AttachmentViews.Add(RTView);

		// Create a write-barrier
		WriteBarriers.AddZeroed();
		VkImageMemoryBarrier& Barrier = WriteBarriers.Last();
		Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		//Barrier.pNext = NULL;
		Barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		Barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		Barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		Barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		Barrier.image = Texture->Surface.Image;
		Barrier.subresourceRange.aspectMask = Texture->Surface.GetFullAspectMask();
		//Barrier.subresourceRange.baseMipLevel = 0;
		Barrier.subresourceRange.levelCount = 1;
		//Barrier.subresourceRange.baseArrayLayer = 0;
		Barrier.subresourceRange.layerCount = 1;

		if (Texture->Surface.GetViewType() == VK_IMAGE_VIEW_TYPE_2D)
		{
			FVulkanTexture2D* Texture2D = (FVulkanTexture2D*)Texture;
			BackBuffer = Texture2D->GetBackBuffer();
		}

		NumColorAttachments++;
	}

	if (RTLayout.GetHasDepthStencil())
	{
		FVulkanTextureBase* Texture = FVulkanTextureBase::Cast(InRTInfo.DepthStencilRenderTarget.Texture);

		bool bHasStencil = (Texture->Surface.Format == PF_DepthStencil);

		// Create a write-barrier
		WriteBarriers.AddZeroed();
		VkImageMemoryBarrier& Barrier = WriteBarriers.Last();
		Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		//Barrier.pNext = NULL;
		Barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		Barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		Barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		Barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		Barrier.image = Texture->Surface.Image;

		Barrier.subresourceRange.aspectMask = Texture->Surface.GetFullAspectMask();
		//Barrier.subresourceRange.baseMipLevel = 0;
		Barrier.subresourceRange.levelCount = 1;
		//#todo-rco: Cube depth face?
		//Barrier.subresourceRange.baseArrayLayer = 0;
		Barrier.subresourceRange.layerCount = 1;

		VkImageView RTView = Texture->CreateRenderTargetView(0, 0);
		AttachmentViews.Add(RTView);
	}

	VkFramebufferCreateInfo Info;
	FMemory::Memzero(Info);
	Info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	Info.pNext = nullptr;
	Info.renderPass = RenderPass.GetHandle();
	Info.attachmentCount = AttachmentViews.Num();
	Info.pAttachments = AttachmentViews.GetData();
	Info.width  = RTLayout.GetExtent3D().width;
	Info.height = RTLayout.GetExtent3D().height;
	Info.layers = 1;
	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkCreateFramebuffer(Device.GetInstanceHandle(), &Info, nullptr, &Framebuffer));

	Extents.width = Info.width;
	Extents.height = Info.height;
}

void FVulkanFramebuffer::Destroy(FVulkanDevice& Device)
{
	VulkanRHI::FDeferredDeletionQueue& Queue = Device.GetDeferredDeletionQueue();

	for (int32 Index = 0; Index < AttachmentViews.Num(); ++Index)
	{
		Queue.EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::ImageView, AttachmentViews[Index]);
	}

	Queue.EnqueueResource(VulkanRHI::FDeferredDeletionQueue::EType::Framebuffer, Framebuffer);
	Framebuffer = VK_NULL_HANDLE;
}

bool FVulkanFramebuffer::Matches(const FRHISetRenderTargetsInfo& InRTInfo) const
{
	if (RTInfo.NumColorRenderTargets != InRTInfo.NumColorRenderTargets)
	{
		return false;
	}
	if (RTInfo.bClearColor != InRTInfo.bClearColor)
	{
		return false;
	}
	if (RTInfo.bClearDepth != InRTInfo.bClearDepth)
	{
		return false;
	}
	if (RTInfo.bClearStencil != InRTInfo.bClearStencil)
	{
		return false;
	}

	{
		const FRHIDepthRenderTargetView& A = RTInfo.DepthStencilRenderTarget;
		const FRHIDepthRenderTargetView& B = InRTInfo.DepthStencilRenderTarget;
		if (!(A == B))
		{
			return false;
		}
	}

	// We dont need to compare all render-tagets, since we
	// already have compared the number of render-targets
	for (int32 Index = 0; Index < RTInfo.NumColorRenderTargets; ++Index)
	{
		const FRHIRenderTargetView& A = RTInfo.ColorRenderTarget[Index];
		const FRHIRenderTargetView& B = InRTInfo.ColorRenderTarget[Index];
		if (!(A == B))
		{
			return false;
		}
	}

	return true;
}

void FVulkanViewport::CreateSwapchain()
{
	uint32 DesiredNumBackBuffers = NUM_BUFFERS;

	TArray<VkImage> Images;
	SwapChain = new FVulkanSwapChain(
		RHI->Instance, *Device, WindowHandle,
		PixelFormat, SizeX, SizeY,
		&DesiredNumBackBuffers,
		Images
		);

	check(Images.Num() == NUM_BUFFERS);

	FVulkanCmdBuffer* CmdBuffer = Device->GetImmediateContext().GetCommandBufferManager()->GetUploadCmdBuffer();
	ensure(CmdBuffer->IsOutsideRenderPass());

	for (int32 Index = 0; Index < Images.Num(); ++Index)
	{
		BackBufferImages[Index] = Images[Index];

		FName Name = FName(*FString::Printf(TEXT("BackBuffer%d"), Index));
		//BackBuffers[Index]->SetName(Name);

		TextureViews[Index].Create(*Device, Images[Index], VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, PixelFormat, UEToVkFormat(PixelFormat, false), 0, 1, 0, 1);

		// Clear the swapchain to avoid a validation warning, and transition to ColorAttachment
		{
			VkImageSubresourceRange Range;
			FMemory::Memzero(Range);
			Range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			Range.baseMipLevel = 0;
			Range.levelCount = 1;
			Range.baseArrayLayer = 0;
			Range.layerCount = 1;

			VkClearColorValue Color;
			FMemory::Memzero(Color);
			VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), Images[Index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
			VulkanRHI::vkCmdClearColorImage(CmdBuffer->GetHandle(), Images[Index], VK_IMAGE_LAYOUT_GENERAL, &Color, 1, &Range);
			VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), Images[Index], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		}
	}
}

bool FVulkanViewport::Present(FVulkanCmdBuffer* CmdBuffer, FVulkanQueue* Queue, bool bLockToVsync)
{
	//FRCLog::Printf(FString::Printf(TEXT("FVulkanViewport::Present(), AcquiredImageIndex=%d"), AcquiredImageIndex));
	check(AcquiredImageIndex != -1);

	//Transition back buffer to presentable and submit that command
	check(CmdBuffer->IsOutsideRenderPass());

	check(RHIBackBuffer && RHIBackBuffer->Surface.Image == BackBufferImages[AcquiredImageIndex]);

	//#todo-rco: Might need to NOT be undefined...
	VulkanSetImageLayoutSimple(CmdBuffer->GetHandle(), BackBufferImages[AcquiredImageIndex], VK_IMAGE_LAYOUT_UNDEFINED/*VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	{
		FVulkanTimestampQueryPool* TimestampPool = Device->GetTimestampQueryPool(PresentCount % FVulkanDevice::NumTimestampPools);
		if (TimestampPool)
		{
			TimestampPool->WriteEndFrame(CmdBuffer);
		}

		if (PresentCount >= FVulkanDevice::NumTimestampPools)
		{
			FVulkanTimestampQueryPool* PrevTimestampPool = Device->GetTimestampQueryPool((PresentCount + FVulkanDevice::NumTimestampPools - 1) % FVulkanDevice::NumTimestampPools);
			PrevTimestampPool->CalculateFrameTime();
		}
	}

	CmdBuffer->End();
	Queue->Submit(CmdBuffer, nullptr, 0, RenderingDoneSemaphores[AcquiredImageIndex]);

	//Flush all commands
	//check(0);

	//#todo-rco: Proper SyncInterval bLockToVsync ? RHIConsoleVariables::SyncInterval : 0
	int32 SyncInterval = 0;
	bool bNeedNativePresent = true;
	if (IsValidRef(CustomPresent))
	{
		bNeedNativePresent = CustomPresent->Present(SyncInterval);
	}

	bool bResult = false;
	if (bNeedNativePresent)
	{
		// Present the back buffer to the viewport window.
		bResult = SwapChain->Present(Queue, RenderingDoneSemaphores[AcquiredImageIndex]);//, SyncInterval, 0);

		// Release the back buffer
		RHIBackBuffer = nullptr;
	}

	static const TConsoleVariableData<int32>* CFinishFrameVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.FinishCurrentFrame"));
	if (!CFinishFrameVar->GetValueOnRenderThread())
	{
		// Wait for the GPU to finish rendering the previous frame before finishing this frame.
		WaitForFrameEventCompletion();
		IssueFrameEvent();
	}
	else
	{
		// Finish current frame immediately to reduce latency
		IssueFrameEvent();
		WaitForFrameEventCompletion();
	}

	// If the input latency timer has been triggered, block until the GPU is completely
	// finished displaying this frame and calculate the delta time.
	if (GInputLatencyTimer.RenderThreadTrigger)
	{
		WaitForFrameEventCompletion();
		uint32 EndTime = FPlatformTime::Cycles();
		GInputLatencyTimer.DeltaTime = EndTime - GInputLatencyTimer.StartTime;
		GInputLatencyTimer.RenderThreadTrigger = false;
	}

	//#todo-rco: This needs to go on RHIEndFrame but the CmdBuffer index is not the correct one to read the stats out!
	VulkanRHI::GManager.GPUProfilingData.EndFrame();

	FVulkanCommandBufferManager* ImmediateCmdBufMgr = Device->GetImmediateContext().GetCommandBufferManager();
	ImmediateCmdBufMgr->PrepareForNewActiveCommandBuffer();

	//#todo-rco: Consolidate 'end of frame'
	Device->GetImmediateContext().GetTempFrameAllocationBuffer().Reset();
#if 0
	CurrentBackBuffer = -1;
	PendingState->Reset();

	const uint32 QueryCurrFrameIndex = PresentCount % FVulkanDevice::NumTimestampPools;
	const uint32 QueryPrevFrameIndex = (QueryCurrFrameIndex + FVulkanDevice::NumTimestampPools - 1) % FVulkanDevice::NumTimestampPools;
	const uint32 QueryNextFrameIndex = (QueryCurrFrameIndex + 1) % FVulkanDevice::NumTimestampPools;

	FVulkanTimestampQueryPool* TimestampQueryPool = Device->GetTimestampQueryPool(QueryPrevFrameIndex);
	if (TimestampQueryPool)
	{
		if (PresentCount > FVulkanDevice::NumTimestampPools)
		{
			TimestampQueryPool->CalculateFrameTime();
		}

		Device->GetTimestampQueryPool(QueryNextFrameIndex)->WriteStartFrame(CmdBufferManager->GetActiveCmdBuffer()->GetHandle());
	}
#endif
	++PresentCount;
	{
		FVulkanTimestampQueryPool* TimestampPool = Device->GetTimestampQueryPool(PresentCount % FVulkanDevice::NumTimestampPools);
		if (TimestampPool)
		{
			FVulkanCmdBuffer* ActiveCmdBuffer = ImmediateCmdBufMgr->GetActiveCmdBuffer();
			TimestampPool->WriteStartFrame(ActiveCmdBuffer->GetHandle());
		}
	}

	return bResult;
}

void FVulkanFramebuffer::InsertWriteBarriers(FVulkanCmdBuffer* CmdBuffer)
{
	if (WriteBarriers.Num() == 0)
	{
		return;
	}

	const VkPipelineStageFlags SrcStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	const VkPipelineStageFlags DestStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	check(CmdBuffer->IsOutsideRenderPass());
	VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), SrcStages, DestStages, 0, 0, nullptr, 0, nullptr, WriteBarriers.Num(), WriteBarriers.GetData());
}

/*=============================================================================
 *	The following RHI functions must be called from the main thread.
 *=============================================================================*/
FViewportRHIRef FVulkanDynamicRHI::RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
{
	check( IsInGameThread() );
	return new FVulkanViewport(this, Device, WindowHandle, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat);
}

void FVulkanDynamicRHI::RHIResizeViewport(FViewportRHIParamRef ViewportRHI, uint32 SizeX, uint32 SizeY, bool bIsFullscreen)
{
	check(IsInGameThread());
	FVulkanViewport* Viewport = ResourceCast(ViewportRHI);
}

void FVulkanDynamicRHI::RHITick(float DeltaTime)
{
	check(IsInGameThread());
}

/*
void FVulkanDynamicRHI::WriteEndFrameTimestamp(void* Data)
{
	FVulkanDynamicRHI* This = (FVulkanDynamicRHI*)Data;
	if (This && This->Device)
	{
		FVulkanTimestampQueryPool* TimestampQueryPool = This->Device->GetTimestampQueryPool(This->PresentCount % (uint32)FVulkanDevice::NumTimestampPools);
		if (TimestampQueryPool)
		{
			FVulkanCmdBuffer* CmdBuffer = This->Device->GetImmediateContext().GetCommandBufferManager()->GetActiveCmdBuffer();
			TimestampQueryPool->WriteEndFrame(CmdBuffer->GetHandle());
		}
	}

	VulkanRHI::GManager.GPUProfilingData.EndFrameBeforeSubmit();
}
*/

#if 0
void FVulkanDynamicRHI::Present()
{
#if 0
	check(DrawingViewport);

	check(Device);

	FVulkanPendingState* PendingState = &Device->GetImmediateContext().GetPendingState();
	FVulkanCommandBufferManager* CmdBufferManager = Device->GetImmediateContext().GetCommandBufferManager();
	FVulkanCmdBuffer* CmdBuffer = CmdBufferManager->GetActiveCmdBuffer();
	if (PendingState->IsRenderPassActive())
	{
		PendingState->RenderPassEnd(CmdBuffer);
	}




	check(0);


	//#todo-rco: Proper SyncInterval bLockToVsync ? RHIConsoleVariables::SyncInterval : 0
	int32 SyncInterval = 0;
	bool bNeedNativePresent = true;
	if (IsValidRef(DrawingViewport->CustomPresent))
	{
		bNeedNativePresent = DrawingViewport->CustomPresent->Present(SyncInterval);
	}


	if (bNeedNativePresent)
	{
		// Present the back buffer to the viewport window.
		/*Result = */DrawingViewport->SwapChain->Present(SyncInterval, 0);
	}
#endif
	check(0);

#if 0
	FVulkanBackBuffer* BackBuffer = DrawingViewport->PrepareBackBufferForPresent(CmdBuffer);
	WriteEndFrameTimestamp(this);
	CmdBuffer->End();
	Device->GetQueue()->Submit(CmdBuffer, BackBuffer->AcquiredSemaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, BackBuffer->RenderingDoneSemaphore);
	// No need to acquire it anymore
	BackBuffer->AcquiredSemaphore = nullptr;

	DrawingViewport->GetSwapChain()->Present(Device->GetQueue(), BackBuffer->RenderingDoneSemaphore);

	bool bNativelyPresented = true;
	if (bNativelyPresented)
	{
		static const TConsoleVariableData<int32>* CFinishFrameVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.FinishCurrentFrame"));
		if (!CFinishFrameVar->GetValueOnRenderThread())
		{
			// Wait for the GPU to finish rendering the previous frame before finishing this frame.
			DrawingViewport->WaitForFrameEventCompletion();
			DrawingViewport->IssueFrameEvent();
		}
		else
		{
			// Finish current frame immediately to reduce latency
			DrawingViewport->IssueFrameEvent();
			DrawingViewport->WaitForFrameEventCompletion();
		}
	}

	// If the input latency timer has been triggered, block until the GPU is completely
	// finished displaying this frame and calculate the delta time.
	if (GInputLatencyTimer.RenderThreadTrigger)
	{
		DrawingViewport->WaitForFrameEventCompletion();
		uint32 EndTime = FPlatformTime::Cycles();
		GInputLatencyTimer.DeltaTime = EndTime - GInputLatencyTimer.StartTime;
		GInputLatencyTimer.RenderThreadTrigger = false;
	}

	//#todo-rco: This needs to go on RHIEndFrame but the CmdBuffer index is not the correct one to read the stats out!
	VulkanRHI::GManager.GPUProfilingData.EndFrame();

	Device->GetImmediateContext().GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();

	//#todo-rco: Consolidate 'end of frame'
	Device->GetImmediateContext().GetTempFrameAllocationBuffer().Reset();

	DrawingViewport->CurrentBackBuffer = -1;
	DrawingViewport = nullptr;
	PendingState->Reset();

	const uint32 QueryCurrFrameIndex = PresentCount % FVulkanDevice::NumTimestampPools;
	const uint32 QueryPrevFrameIndex = (QueryCurrFrameIndex + FVulkanDevice::NumTimestampPools - 1) % FVulkanDevice::NumTimestampPools;
	const uint32 QueryNextFrameIndex = (QueryCurrFrameIndex + 1) % FVulkanDevice::NumTimestampPools;

	FVulkanTimestampQueryPool* TimestampQueryPool = Device->GetTimestampQueryPool(QueryPrevFrameIndex);
	if (TimestampQueryPool)
	{
		if(PresentCount > FVulkanDevice::NumTimestampPools)
		{
			TimestampQueryPool->CalculateFrameTime();
		}

		Device->GetTimestampQueryPool(QueryNextFrameIndex)->WriteStartFrame(CmdBufferManager->GetActiveCmdBuffer()->GetHandle());
	}
#endif
	PresentCount++;
}
#endif

FTexture2DRHIRef FVulkanDynamicRHI::RHIGetViewportBackBuffer(FViewportRHIParamRef ViewportRHI)
{
	check(IsInRenderingThread());
	check(ViewportRHI);
	FVulkanViewport* Viewport = ResourceCast(ViewportRHI);
	return Viewport->GetBackBuffer(FRHICommandListExecutor::GetImmediateCommandList());
}

void FVulkanDynamicRHI::RHIAdvanceFrameForGetViewportBackBuffer()
{
	for (FVulkanViewport* Viewport : Viewports)
	{
		Viewport->AdvanceBackBufferFrame();
	}
}

void FVulkanCommandListContext::RHISetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
{
	check(Device);
	PendingState->SetViewport(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
}

void FVulkanCommandListContext::RHISetStereoViewport(uint32 LeftMinX, uint32 RightMinX, uint32 MinY, float MinZ, uint32 LeftMaxX, uint32 RightMaxX, uint32 MaxY, float MaxZ)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
{
	check(Device);
	PendingState->SetScissor(bEnable, MinX, MinY, MaxX, MaxY);
}
