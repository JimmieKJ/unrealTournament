// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanCommandBuffer.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanRHIPrivate.h"

struct FVulkanDevice;

class FVulkanCmdBuffer
{
	friend struct FVulkanCommandBufferManager;
	friend struct FVulkanQueue;

	FVulkanCmdBuffer(FVulkanDevice* InDevice, FVulkanCommandBufferManager* InCommandBufferManager);

	~FVulkanCmdBuffer();

public:
	VkCommandBuffer& GetHandle(const VkBool32 MakeDirty = true);

	void Begin();

	void Reset();

	void End();

	VkBool32 GetIsWriting() const;
	VkBool32 GetIsEmpty() const;

private:
	FVulkanDevice* Device;
	VkCommandBuffer CommandBufferHandle;
	VkBool32 IsWriting;
	VkBool32 IsEmpty;

	FVulkanCommandBufferManager* CommandBufferManager;
};

struct FVulkanCommandBufferManager
{
	FVulkanCommandBufferManager(FVulkanDevice* InDevice);

	~FVulkanCommandBufferManager();

	FVulkanCmdBuffer* Create();

	void Destroy(FVulkanCmdBuffer* CmdBuffer);

	void Submit(FVulkanCmdBuffer* CmdBuffer);

private:
	friend class FVulkanCmdBuffer;
	friend class FVulkanDynamicRHI;

	inline VkCommandPool GetHandle() const
	{
		check(Handle != VK_NULL_HANDLE);
		return Handle;
	}

private:
	FVulkanDevice* Device;
	VkCommandPool Handle;
	TArray<FVulkanCmdBuffer*> CmdBuffers;
};

