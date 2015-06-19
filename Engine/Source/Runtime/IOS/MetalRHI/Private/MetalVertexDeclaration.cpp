// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalVertexDeclaration.cpp: Metal vertex declaration RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"

static MTLVertexFormat TranslateElementTypeToMTLType(EVertexElementType Type)
{
	switch (Type)
	{
		case VET_Float1:			return MTLVertexFormatFloat;
		case VET_Float2:			return MTLVertexFormatFloat2;
		case VET_Float3:			return MTLVertexFormatFloat3;
		case VET_Float4:			return MTLVertexFormatFloat4;
		case VET_PackedNormal:	return MTLVertexFormatUChar4Normalized;
		case VET_UByte4:			return MTLVertexFormatUChar4;
		case VET_UByte4N:		return MTLVertexFormatUChar4Normalized;
		case VET_Color:			return MTLVertexFormatUChar4Normalized;
		case VET_Short2:			return MTLVertexFormatShort2;
		case VET_Short4:			return MTLVertexFormatShort4;
		case VET_Short2N:		return MTLVertexFormatShort2Normalized;
		case VET_Half2:			return MTLVertexFormatHalf2;
		case VET_Half4:			return MTLVertexFormatHalf4;
		case VET_Short4N:		return MTLVertexFormatShort4Normalized;
		case VET_UShort2:		return MTLVertexFormatUShort2;
		case VET_UShort4:		return MTLVertexFormatUShort4;
		case VET_UShort2N:		return MTLVertexFormatUShort2Normalized;
		case VET_UShort4N:		return MTLVertexFormatUShort4Normalized;
		default:				UE_LOG(LogMetal, Fatal, TEXT("Unknown vertex element type!")); return MTLVertexFormatFloat;
	};

}

static uint32 TranslateElementTypeToSize(EVertexElementType Type)
{
	switch (Type)
	{
		case VET_Float1:			return 4;
		case VET_Float2:			return 8;
		case VET_Float3:			return 12;
		case VET_Float4:			return 16;
		case VET_PackedNormal:	return 4;
		case VET_UByte4:			return 4;
		case VET_UByte4N:		return 4;
		case VET_Color:			return 4;
		case VET_Short2:			return 4;
		case VET_Short4:			return 8;
		case VET_Short2N:		return 4;
		case VET_Half2:			return 4;
		case VET_Half4:			return 8;
		case VET_Short4N:		return 8;
		default:				UE_LOG(LogMetal, Fatal, TEXT("Unknown vertex element type!")); return 0;
	};
}

FMetalVertexDeclaration::FMetalVertexDeclaration(const FVertexDeclarationElementList& InElements)
	: Elements(InElements)
	, Layout(nil)
{
	GenerateLayout(InElements);
}

FMetalVertexDeclaration::~FMetalVertexDeclaration()
{
	FMetalManager::ReleaseObject(Layout);
}

static TMap<uint32, FVertexDeclarationRHIRef> GVertexDeclarationCache;

FVertexDeclarationRHIRef FMetalDynamicRHI::RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	uint32 Key = FCrc::MemCrc32(Elements.GetData(), Elements.Num() * sizeof(FVertexElement));
	// look up an existing declaration
	FVertexDeclarationRHIRef* VertexDeclarationRefPtr = GVertexDeclarationCache.Find(Key);
	if (VertexDeclarationRefPtr == NULL)
	{
//		NSLog(@"VertDecl Key: %x", Key);

		// create and add to the cache if it doesn't exist.
		VertexDeclarationRefPtr = &GVertexDeclarationCache.Add(Key, new FMetalVertexDeclaration(Elements));
	}

	return *VertexDeclarationRefPtr;
}


void FMetalVertexDeclaration::GenerateLayout(const FVertexDeclarationElementList& InElements)
{
	Layout = [[MTLVertexDescriptor alloc] init];
	TRACK_OBJECT(Layout);

	TMap<uint32, uint32> BufferStrides;
	for (uint32 ElementIndex = 0; ElementIndex < InElements.Num(); ElementIndex++)
	{
		const FVertexElement& Element = InElements[ElementIndex];
		
		checkf(Element.Stride == 0 || Element.Offset + TranslateElementTypeToSize(Element.Type) <= Element.Stride, 
			TEXT("Stream component is bigger than stride: Offset: %d, Size: %d [Type %d], Stride: %d"), Element.Offset, TranslateElementTypeToSize(Element.Type), (uint32)Element.Type, Element.Stride);

		// Vertex & Constant buffers are set up in the same space, so add VB's from the top
		uint32 ShaderBufferIndex = UNREAL_TO_METAL_BUFFER_INDEX(Element.StreamIndex);

		// track the buffer stride, making sure all elements with the same buffer have the same stride
		uint32* ExistingStride = BufferStrides.Find(ShaderBufferIndex);
		if (ExistingStride == NULL)
		{
			// handle 0 stride buffers
			MTLVertexStepFunction Function = (Element.Stride == 0 ? MTLVertexStepFunctionConstant : (Element.bUseInstanceIndex ? MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex));
			uint32 StepRate = (Element.Stride == 0 ? 0 : 1);
			// even with MTLVertexStepFunctionConstant, it needs a non-zero stride (not sure why)
			uint32 Stride = (Element.Stride == 0 ? 4 : Element.Stride);

			// look for any unset strides coming from UE4 (this can be removed when all are fixed)
			if (Element.Stride == 0xFFFF)
			{
				NSLog(@"Setting illegal stride - break here if you want to find out why, but this won't break until we try to render with it");
				Stride = 200;
			}

			// set the stride once per buffer
			Layout.layouts[ShaderBufferIndex].stride = Stride;
			Layout.layouts[ShaderBufferIndex].stepFunction = Function;
			Layout.layouts[ShaderBufferIndex].stepRate = StepRate;

			// track this buffer and stride
			BufferStrides.Add(ShaderBufferIndex, Element.Stride);
		}
		else
		{
			// if the strides of elements with same buffer index have different strides, something is VERY wrong
			check(Element.Stride == *ExistingStride);
		}

		// set the format for each element
		Layout.attributes[Element.AttributeIndex].format = TranslateElementTypeToMTLType(Element.Type);
		Layout.attributes[Element.AttributeIndex].offset = Element.Offset;
		Layout.attributes[Element.AttributeIndex].bufferIndex = ShaderBufferIndex;
	}
}
