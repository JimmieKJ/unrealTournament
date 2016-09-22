// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "VulkanRHIPrivate.h"
#include "VulkanContext.h"

FVulkanShaderResourceView::~FVulkanShaderResourceView()
{
	TextureView.Destroy(*Device);
	BufferView = nullptr;
	SourceVertexBuffer = nullptr;
	SourceTexture = nullptr;
	Device = nullptr;
}

void FVulkanShaderResourceView::UpdateView()
{
	// update the buffer view for dynamic VB backed buffers (or if it was never set)
	if (SourceVertexBuffer != nullptr)
	{
		if (SourceVertexBuffer->IsVolatile() && VolatileLockCounter != SourceVertexBuffer->GetVolatileLockCounter())
		{
			BufferView = nullptr;
			VolatileLockCounter = SourceVertexBuffer->GetVolatileLockCounter();
		}

		if (BufferView == nullptr || SourceVertexBuffer->IsDynamic())
		{
			SCOPE_CYCLE_COUNTER(STAT_VulkanSRVUpdateTime);
			// thanks to ref counting, overwriting the buffer will toss the old view
			BufferView = new FVulkanBufferView(Device);
			BufferView->Create(SourceVertexBuffer.GetReference(), BufferViewFormat, SourceVertexBuffer->GetOffset(), SourceVertexBuffer->GetSize());
		}
	}
	else
	{
		if (TextureView.View == VK_NULL_HANDLE)
		{
			EPixelFormat Format = (BufferViewFormat == PF_Unknown) ? SourceTexture->GetFormat() : BufferViewFormat;
			if (FRHITexture2D* Tex2D = SourceTexture->GetTexture2D())
			{
				FVulkanTexture2D* VTex2D = ResourceCast(Tex2D);
				TextureView.Create(*Device, VTex2D->Surface.Image, VK_IMAGE_VIEW_TYPE_2D, VTex2D->Surface.GetPartialAspectMask(), Format, UEToVkFormat(Format, false), MipLevel, NumMips, 0, 1);
			}
			else
			{
				ensure(0);
			}
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
	FVulkanTextureBase* Base = nullptr;
	if (auto* Tex2D = TextureRHI->GetTexture2D())
	{
		Base = ResourceCast(Tex2D);
	}
	else if (auto* Tex3D = TextureRHI->GetTexture3D())
	{
		Base = ResourceCast(Tex3D);
	}
	else if (auto* TexCube = TextureRHI->GetTextureCube())
	{
		Base = ResourceCast(TexCube);
	}
	else
	{
		ensure(0);
	}
#endif
	FVulkanUnorderedAccessView* UAV = new FVulkanUnorderedAccessView;
	UAV->SourceTexture = TextureRHI;
	UAV->MipIndex = MipLevel;
	return UAV;
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
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView(Device);
	// delay the shader view create until we use it, so we just track the source info here
	SRV->SourceVertexBuffer = ResourceCast(VertexBufferRHI);
	SRV->BufferViewFormat = (EPixelFormat)Format;
	return SRV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
{
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView(Device);
	// delay the shader view create until we use it, so we just track the source info here
	SRV->SourceTexture = ResourceCast(Texture2DRHI);
	SRV->MipLevel = MipLevel;
	SRV->NumMips = 1;
	return SRV;
}

FShaderResourceViewRHIRef FVulkanDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
{
	FVulkanShaderResourceView* SRV = new FVulkanShaderResourceView(Device);
	// delay the shader view create until we use it, so we just track the source info here
	SRV->SourceTexture = ResourceCast(Texture2DRHI);
	SRV->BufferViewFormat = (EPixelFormat)Format;
	SRV->MipLevel = MipLevel;
	SRV->NumMips = NumMipLevels;
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

