// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#include "MetalRenderPipelineDesc.h"
#include "MetalProfiler.h"

static_assert(Offset_End <= sizeof(FMetalRenderPipelineHash) * 8, "Too many bits used for the Hash");

NSMutableDictionary* FMetalRenderPipelineDesc::MetalPipelineCache = [NSMutableDictionary new];
uint32 FMetalRenderPipelineDesc::BlendBitOffsets[] = { Offset_BlendState0, Offset_BlendState1, Offset_BlendState2, Offset_BlendState3, Offset_BlendState4, Offset_BlendState5, };
uint32 FMetalRenderPipelineDesc::RTBitOffsets[] = { Offset_RenderTargetFormat0, Offset_RenderTargetFormat1, Offset_RenderTargetFormat2, Offset_RenderTargetFormat3, Offset_RenderTargetFormat4, Offset_RenderTargetFormat5, };

FMetalRenderPipelineDesc::FMetalRenderPipelineDesc()
: PipelineDescriptor([[MTLRenderPipelineDescriptor alloc] init])
, SampleCount(0)
, Hash(0)
{
	for (int Index = 0; Index < MaxMetalRenderTargets; Index++)
	{
		[PipelineDescriptor.colorAttachments setObject:[[MTLRenderPipelineColorAttachmentDescriptor new] autorelease] atIndexedSubscript:Index];
	}
}

id<MTLRenderPipelineState> FMetalRenderPipelineDesc::CreatePipelineStateForBoundShaderState(FMetalBoundShaderState* BSS) const
{
	SCOPE_CYCLE_COUNTER(STAT_MetalPipelineStateTime);
	
	MTLPixelFormat DepthFormat = PipelineDescriptor.depthAttachmentPixelFormat;
	MTLPixelFormat StencilFormat = PipelineDescriptor.stencilAttachmentPixelFormat;
	if(BSS->PixelShader && (BSS->PixelShader->Bindings.InOutMask & 0x8000) && PipelineDescriptor.depthAttachmentPixelFormat == MTLPixelFormatInvalid)
	{
		PipelineDescriptor.depthAttachmentPixelFormat = (MTLPixelFormat)GPixelFormats[PF_DepthStencil].PlatformFormat;
		PipelineDescriptor.stencilAttachmentPixelFormat = (MTLPixelFormat)GPixelFormats[PF_DepthStencil].PlatformFormat;
	}
	
	// set the bound shader state settings
	PipelineDescriptor.vertexDescriptor = BSS->VertexDeclaration->Layout;
	PipelineDescriptor.vertexFunction = BSS->VertexShader->Function;
	PipelineDescriptor.fragmentFunction = BSS->PixelShader ? BSS->PixelShader->Function : nil;
	PipelineDescriptor.sampleCount = SampleCount;

	check(SampleCount > 0);

	NSError* Error = nil;
	id<MTLRenderPipelineState> PipelineState = (id<MTLRenderPipelineState>)[MetalPipelineCache objectForKey:PipelineDescriptor];
	if(PipelineState == nil)
	{
		id<MTLDevice> Device = GetMetalDeviceContext().GetDevice();
		PipelineState = [Device newRenderPipelineStateWithDescriptor:PipelineDescriptor error : &Error];
		TRACK_OBJECT(PipelineState);
		if(PipelineState)
		{
			[MetalPipelineCache setObject:PipelineState forKey:PipelineDescriptor];
		}
	}
	
	PipelineDescriptor.depthAttachmentPixelFormat = DepthFormat;
	PipelineDescriptor.stencilAttachmentPixelFormat = StencilFormat;
	
	if (PipelineState == nil)
	{
		NSLog(@"Failed to generate a pipeline state object: %@", Error);
		NSLog(@"Vertex shader: %@", BSS->VertexShader->GlslCodeNSString);
		NSLog(@"Pixel shader: %@", BSS->PixelShader->GlslCodeNSString);
		NSLog(@"Descriptor: %@", PipelineDescriptor);
		return nil;
	}

	return PipelineState;
}
