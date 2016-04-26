// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12IndexBuffer.cpp: D3D Index buffer RHI implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"

D3D12_RESOURCE_DESC CreateIndexBufferResourceDesc(uint32 Size, uint32 InUsage)
{
	// Describe the vertex buffer.
	D3D12_RESOURCE_DESC Desc = CD3DX12_RESOURCE_DESC::Buffer(Size);

	if (InUsage & BUF_UnorderedAccess)
	{
		Desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	if ((InUsage & BUF_ShaderResource) == 0)
	{
		Desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}

	return Desc;
}

FD3D12IndexBuffer::~FD3D12IndexBuffer()
{
	UpdateBufferStats(ResourceLocation, false, D3D12_BUFFER_TYPE_INDEX);

	// Dynamic resources have a permanent entry in the OutstandingLocks that needs to be cleaned up. 
	if (GetUsage() & BUF_AnyDynamic)
	{
		FD3D12LockedKey LockedKey(this);
		FD3D12DynamicRHI::GetD3DRHI()->RemoveFromOutstandingLocks(LockedKey);
	}
}

void FD3D12IndexBuffer::Rename(FD3D12ResourceLocation* NewResource)
{
	FD3D12CommandContext& DefaultContext = GetParentDevice()->GetDefaultCommandContext();

	// If this resource is bound to the device, unbind it
	DefaultContext.ConditionalClearShaderResource(ResourceLocation);

	ResourceLocation = NewResource;
}

FIndexBufferRHIRef FD3D12DynamicRHI::RHICreateIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	const D3D12_RESOURCE_DESC Desc = CreateIndexBufferResourceDesc(Size, InUsage);
	const uint32 Alignment = 4;

	FD3D12ResourceLocation* ResourceLocation = CreateBuffer(nullptr, Desc, Size, InUsage, CreateInfo, Alignment);
	FD3D12IndexBuffer* NewBuffer = new FD3D12IndexBuffer(GetRHIDevice(), ResourceLocation, Stride, Size, InUsage);
	UpdateBufferStats(NewBuffer->ResourceLocation, true, D3D12_BUFFER_TYPE_INDEX);
	NewBuffer->BufferAlignment = Alignment;
	return NewBuffer;
}

void* FD3D12DynamicRHI::RHILockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	return LockBuffer(nullptr, FD3D12DynamicRHI::ResourceCast(IndexBufferRHI), Offset, Size, LockMode);
}

void FD3D12DynamicRHI::RHIUnlockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI)
{
	UnlockBuffer(nullptr, FD3D12DynamicRHI::ResourceCast(IndexBufferRHI));
}

FIndexBufferRHIRef FD3D12DynamicRHI::CreateIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	const D3D12_RESOURCE_DESC Desc = CreateIndexBufferResourceDesc(Size, InUsage);
	const uint32 Alignment = 4;

	FD3D12ResourceLocation* ResourceLocation = CreateBuffer(&RHICmdList, Desc, Size, InUsage, CreateInfo, Alignment);
	FD3D12IndexBuffer* NewBuffer = new FD3D12IndexBuffer(GetRHIDevice(), ResourceLocation, Stride, Size, InUsage);
	UpdateBufferStats(NewBuffer->ResourceLocation, true, D3D12_BUFFER_TYPE_INDEX);
	NewBuffer->BufferAlignment = Alignment;
	return NewBuffer;
}

void* FD3D12DynamicRHI::LockIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef IndexBufferRHI, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	return LockBuffer(&RHICmdList, FD3D12DynamicRHI::ResourceCast(IndexBufferRHI), Offset, SizeRHI, LockMode);
}

void FD3D12DynamicRHI::UnlockIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef IndexBufferRHI)
{
	// Pull down the above RHI implementation so that we can flush only when absolutely necessary
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicRHI_UnlockIndexBuffer_RenderThread);
	check(IsInRenderingThread());

	UnlockBuffer(&RHICmdList, FD3D12DynamicRHI::ResourceCast(IndexBufferRHI));
}
