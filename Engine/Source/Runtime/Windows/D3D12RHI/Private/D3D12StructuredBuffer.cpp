// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"

D3D12_RESOURCE_DESC CreateStructuredBufferResourceDesc(uint32 Size, uint32 InUsage)
{
	// Describe the structured buffer.
	D3D12_RESOURCE_DESC Desc = CD3DX12_RESOURCE_DESC::Buffer(Size);

	if ((InUsage & BUF_ShaderResource) == 0)
	{
		Desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}

	if (InUsage & BUF_UnorderedAccess)
	{
		Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	if (FPlatformProperties::SupportsFastVRAMMemory())
	{
		if (InUsage & BUF_FastVRAM)
		{
			FFastVRAMAllocator::GetFastVRAMAllocator()->AllocUAVBuffer(Desc);
		}
	}

	return Desc;
}

FStructuredBufferRHIRef FD3D12DynamicRHI::RHICreateStructuredBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	// Check for values that will cause D3D calls to fail
	check(Size / Stride > 0 && Size % Stride == 0);

	const D3D12_RESOURCE_DESC Desc = CreateStructuredBufferResourceDesc(Size, InUsage);

	// Structured buffers, non-byte address buffers, need to be aligned to their stride to ensure that they
	// can be addressed correctly with element based offsets.
	const uint32 Alignment = ((InUsage & (BUF_ByteAddressBuffer | BUF_DrawIndirect)) == 0) ? Stride : 4;

	FD3D12ResourceLocation* ResourceLocation = CreateBuffer(nullptr, Desc, Size, InUsage, CreateInfo, Alignment);
	FD3D12StructuredBuffer* NewBuffer = new FD3D12StructuredBuffer(GetRHIDevice(), ResourceLocation, Stride, Size, InUsage);
	UpdateBufferStats(NewBuffer->ResourceLocation, true, D3D12_BUFFER_TYPE_STRUCTURED);
	NewBuffer->BufferAlignment = Alignment;
	return NewBuffer;
}

FD3D12StructuredBuffer::~FD3D12StructuredBuffer()
{
	UpdateBufferStats(ResourceLocation, false, D3D12_BUFFER_TYPE_STRUCTURED);

	// Dynamic resources have a permanent entry in the OutstandingLocks that needs to be cleaned up. 
	if (GetUsage() & BUF_AnyDynamic)
	{
		FD3D12LockedKey LockedKey(this);
		FD3D12DynamicRHI::GetD3DRHI()->RemoveFromOutstandingLocks(LockedKey);
	}
}

void FD3D12StructuredBuffer::Rename(FD3D12ResourceLocation* NewResource)
{
	FD3D12CommandContext& DefaultContext = GetParentDevice()->GetDefaultCommandContext();

	// If this resource is bound to the device, unbind it
	DefaultContext.ConditionalClearShaderResource(ResourceLocation);

	ResourceLocation = NewResource;
}

void* FD3D12DynamicRHI::RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	return LockBuffer(nullptr, FD3D12DynamicRHI::ResourceCast(StructuredBufferRHI), Offset, Size, LockMode);
}

void FD3D12DynamicRHI::RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	UnlockBuffer(nullptr, FD3D12DynamicRHI::ResourceCast(StructuredBufferRHI));
}
