// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#include "MetalRenderPipelineDesc.h"
#include "MetalProfiler.h"

static_assert(Offset_End <= sizeof(FMetalRenderPipelineHash) * 8, "Too many bits used for the Hash");

TMap<FMetalRenderPipelineDesc::FMetalRenderPipelineKey, id<MTLRenderPipelineState>> FMetalRenderPipelineDesc::MetalPipelineCache;
#if !UE_BUILD_SHIPPING
TMap<FMetalRenderPipelineDesc::FMetalRenderPipelineKey, MTLRenderPipelineReflection*> FMetalRenderPipelineDesc::MetalReflectionCache;
#endif
FCriticalSection FMetalRenderPipelineDesc::MetalPipelineMutex;

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

id<MTLRenderPipelineState> FMetalRenderPipelineDesc::CreatePipelineStateForBoundShaderState(FMetalBoundShaderState* BSS, FMetalHashedVertexDescriptor const& VertexDesc, MTLRenderPipelineReflection** Reflection) const
{
	SCOPE_CYCLE_COUNTER(STAT_MetalPipelineStateTime);
	
	MTLPixelFormat DepthFormat = PipelineDescriptor.depthAttachmentPixelFormat;
	MTLPixelFormat StencilFormat = PipelineDescriptor.stencilAttachmentPixelFormat;
	if(BSS->PixelShader && (BSS->PixelShader->Bindings.InOutMask & 0x8000) && PipelineDescriptor.depthAttachmentPixelFormat == MTLPixelFormatInvalid)
	{
		PipelineDescriptor.depthAttachmentPixelFormat = (MTLPixelFormat)GPixelFormats[PF_DepthStencil].PlatformFormat;
		PipelineDescriptor.stencilAttachmentPixelFormat = (MTLPixelFormat)GPixelFormats[PF_DepthStencil].PlatformFormat;
	}
	
	// Disable blending and writing on unbound targets or Metal will assert/crash/abort depending on build.
	// At least with this API all the state must match all of the time for it to work.
	for (int Index = 0; Index < MaxMetalRenderTargets; Index++)
	{
		MTLRenderPipelineColorAttachmentDescriptor* Desc = [PipelineDescriptor.colorAttachments objectAtIndexedSubscript:Index];
		if(Desc.pixelFormat == MTLPixelFormatInvalid)
		{
			Desc.blendingEnabled = NO;
			Desc.writeMask = 0;
		}
	}
	
	// set the bound shader state settings
	PipelineDescriptor.vertexDescriptor = VertexDesc.VertexDesc;
	PipelineDescriptor.vertexFunction = BSS->VertexShader->Function;
	PipelineDescriptor.fragmentFunction = BSS->PixelShader ? BSS->PixelShader->Function : nil;
	PipelineDescriptor.sampleCount = SampleCount;
	
	check(SampleCount > 0);
	
	NSError* Error = nil;
	
	if(GUseRHIThread)
	{
		MetalPipelineMutex.Lock();
	}
	
	FMetalRenderPipelineKey ComparableDesc;
	ComparableDesc.RenderPipelineHash = GetHash();
	ComparableDesc.VertexDescriptorHash = VertexDesc;
	ComparableDesc.VertexFunction = BSS->VertexShader->Function;
	ComparableDesc.PixelFunction = BSS->PixelShader ? BSS->PixelShader->Function : nil;
	
	id<MTLRenderPipelineState> PipelineState = MetalPipelineCache.FindRef(ComparableDesc);
	if(PipelineState == nil)
	{
		id<MTLDevice> Device = GetMetalDeviceContext().GetDevice();
#if !UE_BUILD_SHIPPING
		if (Reflection)
		{
			PipelineState = [Device newRenderPipelineStateWithDescriptor:PipelineDescriptor options:MTLPipelineOptionArgumentInfo reflection:Reflection error:&Error];
			if (PipelineState)
			{
				check(*Reflection);
				MetalReflectionCache.Add(ComparableDesc, [*Reflection retain]);
			}
		}
		else
#endif
		{
			PipelineState = [Device newRenderPipelineStateWithDescriptor:PipelineDescriptor error : &Error];
		}
		TRACK_OBJECT(STAT_MetalRenderPipelineStateCount, PipelineState);
		if(PipelineState)
		{
			MetalPipelineCache.Add(ComparableDesc, PipelineState);
		}
	}
#if !UE_BUILD_SHIPPING
	else if (Reflection)
	{
		*Reflection = MetalReflectionCache.FindChecked(ComparableDesc);
	}
#endif
	
	if(GUseRHIThread)
	{
		MetalPipelineMutex.Unlock();
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

#if !UE_BUILD_SHIPPING
MTLRenderPipelineReflection* FMetalRenderPipelineDesc::GetReflectionData(FMetalBoundShaderState* BSS, FMetalHashedVertexDescriptor const& VertexDesc) const
{
	if(GUseRHIThread)
	{
		MetalPipelineMutex.Lock();
	}
	
	FMetalRenderPipelineKey ComparableDesc;
	ComparableDesc.RenderPipelineHash = GetHash();
	ComparableDesc.VertexDescriptorHash = VertexDesc;
	ComparableDesc.VertexFunction = BSS->VertexShader->Function;
	ComparableDesc.PixelFunction = BSS->PixelShader ? BSS->PixelShader->Function : nil;
	
	MTLRenderPipelineReflection* Reflection = MetalReflectionCache.FindChecked(ComparableDesc);
	
	if(GUseRHIThread)
	{
		MetalPipelineMutex.Unlock();
	}
	
	return Reflection;
}
#endif

bool FMetalRenderPipelineDesc::FMetalRenderPipelineKey::operator==(FMetalRenderPipelineKey const& Other) const
{
	if (this != &Other)
	{
		return (RenderPipelineHash == Other.RenderPipelineHash) && (VertexDescriptorHash == Other.VertexDescriptorHash) && (VertexFunction == Other.VertexFunction) && (PixelFunction == Other.PixelFunction);
	}
	return true;
}
