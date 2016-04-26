// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanCommandBuffer.cpp: Vulkan device RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanCommandBuffer.h"

FVulkanCmdBuffer::FVulkanCmdBuffer(FVulkanDevice* InDevice, FVulkanCommandBufferManager* InCommandBufferManager) :
	Device(InDevice),
	CommandBufferManager(InCommandBufferManager),
	CommandBufferHandle(VK_NULL_HANDLE),
	IsWriting(VK_FALSE),
	IsEmpty(VK_TRUE)
{
	check(Device);

	VkCommandBufferAllocateInfo CreateCmdBufInfo;
	FMemory::Memzero(CreateCmdBufInfo);
	CreateCmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	CreateCmdBufInfo.pNext = nullptr;
	CreateCmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	CreateCmdBufInfo.commandBufferCount = 1;
	CreateCmdBufInfo.commandPool = CommandBufferManager->GetHandle();

	VERIFYVULKANRESULT(vkAllocateCommandBuffers(Device->GetInstanceHandle(), &CreateCmdBufInfo, &CommandBufferHandle));
}

FVulkanCmdBuffer::~FVulkanCmdBuffer()
{
	check(IsWriting == VK_FALSE);
	if (CommandBufferHandle != VK_NULL_HANDLE)
	{
		vkFreeCommandBuffers(Device->GetInstanceHandle(), CommandBufferManager->GetHandle(), 1, &CommandBufferHandle);
		CommandBufferHandle = VK_NULL_HANDLE;
	}
}

VkCommandBuffer& FVulkanCmdBuffer::GetHandle(const VkBool32 WritingToCommandBuffer /* = true */)
{
	check(CommandBufferHandle != VK_NULL_HANDLE);
	IsEmpty &= !WritingToCommandBuffer;
	return CommandBufferHandle;
}

void FVulkanCmdBuffer::Begin()
{
	check(!IsWriting);
	check(IsEmpty);

	VkCommandBufferBeginInfo CmdBufBeginInfo;
	FMemory::Memzero(CmdBufBeginInfo);
	CmdBufBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	CmdBufBeginInfo.pNext = nullptr;
	CmdBufBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	CmdBufBeginInfo.pInheritanceInfo = nullptr;

	VERIFYVULKANRESULT(vkBeginCommandBuffer(GetHandle(false), &CmdBufBeginInfo));

	IsWriting = VK_TRUE;
}

VkBool32 FVulkanCmdBuffer::GetIsWriting() const
{
	return IsWriting;
}

VkBool32 FVulkanCmdBuffer::GetIsEmpty() const
{
	return IsEmpty;
}

void FVulkanCmdBuffer::Reset()
{
	check(!IsWriting)

	VERIFYVULKANRESULT(vkResetCommandBuffer(GetHandle(), VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));

	IsEmpty = VK_TRUE;
}

void FVulkanCmdBuffer::End()
{
	check(IsWriting)

	VERIFYVULKANRESULT(vkEndCommandBuffer(GetHandle()));

	IsWriting = VK_FALSE;
	IsEmpty = VK_TRUE;
}

FVulkanCommandBufferManager::FVulkanCommandBufferManager(FVulkanDevice* InDevice):
	Device(InDevice),
	Handle(VK_NULL_HANDLE)
{
	check(Device);

	VkCommandPoolCreateInfo CmdPoolInfo;
	FMemory::Memzero(CmdPoolInfo);
	CmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	CmdPoolInfo.pNext = nullptr;
	CmdPoolInfo.queueFamilyIndex = Device->GetQueue()->GetNodeIndex();
	CmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VERIFYVULKANRESULT(vkCreateCommandPool(Device->GetInstanceHandle(), &CmdPoolInfo, nullptr, &Handle));
}

FVulkanCommandBufferManager::~FVulkanCommandBufferManager()
{
	check(Device);
	check(Handle != VK_NULL_HANDLE);
	vkDestroyCommandPool(Device->GetInstanceHandle(), Handle, nullptr);
}

FVulkanCmdBuffer* FVulkanCommandBufferManager::Create()
{
	check(Device);
	FVulkanCmdBuffer* CmdBuffer = new FVulkanCmdBuffer(Device, this);
	CmdBuffers.Add(CmdBuffer);
	check(CmdBuffer);
	return CmdBuffer;
}

void FVulkanCommandBufferManager::Destroy(FVulkanCmdBuffer* CmdBuffer)
{
	CmdBuffers.Remove(CmdBuffer);
	check(Device);
	if (CmdBuffer->GetIsWriting())
	{
		CmdBuffer->End();
	}
	vkFreeCommandBuffers(Device->GetInstanceHandle(), GetHandle(), 1, &CmdBuffer->CommandBufferHandle);
	CmdBuffer->CommandBufferHandle = VK_NULL_HANDLE;
	delete CmdBuffer;
}

void FVulkanCommandBufferManager::Submit(FVulkanCmdBuffer* CmdBuffer)
{
	Device->GetQueue()->Submit(CmdBuffer);
}
