// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EmptyRHIPrivate.h"

FEmptyShaderResourceView::~FEmptyShaderResourceView()
{
	SourceVertexBuffer = NULL;
	SourceTexture = NULL;
}



FUnorderedAccessViewRHIRef FEmptyDynamicRHI::RHICreateUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBufferRHI, bool bUseUAVCounter, bool bAppendBuffer)
{
	DYNAMIC_CAST_EMPTYRESOURCE(StructuredBuffer,StructuredBuffer);

	// create the UAV buffer to point to the structured buffer's memory
	FEmptyUnorderedAccessView* UAV = new FEmptyUnorderedAccessView;
	UAV->SourceStructuredBuffer = StructuredBuffer;

	return UAV;
}

FUnorderedAccessViewRHIRef FEmptyDynamicRHI::RHICreateUnorderedAccessView(FTextureRHIParamRef TextureRHI)
{
	FEmptySurface& Surface = GetEmptySurfaceFromRHITexture(TextureRHI);

	// create the UAV buffer to point to the structured buffer's memory
	FEmptyUnorderedAccessView* UAV = new FEmptyUnorderedAccessView;
	UAV->SourceTexture = (FRHITexture*)TextureRHI;

	return UAV;
}

FUnorderedAccessViewRHIRef FEmptyDynamicRHI::RHICreateUnorderedAccessView(FVertexBufferRHIParamRef VertexBufferRHI, uint8 Format)
{
	DYNAMIC_CAST_EMPTYRESOURCE(VertexBuffer, VertexBuffer);

	// create the UAV buffer to point to the structured buffer's memory
	FEmptyUnorderedAccessView* UAV = new FEmptyUnorderedAccessView;
	UAV->SourceVertexBuffer = VertexBuffer;

	return UAV;
}

FShaderResourceViewRHIRef FEmptyDynamicRHI::RHICreateShaderResourceView(FStructuredBufferRHIParamRef StructuredBufferRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(StructuredBuffer, StructuredBuffer);

	FEmptyShaderResourceView* SRV = new FEmptyShaderResourceView;
	return SRV;
}

FShaderResourceViewRHIRef FEmptyDynamicRHI::RHICreateShaderResourceView(FVertexBufferRHIParamRef VertexBufferRHI, uint32 Stride, uint8 Format)
{
	DYNAMIC_CAST_EMPTYRESOURCE(VertexBuffer, VertexBuffer);

	FEmptyShaderResourceView* SRV = new FEmptyShaderResourceView;
	SRV->SourceVertexBuffer = VertexBuffer;
	return SRV;
}

FShaderResourceViewRHIRef FEmptyDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
{
	FEmptyShaderResourceView* SRV = new FEmptyShaderResourceView;
	SRV->SourceTexture = (FRHITexture*)Texture2DRHI;
	return SRV;
}

FShaderResourceViewRHIRef FEmptyDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
{
	FEmptyShaderResourceView* SRV = new FEmptyShaderResourceView;
	SRV->SourceTexture = (FRHITexture*)Texture2DRHI;
	return SRV;
}

void FEmptyDynamicRHI::RHIClearUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32* Values)
{
	DYNAMIC_CAST_EMPTYRESOURCE(UnorderedAccessView, UnorderedAccessView);

}


