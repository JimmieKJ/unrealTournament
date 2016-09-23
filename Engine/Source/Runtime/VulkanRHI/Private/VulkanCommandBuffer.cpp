// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanCommandBuffer.cpp: Vulkan device RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanCommandBuffer.h"

FVulkanCmdBuffer::FVulkanCmdBuffer(FVulkanDevice* InDevice, FVulkanCommandBufferManager* InCommandBufferManager)
	: Device(InDevice)
	, CommandBufferManager(InCommandBufferManager)
	, CommandBufferHandle(VK_NULL_HANDLE)
	, State(EState::ReadyForBegin)
	, Fence(nullptr)
	, FenceSignaledCounter(0)
{
	VkCommandBufferAllocateInfo CreateCmdBufInfo;
	FMemory::Memzero(CreateCmdBufInfo);
	CreateCmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	CreateCmdBufInfo.pNext = nullptr;
	CreateCmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	CreateCmdBufInfo.commandBufferCount = 1;
	CreateCmdBufInfo.commandPool = CommandBufferManager->GetHandle();

	VERIFYVULKANRESULT(VulkanRHI::vkAllocateCommandBuffers(Device->GetInstanceHandle(), &CreateCmdBufInfo, &CommandBufferHandle));
	Fence = Device->GetFenceManager().AllocateFence();
}

FVulkanCmdBuffer::~FVulkanCmdBuffer()
{
	VulkanRHI::FFenceManager& FenceManager = Device->GetFenceManager();
	if (State == EState::Submitted)
	{
		// Wait 60ms
		uint64 WaitForCmdBufferInNanoSeconds = 60 * 1000 * 1000LL;
		FenceManager.WaitAndReleaseFence(Fence, WaitForCmdBufferInNanoSeconds);
	}
	else
	{
		// Just free the fence, CmdBuffer was not submitted
		FenceManager.ReleaseFence(Fence);
	}

	VulkanRHI::vkFreeCommandBuffers(Device->GetInstanceHandle(), CommandBufferManager->GetHandle(), 1, &CommandBufferHandle);
	CommandBufferHandle = VK_NULL_HANDLE;
}

void FVulkanCmdBuffer::BeginRenderPass(const FVulkanRenderTargetLayout& Layout, VkRenderPass RenderPass, VkFramebuffer Framebuffer, const VkClearValue* AttachmentClearValues)
{
	check(IsOutsideRenderPass());

	VkRenderPassBeginInfo Info;
	FMemory::Memzero(Info);
	Info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	Info.renderPass = RenderPass;
	Info.framebuffer = Framebuffer;
	Info.renderArea.offset.x = 0;
	Info.renderArea.offset.y = 0;
	Info.renderArea.extent = Layout.GetExtent2D();
	Info.clearValueCount = Layout.GetNumAttachments();
	Info.pClearValues = AttachmentClearValues;

	VulkanRHI::vkCmdBeginRenderPass(CommandBufferHandle, &Info, VK_SUBPASS_CONTENTS_INLINE);

	State = EState::IsInsideRenderPass;
}

void FVulkanCmdBuffer::Begin()
{
	check(State == EState::ReadyForBegin);

	VkCommandBufferBeginInfo CmdBufBeginInfo;
	FMemory::Memzero(CmdBufBeginInfo);
	CmdBufBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	CmdBufBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VERIFYVULKANRESULT(VulkanRHI::vkBeginCommandBuffer(CommandBufferHandle, &CmdBufBeginInfo));

	State = EState::IsInsideBegin;
}

void FVulkanCmdBuffer::RefreshFenceStatus()
{
	if (State == EState::Submitted)
	{
		VulkanRHI::FFenceManager* FenceMgr = Fence->GetOwner();
		if (FenceMgr->IsFenceSignaled(Fence))
		{
			State = EState::ReadyForBegin;
			VulkanRHI::vkResetCommandBuffer(CommandBufferHandle, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
#if VULKAN_REUSE_FENCES
			Fence->GetOwner()->ResetFence(Fence);
#else
			VulkanRHI::FFence* PrevFence = Fence;
			Fence = FenceMgr->AllocateFence();
			FenceMgr->ReleaseFence(PrevFence);
#endif
			++FenceSignaledCounter;
		}
	}
	else
	{
		check(!Fence->IsSignaled());
	}
}


FVulkanCommandBufferManager::FVulkanCommandBufferManager(FVulkanDevice* InDevice)
	: Device(InDevice)
	, Handle(VK_NULL_HANDLE)
	, ActiveCmdBuffer(nullptr)
	, UploadCmdBuffer(nullptr)
{
	check(Device);

	VkCommandPoolCreateInfo CmdPoolInfo;
	FMemory::Memzero(CmdPoolInfo);
	CmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	CmdPoolInfo.queueFamilyIndex = Device->GetQueue()->GetFamilyIndex();
	//#todo-rco: Should we use VK_COMMAND_POOL_CREATE_TRANSIENT_BIT?
	CmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VERIFYVULKANRESULT(VulkanRHI::vkCreateCommandPool(Device->GetInstanceHandle(), &CmdPoolInfo, nullptr, &Handle));

	ActiveCmdBuffer = Create();
	ActiveCmdBuffer->Begin();
}

FVulkanCommandBufferManager::~FVulkanCommandBufferManager()
{
	for (int32 Index = 0; Index < CmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = CmdBuffers[Index];
		delete CmdBuffer;
	}

	VulkanRHI::vkDestroyCommandPool(Device->GetInstanceHandle(), Handle, nullptr);
}

void FVulkanCommandBufferManager::SubmitUploadCmdBuffer(bool bWaitForFence)
{
	check(UploadCmdBuffer);
	check(UploadCmdBuffer->IsOutsideRenderPass());
	UploadCmdBuffer->End();
	Device->GetQueue()->Submit(UploadCmdBuffer, nullptr, 0, nullptr);
	UploadCmdBuffer = nullptr;

	if (bWaitForFence)
	{
		Device->WaitUntilIdle();
		RefreshFenceStatus();
	}
}

FVulkanCmdBuffer* FVulkanCommandBufferManager::Create()
{
	check(Device);
	FVulkanCmdBuffer* CmdBuffer = new FVulkanCmdBuffer(Device, this);
	CmdBuffers.Add(CmdBuffer);
	check(CmdBuffer);
	return CmdBuffer;
}

void FVulkanCommandBufferManager::RefreshFenceStatus()
{
	for (int32 Index = 0; Index < CmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = CmdBuffers[Index];
		CmdBuffer->RefreshFenceStatus();
	}
}

void FVulkanCommandBufferManager::PrepareForNewActiveCommandBuffer()
{
	check(!UploadCmdBuffer);

	for (int32 Index = 0; Index < CmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = CmdBuffers[Index];
		CmdBuffer->RefreshFenceStatus();
		if (CmdBuffer->State == FVulkanCmdBuffer::EState::ReadyForBegin)
		{
			ActiveCmdBuffer = CmdBuffer;
			ActiveCmdBuffer->Begin();
			return;
		}
		else
		{
			check(CmdBuffer->State == FVulkanCmdBuffer::EState::Submitted);
		}
	}

	// All cmd buffers are being executed still
	ActiveCmdBuffer = Create();
	ActiveCmdBuffer->Begin();
}

FVulkanCmdBuffer* FVulkanCommandBufferManager::GetUploadCmdBuffer()
{
	if (!UploadCmdBuffer)
	{
		for (int32 Index = 0; Index < CmdBuffers.Num(); ++Index)
		{
			FVulkanCmdBuffer* CmdBuffer = CmdBuffers[Index];
			CmdBuffer->RefreshFenceStatus();
			if (CmdBuffer->State == FVulkanCmdBuffer::EState::ReadyForBegin)
			{
				UploadCmdBuffer = CmdBuffer;
				UploadCmdBuffer->Begin();
				return UploadCmdBuffer;
			}
		}

		// All cmd buffers are being executed still
		UploadCmdBuffer = Create();
		UploadCmdBuffer->Begin();
	}

	return UploadCmdBuffer;
}
