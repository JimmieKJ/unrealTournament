// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "VulkanRHIPrivate.h"

FVulkanShaderResourceView::~FVulkanShaderResourceView()
{
	BufferView = nullptr;
	SourceVertexBuffer = nullptr;
	SourceTexture = nullptr;
}

void FVulkanShaderResourceView::UpdateView(FVulkanDevice* Device)
{
	// update the buffer view for dynamic VB backed buffers (or if it was never set)
	if (SourceVertexBuffer != nullptr)
	{
		if (BufferView == nullptr || SourceVertexBuffer->IsDynamic())
		{
			SCOPE_CYCLE_COUNTER(STAT_VulkanSRVUpdateTime);
			// thanks to ref counting, overwriting the buffer will toss the old view
			BufferView = new FVulkanBufferView();
			check(Device);
			BufferView->Create(*Device, *SourceVertexBuffer->GetBuffer(), BufferViewFormat, SourceVertexBuffer->GetOffset(), SourceVertexBuffer->GetSize());
		}
	}
}




FUnorderedAccessViewRHIRef FVulkanDynamicRHI::RHICreateUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBufferRHI, bool bUseUAVCounter, bool bAppendBuffer)
{
#if 0
	FVulkanStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	// create the UAV buffer to point to the structured buffer's memory
	FVulkanUnorderedAccessView* UAV = new FVulkanUnorderedAccessView;
	UAV->SourceStructuredBuffer = StructuredBuffer;

	return UAV;
#else
	VULKAN_SIGNAL_UNIMPLEMENTED();
	return nullptr;
#endif
}

FUnorderedAccessViewRHIRef FVulkanDynamicRHI::RHICreateUnorderedAccessView(FTextureRHIParamRef TextureRHI, uint32 MipLevel)
{
#if 0
	FVulkanSurface& Surface = GetVulkanSurfaceFromRHITexture(TextureRHI);

	// create the UAV buffer to point to the structured buffer's memory
	FVulkanUnorderedAccessView* UAV = new FVulkanUnorderedAccessView;
	UAV->SourceTexture = (FRHITexture*)TextureRHI;

	return UAV;
#else
	VULKAN_SIGNAL_UNIMPLEMENTED();
	return nullptr;
#endif
}

FUnorderedAccessViewRHIRef FVulkanDynamicRHI::RHICreateUnorderedAccessView(FVertexBufferRHIParamRef VertexBufferRHI, uint8 Format)
{
	FVulkanVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

	// create the UAV buffer to point to the structured buffer's memory
	FVulkanUnorderedAccessView* UAV = new FVulkanUnorderedAccessView;
	UAV->SourceVertexBuffer = VertexBuffer;

	return UAV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
#if 0
	FVulkanStructuredBuffer* StructuredBuffer = ResourceCast(StructuredBufferRHI);

	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView;
	return SRV;
#else
	VULKAN_SIGNAL_UNIMPLEMENTED();
	return nullptr;
#endif
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FVertexBufferRHIParamRef VertexBufferRHI, uint32 Stride, uint8 Format)
{	
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView;
	// delay the shader view create until we use it, so we just track the source info here
	SRV->SourceVertexBuffer = ResourceCast(VertexBufferRHI);
	SRV->BufferViewFormat = (EPixelFormat)Format;
	return SRV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
{
#if 0
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView;
	SRV->SourceTexture = (FRHITexture*)Texture2DRHI;
	return SRV;
#else
	VULKAN_SIGNAL_UNIMPLEMENTED();
	return nullptr;
#endif
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
{
	//#todo-rco
	check(MipLevel == 0 && NumMipLevels == 1);
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView;
	// delay the shader view create until we use it, so we just track the source info here
	SRV->SourceTexture = ResourceCast(Texture2DRHI);
	SRV->BufferViewFormat = (EPixelFormat)Format;
	return SRV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel)
{
#if 0
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView;
	SRV->SourceTexture = (FRHITexture*)Texture3DRHI;
	return SRV;
#else
	VULKAN_SIGNAL_UNIMPLEMENTED();
	return nullptr;
#endif
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel)
{
#if 0
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView;
	SRV->SourceTexture = (FRHITexture*)Texture2DArrayRHI;
	return SRV;
#else
	VULKAN_SIGNAL_UNIMPLEMENTED();
	return nullptr;
#endif
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel)
{
#if 0
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView;
	SRV->SourceTexture = (FRHITexture*)TextureCubeRHI;
	return SRV;
#else
	VULKAN_SIGNAL_UNIMPLEMENTED();
	return nullptr;
#endif
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FIndexBufferRHIParamRef Buffer)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
	return nullptr;
}

void FVulkanCommandListContext::RHIClearUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32* Values)
{
#if 0
	FVulkanUnorderedAccessView* UnorderedAccessView = ResourceCast(UnorderedAccessViewRHI);
#endif
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

