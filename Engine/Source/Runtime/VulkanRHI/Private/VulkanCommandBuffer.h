// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanCommandBuffer.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanConfiguration.h"

class FVulkanDevice;
class FVulkanCommandBufferManager;

namespace VulkanRHI
{
	class FFence;
}

class FVulkanCmdBuffer
{
protected:
	friend class FVulkanCommandBufferManager;
	friend class FVulkanQueue;

	FVulkanCmdBuffer(FVulkanDevice* InDevice, FVulkanCommandBufferManager* InCommandBufferManager);
	~FVulkanCmdBuffer();

public:
	FVulkanCommandBufferManager* GetOwner()
	{
		return CommandBufferManager;
	}

	inline bool IsInsideRenderPass() const
	{
		return State == EState::IsInsideRenderPass;
	}

	inline bool IsOutsideRenderPass() const
	{
		return State == EState::IsInsideBegin;
	}

	inline bool HasEnded() const
	{
		return State == EState::HasEnded;
	}

	inline bool IsSubmitted() const
	{
		return State == EState::Submitted;
	}

	inline VkCommandBuffer GetHandle()
	{
		return CommandBufferHandle;
	}

	void BeginRenderPass(const FVulkanRenderTargetLayout& Layout, VkRenderPass RenderPass, VkFramebuffer Framebuffer, const VkClearValue* AttachmentClearValues);

	void EndRenderPass()
	{
		check(IsInsideRenderPass());
		VulkanRHI::vkCmdEndRenderPass(CommandBufferHandle);
		State = EState::IsInsideBegin;
	}

	void End()
	{
		check(IsOutsideRenderPass());
		VERIFYVULKANRESULT(VulkanRHI::vkEndCommandBuffer(GetHandle()));
		State = EState::HasEnded;
	}

	inline uint64 GetFenceSignaledCounter() const
	{
		return FenceSignaledCounter;
	}

	void Begin();

	enum class EState
	{
		ReadyForBegin,
		IsInsideBegin,
		IsInsideRenderPass,
		HasEnded,
		Submitted,
	};

private:
	FVulkanDevice* Device;
	VkCommandBuffer CommandBufferHandle;
	EState State;

	// Do not cache this pointer as it might change depending on VULKAN_REUSE_FENCES
	VulkanRHI::FFence* Fence;

	uint64 FenceSignaledCounter;

	void RefreshFenceStatus();

	FVulkanCommandBufferManager* CommandBufferManager;
};

class FVulkanCommandBufferManager
{
public:
	FVulkanCommandBufferManager(FVulkanDevice* InDevice);

	~FVulkanCommandBufferManager();

	FVulkanCmdBuffer* GetActiveCmdBuffer();

	FVulkanCmdBuffer* GetUploadCmdBuffer();

	void RefreshFenceStatus();
	void PrepareForNewActiveCommandBuffer();

	inline VkCommandPool GetHandle() const
	{
		check(Handle != VK_NULL_HANDLE);
		return Handle;
	}

private:
	FVulkanDevice* Device;
	VkCommandPool Handle;
	FVulkanCmdBuffer* ActiveCmdBuffer;
	FVulkanCmdBuffer* UploadCmdBuffer;

	FVulkanCmdBuffer* Create();

	TArray<FVulkanCmdBuffer*> CmdBuffers;
};
