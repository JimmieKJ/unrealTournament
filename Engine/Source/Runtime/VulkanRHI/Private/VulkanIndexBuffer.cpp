// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanIndexBuffer.cpp: Vulkan Index buffer RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanDevice.h"
#include "VulkanContext.h"

static TMap<FVulkanResourceMultiBuffer*, VulkanRHI::FPendingBufferLock> GPendingLockIBs;
static FCriticalSection GPendingLockIBsMutex;

FVulkanResourceMultiBuffer::FVulkanResourceMultiBuffer(FVulkanDevice* InDevice, VkBufferUsageFlags InBufferUsageFlags, uint32 InSize, uint32 InUEUsage, FRHIResourceCreateInfo& CreateInfo)
	: VulkanRHI::FDeviceChild(InDevice)
	, UEUsage(InUEUsage)
	, BufferUsageFlags(InBufferUsageFlags)
	, NumBuffers(0)
	, DynamicBufferIndex(0)
{
	if (InSize > 0)
	{
		const bool bStatic = (InUEUsage & BUF_Static) != 0;
		const bool bDynamic = (InUEUsage & BUF_Dynamic) != 0;
		const bool bVolatile = (InUEUsage & BUF_Volatile) != 0;
		const bool bShaderResource = (InUEUsage & BUF_ShaderResource) != 0;
		const bool bIsUniformBuffer = (InBufferUsageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) != 0;

		BufferUsageFlags |= bVolatile ? 0 : VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		BufferUsageFlags |= (bShaderResource && !bIsUniformBuffer) ? VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT : 0;

		if (!bVolatile)
		{
			check(bStatic || bDynamic);
			VkDevice VulkanDevice = InDevice->GetInstanceHandle();

			NumBuffers = bDynamic ? NUM_RENDER_BUFFERS : 1;

			Buffers.AddDefaulted(NumBuffers);
			for (uint32 Index = 0; Index < NumBuffers; ++Index)
			{
				Buffers[Index] = InDevice->GetResourceHeapManager().AllocateBuffer(InSize, BufferUsageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, __FILE__, __LINE__);
			}

			if (CreateInfo.ResourceArray)
			{
				ensure(bStatic);
				uint32 CopyDataSize = FMath::Min(InSize, CreateInfo.ResourceArray->GetResourceDataSize());
				void* Data = Lock(RLM_WriteOnly, CopyDataSize, 0);
				FMemory::Memcpy(Data, CreateInfo.ResourceArray->GetResourceData(), CopyDataSize);
				Unlock();

				CreateInfo.ResourceArray->Discard();
			}
		}
	}
}

FVulkanResourceMultiBuffer::~FVulkanResourceMultiBuffer()
{
	//#todo-rco: Free VkBuffers
}

void* FVulkanResourceMultiBuffer::Lock(EResourceLockMode LockMode, uint32 Size, uint32 Offset)
{
	void* Data = nullptr;

	const bool bStatic = (UEUsage & BUF_Static) != 0;
	const bool bDynamic = (UEUsage & BUF_Dynamic) != 0;
	const bool bVolatile = (UEUsage & BUF_Volatile) != 0;
	const bool bCPUReadable = (UEUsage & BUF_KeepCPUAccessible) != 0;

	if (bVolatile)
	{
		check(NumBuffers == 0);
		if (LockMode == RLM_ReadOnly)
		{
			ensure(0);
		}
		else
		{
			bool bResult = Device->GetImmediateContext().GetTempFrameAllocationBuffer().Alloc(Size + Offset, 256, VolatileLockInfo);
			Data = VolatileLockInfo.Data;
			check(bResult);
			++VolatileLockInfo.LockCounter;
		}
	}
	else
	{
		check(bStatic || bDynamic);

		VulkanRHI::FPendingBufferLock PendingLock;
		PendingLock.Offset = Offset;
		PendingLock.Size = Size;
		PendingLock.LockMode = LockMode;

		if (LockMode == RLM_ReadOnly)
		{
			ensure(0);
		}
		else
		{
			check(LockMode == RLM_WriteOnly);
			DynamicBufferIndex = (DynamicBufferIndex + 1) % NumBuffers;

			VulkanRHI::FStagingBuffer* StagingBuffer = Device->GetStagingManager().AcquireBuffer(Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

			PendingLock.StagingBuffer = StagingBuffer;

			Data = StagingBuffer->GetMappedPointer();
		}

		{
			FScopeLock ScopeLock(&GPendingLockIBsMutex);
			check(!GPendingLockIBs.Contains(this));
			GPendingLockIBs.Add(this, PendingLock);
		}
	}

	check(Data);
	return Data;
}

void FVulkanResourceMultiBuffer::Unlock()
{
	const bool bStatic = (UEUsage & BUF_Static) != 0;
	const bool bDynamic = (UEUsage & BUF_Dynamic) != 0;
	const bool bVolatile = (UEUsage & BUF_Volatile) != 0;
	const bool bCPUReadable = (UEUsage & BUF_KeepCPUAccessible) != 0;

	if (bVolatile)
	{
		check(NumBuffers == 0);

		// Nothing to do here...
	}
	else
	{
		check(bStatic || bDynamic);

		VulkanRHI::FPendingBufferLock PendingLock;
		bool bFound = false;
		{
			// Found only if it was created for Write
			FScopeLock ScopeLock(&GPendingLockIBsMutex);
			bFound = GPendingLockIBs.RemoveAndCopyValue(this, PendingLock);
		}

		checkf(bFound, TEXT("Mismatched lock/unlock IndexBuffer!"));
		if (PendingLock.LockMode == RLM_WriteOnly)
		{
			uint32 LockSize = PendingLock.Size;
			uint32 LockOffset = PendingLock.Offset;
			VulkanRHI::FStagingBuffer* StagingBuffer = PendingLock.StagingBuffer;
			PendingLock.StagingBuffer = nullptr;

			FVulkanCmdBuffer* Cmd = Device->GetImmediateContext().GetCommandBufferManager()->GetUploadCmdBuffer();
			ensure(Cmd->IsOutsideRenderPass());
			VkCommandBuffer CmdBuffer = Cmd->GetHandle();

			VkBufferCopy Region;
			FMemory::Memzero(Region);
			Region.size = LockSize;
			//Region.srcOffset = 0;
			Region.dstOffset = LockOffset + Buffers[DynamicBufferIndex]->GetOffset();
			VulkanRHI::vkCmdCopyBuffer(CmdBuffer, StagingBuffer->GetHandle(), Buffers[DynamicBufferIndex]->GetHandle(), 1, &Region);
			//UpdateBuffer(ResourceAllocation, IndexBuffer->GetBuffer(), LockSize, LockOffset);

			//Device->GetDeferredDeletionQueue().EnqueueResource(Cmd, StagingBuffer);
			Device->GetStagingManager().ReleaseBuffer(Cmd, StagingBuffer);
		}
	}
}


FVulkanIndexBuffer::FVulkanIndexBuffer(FVulkanDevice* InDevice, uint32 InStride, uint32 InSize, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
	: FRHIIndexBuffer(InStride, InSize, InUsage)
	, FVulkanResourceMultiBuffer(InDevice, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, InSize, InUsage, CreateInfo)
	, IndexType(InStride == 4 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16)
{
}


FIndexBufferRHIRef FVulkanDynamicRHI::RHICreateIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	return new FVulkanIndexBuffer(Device, Stride, Size, InUsage, CreateInfo);
}

void* FVulkanDynamicRHI::RHILockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	FVulkanIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	return IndexBuffer->Lock(LockMode, Size, Offset);
}

void FVulkanDynamicRHI::RHIUnlockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI)
{
	FVulkanIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	IndexBuffer->Unlock();
}
