// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandEncoder.cpp: Metal command encoder wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalCommandEncoder.h"
#include "MetalCommandBuffer.h"
#include "MetalProfiler.h"

#pragma mark - Public C++ Boilerplate -

FMetalCommandEncoder::FMetalCommandEncoder(FMetalCommandList& CmdList)
: CommandList(CmdList)
, FrontFacingWinding(MTLWindingClockwise)
, CullMode(MTLCullModeNone)
#if METAL_API_1_1 && PLATFORM_MAC
, DepthClipMode(MTLDepthClipModeClip)
#endif
, FillMode(MTLTriangleFillModeFill)
, DepthStencilState(nil)
, VisibilityMode(MTLVisibilityResultModeDisabled)
, VisibilityOffset(0)
, PipelineStatsBuffer(nil)
, PipelineStatsOffset(0)
, PipelineStatsMask(0)
, RenderPassDesc(nil)
, RenderPassDescApplied(0)
, CommandBuffer(nil)
, RenderCommandEncoder(nil)
, RenderPipelineState(nil)
, ComputeCommandEncoder(nil)
, ComputePipelineState(nil)
, BlitCommandEncoder(nil)
, DebugGroups([NSMutableArray new])
{
	Viewport = {0.0, 0.0, 0.0, 0.0, FLT_MIN, FLT_MAX};
	DepthBias[0] = DepthBias[1] = 0.0f;
	DepthBias[2] = FLT_MAX;
	ScissorRect = {0,0, 1,1};
	FMemory::Memzero(BlendColor);
	StencilRef[0] = StencilRef[1] = 0xff;
	
	FMemory::Memzero(ShaderBuffers);
	FMemory::Memzero(ShaderTextures);
	FMemory::Memzero(ShaderSamplers);
}

FMetalCommandEncoder::~FMetalCommandEncoder(void)
{
	if (CommandBuffer)
	{
		EndEncoding();
		CommitCommandBuffer(false);
	}
	
	if(ComputePipelineState)
	{
		[ComputePipelineState release];
	}
	if(RenderPipelineState)
	{
		[RenderPipelineState release];
	}
	if(DepthStencilState)
	{
		[DepthStencilState release];
	}
	[RenderPassDesc release];
	RenderPassDesc = nil;

	if(DebugGroups)
	{
		[DebugGroups release];
	}
	
	for(int32 i = 0; i < SF_NumFrequencies; i++)
	{
		for(int32 j = 0; j < ML_MaxSamplers; j++)
		{
			if(ShaderSamplers[i].Samplers[j])
			{
				[ShaderSamplers[i].Samplers[j] release];
			}
		}
	}
}

void FMetalCommandEncoder::Reset(void)
{
	check(!CommandBuffer);
	
	if(ComputePipelineState)
	{
		[ComputePipelineState release];
		ComputePipelineState = nil;
	}
	if(RenderPipelineState)
	{
		[RenderPipelineState release];
		RenderPipelineState = nil;
	}
	if(DepthStencilState)
	{
		[DepthStencilState release];
		DepthStencilState = nil;
	}
	
	if(RenderPassDesc)
	{
		UNTRACK_OBJECT(STAT_MetalRenderPassDescriptorCount, RenderPassDesc);
		[RenderPassDesc release];
		RenderPassDesc = nil;
	}
	RenderPassDescApplied = 0;
	
	[DebugGroups removeAllObjects];
	
	FrontFacingWinding = MTLWindingClockwise;
	CullMode = MTLCullModeNone;
#if METAL_API_1_1 && PLATFORM_MAC
	DepthClipMode = MTLDepthClipModeClip;
#endif
	FillMode = MTLTriangleFillModeFill;
	VisibilityMode = MTLVisibilityResultModeDisabled;
	
	Viewport = {0.0, 0.0, 0.0, 0.0, FLT_MIN, FLT_MAX};
	DepthBias[0] = DepthBias[1] = 0.0f;
	DepthBias[2] = FLT_MAX;
	ScissorRect = {0,0, 1,1};
	FMemory::Memzero(BlendColor);
	StencilRef[0] = StencilRef[1] = 0xff;
	
	FMemory::Memzero(ShaderBuffers);
	FMemory::Memzero(ShaderTextures);
	for(int32 i = 0; i < SF_NumFrequencies; i++)
	{
		for(int32 j = 0; j < ML_MaxSamplers; j++)
		{
			if(ShaderSamplers[i].Samplers[j])
			{
				[ShaderSamplers[i].Samplers[j] release];
			}
			ShaderSamplers[i].Samplers[j] = nil;
		}
		ShaderSamplers[i].Bound = 0;
	}
}

#pragma mark - Public Command Buffer Mutators -

void FMetalCommandEncoder::StartCommandBuffer(id<MTLCommandBuffer> const InCommandBuffer)
{
	check(InCommandBuffer);
	check(!CommandBuffer);
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);

	[InCommandBuffer retain];
	id<MTLCommandBuffer> OldCommandBuffer = CommandBuffer;
	CommandBuffer = InCommandBuffer;
	[OldCommandBuffer release];
	
	if ([DebugGroups count] > 0)
	{
		[InCommandBuffer setLabel:[DebugGroups lastObject]];
	}
}
	
void FMetalCommandEncoder::CommitCommandBuffer(bool const bWait)
{
	check(CommandBuffer);
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	
	if (CommandBuffer.label == nil && [DebugGroups count] > 0)
	{
		[CommandBuffer setLabel:[DebugGroups lastObject]];
	}
	
	CommandList.Commit(CommandBuffer, bWait);

	[CommandBuffer release];
	CommandBuffer = nil;
}

#pragma mark - Public Command Encoder Accessors -
	
bool FMetalCommandEncoder::IsRenderCommandEncoderActive(void) const
{
	return RenderCommandEncoder != nil;
}

bool FMetalCommandEncoder::IsComputeCommandEncoderActive(void) const
{
	return ComputeCommandEncoder != nil;
}

bool FMetalCommandEncoder::IsBlitCommandEncoderActive(void) const
{
	return BlitCommandEncoder != nil;
}

bool FMetalCommandEncoder::IsImmediate(void) const
{
	return CommandList.IsImmediate();
}

bool FMetalCommandEncoder::IsRenderPassDescriptorValid(void) const
{
	return (RenderPassDesc != nil);
}

id<MTLRenderCommandEncoder> FMetalCommandEncoder::GetRenderCommandEncoder(void) const
{
	return RenderCommandEncoder;
}

id<MTLComputeCommandEncoder> FMetalCommandEncoder::GetComputeCommandEncoder(void) const
{
	return ComputeCommandEncoder;
}

id<MTLBlitCommandEncoder> FMetalCommandEncoder::GetBlitCommandEncoder(void) const
{
	return BlitCommandEncoder;
}
	
#pragma mark - Public Command Encoder Mutators -

void FMetalCommandEncoder::BeginRenderCommandEncoding(void)
{
	check(RenderPassDesc);
	check(CommandBuffer);
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	
	RenderCommandEncoder = [[CommandBuffer renderCommandEncoderWithDescriptor:RenderPassDesc] retain];
	RenderPassDescApplied++;
	
	if(GEmitDrawEvents)
	{
		NSString* Label = [DebugGroups count] > 0 ? [DebugGroups lastObject] : (NSString*)CFSTR("InitialPass");
		[RenderCommandEncoder setLabel:Label];
#if !UE_BUILD_SHIPPING
		if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelLogOperations)
		{
			FMetalDebugCommandBuffer* Buf = (FMetalDebugCommandBuffer*)CommandBuffer;
			[Buf beginRenderCommandEncoder:Label withDescriptor:RenderPassDesc];
		}
#endif
		
		if([DebugGroups count])
		{
			for (NSString* Group in DebugGroups)
			{
				[RenderCommandEncoder pushDebugGroup:Group];
#if !UE_BUILD_SHIPPING
				if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelLogDebugGroups)
				{
					FMetalDebugCommandBuffer* Buf = (FMetalDebugCommandBuffer*)CommandBuffer;
					[Buf pushDebugGroup:Group];
				}
#endif
			}
		}
    }
}

void FMetalCommandEncoder::RestoreRenderCommandEncoding(void)
{
	check(RenderPassDesc);
	check(CommandBuffer);
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	if ((GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation) && RenderPassDescApplied == 1)
	{
		bool bAllLoadActionsOK = true;
		if(RenderPassDesc.colorAttachments)
		{
			for(uint i = 0; i< 8; i++)
			{
				MTLRenderPassColorAttachmentDescriptor* Desc = [RenderPassDesc.colorAttachments objectAtIndexedSubscript:i];
				if(Desc)
				{
					bAllLoadActionsOK |= (Desc.loadAction == MTLLoadActionLoad);
				}
			}
		}
		if(RenderPassDesc.depthAttachment)
		{
			bAllLoadActionsOK |= (RenderPassDesc.depthAttachment.loadAction == MTLLoadActionLoad);
		}
		if(RenderPassDesc.stencilAttachment)
		{
			bAllLoadActionsOK |= (RenderPassDesc.stencilAttachment.loadAction == MTLLoadActionLoad);
		}
		
		if (!bAllLoadActionsOK)
		{
			UE_LOG(LogMetal, Warning, TEXT("Called RestoreRenderCommandEncoding after the first use of a render-pass to restore the render pass with a clear operation - this would erroneously re-clear any existing draw calls."));
			
			if(RenderPassDesc.colorAttachments)
			{
				for(uint i = 0; i< 8; i++)
				{
					MTLRenderPassColorAttachmentDescriptor* Desc = [RenderPassDesc.colorAttachments objectAtIndexedSubscript:i];
					if(Desc)
					{
						Desc.loadAction = MTLLoadActionLoad;
					}
				}
			}
			if(RenderPassDesc.depthAttachment)
			{
				RenderPassDesc.depthAttachment.loadAction = MTLLoadActionLoad;
			}
			if(RenderPassDesc.stencilAttachment)
			{
				RenderPassDesc.stencilAttachment.loadAction = MTLLoadActionLoad;
			}
		}
	}
#endif
	
	BeginRenderCommandEncoding();
	RestoreRenderCommandEncodingState();
}

void FMetalCommandEncoder::RestoreRenderCommandEncodingState(void)
{
	check(RenderCommandEncoder);
	
	PushDebugGroup(@"RestoreRenderCommandEncodingState");
	
	[RenderCommandEncoder setRenderPipelineState:RenderPipelineState];
	[RenderCommandEncoder setViewport:Viewport];
	[RenderCommandEncoder setFrontFacingWinding:FrontFacingWinding];
	[RenderCommandEncoder setCullMode:CullMode];
#if METAL_API_1_1 && PLATFORM_MAC
	if(GetMetalDeviceContext().SupportsFeature(EMetalFeaturesDepthClipMode))
	{
		[RenderCommandEncoder setDepthClipMode:DepthClipMode];
	}
#endif
	[RenderCommandEncoder setDepthBias:DepthBias[0] slopeScale:DepthBias[1] clamp:DepthBias[2]];
	
	CGRect ViewportRect = CGRectMake(Viewport.originX, Viewport.originY, Viewport.width, Viewport.height);
	CGRect Scissor = CGRectMake(ScissorRect.x, ScissorRect.y, ScissorRect.width, ScissorRect.height);
	CGRect Unified = CGRectIntersection(ViewportRect, Scissor);
	MTLScissorRect UnifiedScissorRect = {(NSUInteger)Unified.origin.x, (NSUInteger)Unified.origin.y, (NSUInteger)Unified.size.width, (NSUInteger)Unified.size.height};
	
	[RenderCommandEncoder setScissorRect:UnifiedScissorRect];
	[RenderCommandEncoder setTriangleFillMode:FillMode];
	[RenderCommandEncoder setBlendColorRed:BlendColor[0] green:BlendColor[1] blue:BlendColor[2] alpha:BlendColor[3]];
	[RenderCommandEncoder setDepthStencilState:DepthStencilState];
#if METAL_API_1_1
	if(GetMetalDeviceContext().SupportsFeature(EMetalFeaturesSeparateStencil))
	{
		[RenderCommandEncoder setStencilFrontReferenceValue: StencilRef[0] backReferenceValue:StencilRef[1]];
	}
	else
#endif
	{
		[RenderCommandEncoder setStencilReferenceValue: StencilRef[0]];
	}
	[RenderCommandEncoder setVisibilityResultMode:VisibilityMode offset:VisibilityOffset];
	
	for(uint32 i = 0; i < ML_MaxBuffers; i++)
	{
		if(ShaderBuffers[SF_Vertex].Buffers[i] != nil && (ShaderBuffers[SF_Vertex].Bound & (1 << i)))
		{
			[RenderCommandEncoder setVertexBuffer:ShaderBuffers[SF_Vertex].Buffers[i] offset:ShaderBuffers[SF_Vertex].Offsets[i] atIndex:i];
		}
		if(ShaderBuffers[SF_Pixel].Buffers[i] != nil && (ShaderBuffers[SF_Pixel].Bound & (1 << i)))
        {
			[RenderCommandEncoder setFragmentBuffer:ShaderBuffers[SF_Pixel].Buffers[i] offset:ShaderBuffers[SF_Pixel].Offsets[i] atIndex:i];
		}
	}
	
	for(uint32 i = 0; i < ML_MaxTextures; i++)
	{
		if(ShaderTextures[SF_Vertex].Textures[i] && (ShaderTextures[SF_Vertex].Bound & (1 << i)))
		{
			[RenderCommandEncoder setVertexTexture:ShaderTextures[SF_Vertex].Textures[i] atIndex:i];
		}
		
		if(ShaderTextures[SF_Pixel].Textures[i] && (ShaderTextures[SF_Pixel].Bound & (1 << i)))
		{
			[RenderCommandEncoder setFragmentTexture:ShaderTextures[SF_Pixel].Textures[i] atIndex:i];
		}
	}
	
	for(uint32 i = 0; i < ML_MaxSamplers; i++)
	{
		if(ShaderSamplers[SF_Vertex].Samplers[i] != nil && (ShaderSamplers[SF_Vertex].Bound & (1 << i)))
		{
			[RenderCommandEncoder setVertexSamplerState:ShaderSamplers[SF_Vertex].Samplers[i]  atIndex:i];
		}
		if(ShaderSamplers[SF_Pixel].Samplers[i] != nil && (ShaderSamplers[SF_Pixel].Bound & (1 << i)))
		{
			[RenderCommandEncoder setFragmentSamplerState:ShaderSamplers[SF_Pixel].Samplers[i] atIndex:i];
		}
	}
	
	PopDebugGroup();
}

void FMetalCommandEncoder::BeginComputeCommandEncoding(void)
{
	check(CommandBuffer);
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	
	ComputeCommandEncoder = [[CommandBuffer computeCommandEncoder] retain];
	
	if(GEmitDrawEvents)
	{
		NSString* Label = [DebugGroups count] > 0 ? [DebugGroups lastObject] : (NSString*)CFSTR("InitialPass");
		[ComputeCommandEncoder setLabel:Label];
#if !UE_BUILD_SHIPPING
		if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelLogOperations)
		{
			FMetalDebugCommandBuffer* Buf = (FMetalDebugCommandBuffer*)CommandBuffer;
			[Buf beginComputeCommandEncoder:Label];
		}
#endif
		
		if([DebugGroups count])
		{
			for (NSString* Group in DebugGroups)
			{
				[ComputeCommandEncoder pushDebugGroup:Group];
#if !UE_BUILD_SHIPPING
				if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelLogDebugGroups)
				{
					FMetalDebugCommandBuffer* Buf = (FMetalDebugCommandBuffer*)CommandBuffer;
					[Buf pushDebugGroup:Group];
				}
#endif
			}
		}
	}
	
    for(uint32 i = 0; i < SF_NumFrequencies; i++)
    {
		ShaderBuffers[i].Bound = 0;
		ShaderTextures[i].Bound = 0;
		ShaderSamplers[i].Bound = 0;
    }
}

void FMetalCommandEncoder::RestoreComputeCommandEncodingState(void)
{
	check(ComputeCommandEncoder);
	
	PushDebugGroup(@"RestoreComputeCommandEncodingState");
	
	[ComputeCommandEncoder setComputePipelineState:ComputePipelineState];
	
	for(uint32 i = 0; i < ML_MaxBuffers; i++)
	{
		if(ShaderBuffers[SF_Compute].Buffers[i] != nil)
		{
			ShaderBuffers[SF_Compute].Bound |= (1 << i);
			[ComputeCommandEncoder setBuffer:ShaderBuffers[SF_Compute].Buffers[i] offset:ShaderBuffers[SF_Compute].Offsets[i] atIndex:i];
		}
	}
	
	for(uint32 i = 0; i < ML_MaxTextures; i++)
	{
		if(ShaderTextures[SF_Compute].Textures[i])
		{
			ShaderTextures[SF_Compute].Bound |= (1 << i);
			[ComputeCommandEncoder setTexture:ShaderTextures[SF_Compute].Textures[i] atIndex:i];
		}
	}
	
	for(uint32 i = 0; i < ML_MaxSamplers; i++)
	{
		if(ShaderSamplers[SF_Compute].Samplers[i] != nil)
		{
			ShaderSamplers[SF_Compute].Bound |= (1 << i);
			[ComputeCommandEncoder setSamplerState:ShaderSamplers[SF_Compute].Samplers[i] atIndex:i];
		}
	}
	
	PopDebugGroup();
}

void FMetalCommandEncoder::BeginBlitCommandEncoding(void)
{
	check(CommandBuffer);
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	
	BlitCommandEncoder = [[CommandBuffer blitCommandEncoder] retain];
	
	if(GEmitDrawEvents)
	{
		NSString* Label = [DebugGroups count] > 0 ? [DebugGroups lastObject] : (NSString*)CFSTR("InitialPass");
		[BlitCommandEncoder setLabel:Label];
#if !UE_BUILD_SHIPPING
		if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelLogOperations)
		{
			FMetalDebugCommandBuffer* Buf = (FMetalDebugCommandBuffer*)CommandBuffer;
			[Buf beginBlitCommandEncoder:Label];
		}
#endif
		
		if([DebugGroups count])
		{
			for (NSString* Group in DebugGroups)
			{
				[BlitCommandEncoder pushDebugGroup:Group];
#if !UE_BUILD_SHIPPING
				if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelLogDebugGroups)
				{
					FMetalDebugCommandBuffer* Buf = (FMetalDebugCommandBuffer*)CommandBuffer;
					[Buf pushDebugGroup:Group];
				}
#endif
			}
		}
	}
}

void FMetalCommandEncoder::EndEncoding(void)
{
#if !UE_BUILD_SHIPPING
	if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelLogOperations)
	{
		FMetalDebugCommandBuffer* Buf = (FMetalDebugCommandBuffer*)CommandBuffer;
		[Buf endCommandEncoder];
	}
#endif
	if(IsRenderCommandEncoderActive())
	{
		[RenderCommandEncoder endEncoding];
		[RenderCommandEncoder release];
		RenderCommandEncoder = nil;
	}
	else if(IsComputeCommandEncoderActive())
	{
		[ComputeCommandEncoder endEncoding];
		[ComputeCommandEncoder release];
		ComputeCommandEncoder = nil;
	}
	else if(IsBlitCommandEncoderActive())
	{
		[BlitCommandEncoder endEncoding];
		[BlitCommandEncoder release];
		BlitCommandEncoder = nil;
	}
}

#pragma mark - Public Debug Support -

void FMetalCommandEncoder::InsertDebugSignpost(NSString* const String)
{
#if !UE_BUILD_SHIPPING
	if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelLogDebugGroups)
	{
		FMetalDebugCommandBuffer* Buf = (FMetalDebugCommandBuffer*)CommandBuffer;
		[Buf insertDebugSignpost:String];
	}
#endif
	
	if (RenderCommandEncoder)
	{
		[RenderCommandEncoder insertDebugSignpost:String];
	}
	if (ComputeCommandEncoder)
	{
		[ComputeCommandEncoder insertDebugSignpost:String];
	}
	if (BlitCommandEncoder)
	{
		[BlitCommandEncoder insertDebugSignpost:String];
	}
}

void FMetalCommandEncoder::PushDebugGroup(NSString* const String)
{
#if !UE_BUILD_SHIPPING
	if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelLogDebugGroups)
	{
		FMetalDebugCommandBuffer* Buf = (FMetalDebugCommandBuffer*)CommandBuffer;
		[Buf pushDebugGroup:String];
	}
#endif
	
	[DebugGroups addObject:String];
	if (RenderCommandEncoder)
	{
		[RenderCommandEncoder pushDebugGroup:String];
	}
	else if (ComputeCommandEncoder)
	{
		[ComputeCommandEncoder pushDebugGroup:String];
	}
	else if (BlitCommandEncoder)
	{
		[BlitCommandEncoder pushDebugGroup:String];
	}
}

void FMetalCommandEncoder::PopDebugGroup(void)
{
#if !UE_BUILD_SHIPPING
	if (CommandList.GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelLogDebugGroups)
	{
		FMetalDebugCommandBuffer* Buf = (FMetalDebugCommandBuffer*)CommandBuffer;
		[Buf popDebugGroup];
	}
#endif
	
	[DebugGroups removeLastObject];
	if (RenderCommandEncoder)
	{
		[RenderCommandEncoder popDebugGroup];
	}
	else if (ComputeCommandEncoder)
	{
		[ComputeCommandEncoder popDebugGroup];
	}
	else if (BlitCommandEncoder)
	{
		[BlitCommandEncoder popDebugGroup];
	}
}
	
#pragma mark - Public Render State Mutators -

void FMetalCommandEncoder::SetRenderPassDescriptor(MTLRenderPassDescriptor* const RenderPass, bool const bReset)
{
	check(IsRenderCommandEncoderActive() == false && IsComputeCommandEncoderActive() == false && IsBlitCommandEncoderActive() == false);
	check(RenderPass);
	
	RenderPassDescApplied = 0;
	if(RenderPass != RenderPassDesc)
	{
		[RenderPass retain];
		if(RenderPassDesc)
		{
			GetMetalDeviceContext().ReleaseObject(RenderPassDesc);
		}
		RenderPassDesc = RenderPass;
	}
	check(RenderPassDesc);
	
	if (bReset)
	{
		for(uint32 i = 0; i < SF_NumFrequencies; i++)
		{
			ShaderBuffers[i].Bound = 0;
			ShaderTextures[i].Bound = 0;
			ShaderSamplers[i].Bound = 0;
		}
	}
}

void FMetalCommandEncoder::SetRenderPipelineState(id<MTLRenderPipelineState> PipelineState)
{
	[PipelineState retain];
	[RenderPipelineState release];
	RenderPipelineState = PipelineState;
	if (RenderCommandEncoder)
	{
		[RenderCommandEncoder setRenderPipelineState:RenderPipelineState];
	}
}

void FMetalCommandEncoder::SetViewport(MTLViewport const& InViewport)
{
	Viewport = InViewport;
	if (RenderCommandEncoder)
	{
		[RenderCommandEncoder setViewport:Viewport];
	}
}

void FMetalCommandEncoder::SetFrontFacingWinding(MTLWinding const InFrontFacingWinding)
{
	FrontFacingWinding = InFrontFacingWinding;
	if (RenderCommandEncoder)
	{
		[RenderCommandEncoder setFrontFacingWinding:FrontFacingWinding];
	}
}

void FMetalCommandEncoder::SetCullMode(MTLCullMode const InCullMode)
{
	CullMode = InCullMode;
	if (RenderCommandEncoder)
	{
		[RenderCommandEncoder setCullMode:InCullMode];
	}
}

#if METAL_API_1_1 && PLATFORM_MAC
void FMetalCommandEncoder::SetDepthClipMode(MTLDepthClipMode const InDepthClipMode)
{
	DepthClipMode = InDepthClipMode;
	if(GetMetalDeviceContext().SupportsFeature(EMetalFeaturesDepthClipMode))
	{
		if (RenderCommandEncoder)
		{
			[RenderCommandEncoder setDepthClipMode:InDepthClipMode];
		}
	}
	else
	{
		UE_LOG(LogMetal, Warning, TEXT("SetDepthClipMode unsupported on the current device."));
	}
}
#endif

void FMetalCommandEncoder::SetDepthBias(float const InDepthBias, float const InSlopeScale, float const InClamp)
{
	DepthBias[0] = InDepthBias;
	DepthBias[1] = InSlopeScale;
	DepthBias[2] = InClamp;
	if (RenderCommandEncoder)
	{
		[RenderCommandEncoder setDepthBias:InDepthBias slopeScale:InSlopeScale clamp:InClamp];
	}
}

void FMetalCommandEncoder::SetScissorRect(MTLScissorRect const& Rect)
{
	ScissorRect = Rect;
	if (RenderCommandEncoder)
	{
		[RenderCommandEncoder setScissorRect:ScissorRect];
	}
}

void FMetalCommandEncoder::SetTriangleFillMode(MTLTriangleFillMode const InFillMode)
{
	FillMode = InFillMode;
	if (RenderCommandEncoder)
	{
		[RenderCommandEncoder setTriangleFillMode:FillMode];
	}
}

void FMetalCommandEncoder::SetBlendColor(float const Red, float const Green, float const Blue, float const Alpha)
{
	BlendColor[0] = Red;
	BlendColor[1] = Green;
	BlendColor[2] = Blue;
	BlendColor[3] = Alpha;
	if (RenderCommandEncoder)
	{
		[RenderCommandEncoder setBlendColorRed:Red green:Green blue:Blue alpha:Alpha];
	}
}

void FMetalCommandEncoder::SetDepthStencilState(id<MTLDepthStencilState> const InDepthStencilState)
{
	[InDepthStencilState retain];
	[DepthStencilState release];
	DepthStencilState = InDepthStencilState;
	if (RenderCommandEncoder)
	{
		[RenderCommandEncoder setDepthStencilState:InDepthStencilState];
	}
}

void FMetalCommandEncoder::SetStencilReferenceValue(uint32 const ReferenceValue)
{
	StencilRef[0] = ReferenceValue;
	StencilRef[1] = ReferenceValue;
	if (RenderCommandEncoder)
	{
		[RenderCommandEncoder setStencilReferenceValue:ReferenceValue];
	}
}

void FMetalCommandEncoder::SetStencilReferenceValue(uint32 const FrontReferenceValue, uint32 const BackReferenceValue)
{
	StencilRef[0] = FrontReferenceValue;
	StencilRef[1] = BackReferenceValue;
	if (RenderCommandEncoder)
	{
#if METAL_API_1_1
		if(GetMetalDeviceContext().SupportsFeature(EMetalFeaturesSeparateStencil))
		{
			[RenderCommandEncoder setStencilFrontReferenceValue: FrontReferenceValue backReferenceValue:BackReferenceValue];
		}
		else
#endif
		{
			UE_LOG(LogMetal, Warning, TEXT("Attempting to set separate stencil ref on device which only supports unified stencil ref."));
			[RenderCommandEncoder setStencilReferenceValue: FrontReferenceValue];
		}
	}
}

void FMetalCommandEncoder::SetVisibilityResultMode(MTLVisibilityResultMode const Mode, NSUInteger const Offset)
{
	VisibilityMode = Mode;
	VisibilityOffset = Offset;
	if (RenderCommandEncoder)
	{
		[RenderCommandEncoder setVisibilityResultMode: Mode offset:Offset];
	}
}
	
#pragma mark - Public Shader Resource Mutators -

void FMetalCommandEncoder::SetShaderBuffer(EShaderFrequency Frequency, id<MTLBuffer> Buffer, NSUInteger const Offset, NSUInteger index)
{
	check(index < ML_MaxBuffers);
    if(GetMetalDeviceContext().SupportsFeature(EMetalFeaturesSetBufferOffset) && (ShaderBuffers[Frequency].Bound & (1 << index)) && ShaderBuffers[Frequency].Buffers[index] == Buffer)
    {
		SetShaderBufferOffset(Frequency, Offset, index);
    }
    else
    {
		if(Buffer)
		{
			ShaderBuffers[Frequency].Bound |= (1 << index);
		}
		else
		{
			ShaderBuffers[Frequency].Bound &= ~(1 << index);
		}
        ShaderBuffers[Frequency].Buffers[index] = Buffer;
        ShaderBuffers[Frequency].Offsets[index] = Offset;
        switch (Frequency)
        {
            case SF_Vertex:
				if (RenderCommandEncoder)
				{
					[RenderCommandEncoder setVertexBuffer:Buffer offset:Offset atIndex:index];
				}
                break;
            case SF_Pixel:
				if (RenderCommandEncoder)
				{
					[RenderCommandEncoder setFragmentBuffer:Buffer offset:Offset atIndex:index];
				}
                break;
            case SF_Compute:
				if (ComputeCommandEncoder)
				{
					[ComputeCommandEncoder setBuffer:Buffer offset:Offset atIndex:index];
				}
                break;
            default:
                check(false);
                break;
        }
    }
}

bool FMetalCommandEncoder::SetShaderBufferConditional(EShaderFrequency const Frequency, id<MTLBuffer> const Buffer, NSUInteger const Offset, NSUInteger const index)
{
	check(index < ML_MaxBuffers);
	check(Buffer);
	bool bBound = (ShaderBuffers[Frequency].Bound & (1 << index));
    if (!bBound)
    {
		ShaderBuffers[Frequency].Bound |= (1 << index);
		ShaderBuffers[Frequency].Buffers[index] = Buffer;
        ShaderBuffers[Frequency].Offsets[index] = Offset;
        switch (Frequency)
        {
            case SF_Vertex:
				if (RenderCommandEncoder)
				{
					[RenderCommandEncoder setVertexBuffer:Buffer offset:Offset atIndex:index];
				}
                break;
            case SF_Pixel:
				if (RenderCommandEncoder)
				{
					[RenderCommandEncoder setFragmentBuffer:Buffer offset:Offset atIndex:index];
				}
                break;
            case SF_Compute:
				if (ComputeCommandEncoder)
				{
					[ComputeCommandEncoder setBuffer:Buffer offset:Offset atIndex:index];
				}
                break;
            default:
                check(false);
                break;
        }
    }
    
    return !bBound;
}

void FMetalCommandEncoder::SetShaderBufferOffset(EShaderFrequency Frequency, NSUInteger const Offset, NSUInteger index)
{
	check(index < ML_MaxBuffers);
    checkf(ShaderBuffers[Frequency].Buffers[index] && (ShaderBuffers[Frequency].Bound & (1 << index)), TEXT("Buffer must already be bound"));
#if METAL_API_1_1
	if(GetMetalDeviceContext().SupportsFeature(EMetalFeaturesSetBufferOffset))
	{
		ShaderBuffers[Frequency].Offsets[index] = Offset;
		switch (Frequency)
		{
			case SF_Vertex:
				if (RenderCommandEncoder)
				{
					[RenderCommandEncoder setVertexBufferOffset:Offset atIndex:index];
				}
				break;
			case SF_Pixel:
				if (RenderCommandEncoder)
				{
					[RenderCommandEncoder setFragmentBufferOffset:Offset atIndex:index];
				}
				break;
			case SF_Compute:
				if (ComputeCommandEncoder)
				{
					[ComputeCommandEncoder setBufferOffset:Offset atIndex:index];
				}
				break;
			default:
				check(false);
				break;
		}
	}
	else
#endif
	{
		SetShaderBuffer(Frequency, ShaderBuffers[Frequency].Buffers[index], Offset, index);
	}
}

void FMetalCommandEncoder::SetShaderBuffers(EShaderFrequency Frequency, id<MTLBuffer> const Buffers[], const NSUInteger Offsets[], NSRange const& Range)
{
	for (NSUInteger i = (NSUInteger)Range.location; i < (NSUInteger)Range.length; i++)
	{
        check(i < ML_MaxBuffers);
		if(Buffers[i])
		{
			ShaderBuffers[Frequency].Bound |= (1 << i);
		}
		else
		{
			ShaderBuffers[Frequency].Bound &= ~(1 << i);
		}
		ShaderBuffers[Frequency].Buffers[i] = Buffers[i];
		ShaderBuffers[Frequency].Offsets[i] = Offsets[i];
	}
	switch (Frequency)
	{
		case SF_Vertex:
			if (RenderCommandEncoder)
			{
				[RenderCommandEncoder setVertexBuffers:Buffers offsets:Offsets withRange:Range];
			}
			break;
		case SF_Pixel:
			if (RenderCommandEncoder)
			{
				[RenderCommandEncoder setFragmentBuffers:Buffers offsets:Offsets withRange:Range];
			}
			break;
		case SF_Compute:
			if (ComputeCommandEncoder)
			{
				[ComputeCommandEncoder setBuffers:Buffers offsets:Offsets withRange:Range];
			}
			break;
		default:
			check(false);
			break;
	}
}

void FMetalCommandEncoder::SetShaderTexture(EShaderFrequency Frequency, id<MTLTexture> Texture, NSUInteger index)
{
	check(index < ML_MaxTextures);
	
	if(Texture)
	{
		ShaderTextures[Frequency].Bound |= (1 << index);
	}
	else
	{
		ShaderTextures[Frequency].Bound &= ~(1 << index);
	}
	
	ShaderTextures[Frequency].Textures[index] = Texture;
	
	switch (Frequency)
	{
		case SF_Vertex:
			if (RenderCommandEncoder)
			{
				[RenderCommandEncoder setVertexTexture:Texture atIndex:index];
			}
			break;
		case SF_Pixel:
			if (RenderCommandEncoder)
			{
				[RenderCommandEncoder setFragmentTexture:Texture atIndex:index];
			}
			break;
		case SF_Compute:
			if (ComputeCommandEncoder)
			{
				[ComputeCommandEncoder setTexture:Texture atIndex:index];
			}
			break;
		default:
			check(false);
			break;
	}
}

void FMetalCommandEncoder::SetShaderTextures(EShaderFrequency Frequency, const id<MTLTexture> Textures[], NSRange const& Range)
{
	for (NSUInteger i = (NSUInteger)Range.location; i < (NSUInteger)Range.length; i++)
	{
		check(i < ML_MaxTextures);
		if(Textures[i])
		{
			ShaderTextures[Frequency].Bound |= (1 << i);
		}
		else
		{
			ShaderTextures[Frequency].Bound &= ~(1 << i);
		}
		
		ShaderTextures[Frequency].Textures[i] = Textures[i];
	}
	
	switch (Frequency)
	{
		case SF_Vertex:
			if (RenderCommandEncoder)
			{
				[RenderCommandEncoder setVertexTextures:Textures withRange:Range];
			}
			break;
		case SF_Pixel:
			if (RenderCommandEncoder)
			{
				[RenderCommandEncoder setFragmentTextures:Textures withRange:Range];
			}
			break;
		case SF_Compute:
			if (ComputeCommandEncoder)
			{
				[ComputeCommandEncoder setTextures:Textures withRange:Range];
			}
			break;
		default:
			check(false);
			break;
	}
}

void FMetalCommandEncoder::SetShaderSamplerState(EShaderFrequency Frequency, id<MTLSamplerState> Sampler, NSUInteger index)
{
	check(index < ML_MaxSamplers);
	[Sampler retain];
	[ShaderSamplers[Frequency].Samplers[index] release];
	
	if(Sampler)
	{
		ShaderSamplers[Frequency].Bound |= (1 << index);
	}
	else
	{
		ShaderSamplers[Frequency].Bound &= ~(1 << index);
	}
	
	ShaderSamplers[Frequency].Samplers[index] = Sampler;
	
	switch (Frequency)
	{
		case SF_Vertex:
			if (RenderCommandEncoder)
			{
				[RenderCommandEncoder setVertexSamplerState:Sampler atIndex:index];
			}
			break;
		case SF_Pixel:
			if (RenderCommandEncoder)
			{
				[RenderCommandEncoder setFragmentSamplerState:Sampler atIndex:index];
			}
			break;
		case SF_Compute:
			if (ComputeCommandEncoder)
			{
				[ComputeCommandEncoder setSamplerState:Sampler atIndex:index];
			}
			break;
		default:
			check(false);
			break;
	}
}

void FMetalCommandEncoder::SetShaderSamplerStates(EShaderFrequency Frequency, const id<MTLSamplerState> Samplers[], NSRange const& Range)
{
	for (NSUInteger i = (NSUInteger)Range.location; i < (NSUInteger)Range.length; i++)
	{
		check(i < ML_MaxSamplers);
		[Samplers[i] retain];
		[ShaderSamplers[Frequency].Samplers[i] release];
		
		if(Samplers[i])
		{
			ShaderSamplers[Frequency].Bound |= (1 << i);
		}
		else
		{
			ShaderSamplers[Frequency].Bound &= ~(1 << i);
		}
		
		ShaderSamplers[Frequency].Samplers[i] = Samplers[i];
	}
	
	switch (Frequency)
	{
		case SF_Vertex:
			if (RenderCommandEncoder)
			{
				[RenderCommandEncoder setVertexSamplerStates:Samplers withRange:Range];
			}
			break;
		case SF_Pixel:
			if (RenderCommandEncoder)
			{
				[RenderCommandEncoder setFragmentSamplerStates:Samplers withRange:Range];
			}
			break;
		case SF_Compute:
			if (ComputeCommandEncoder)
			{
				[ComputeCommandEncoder setSamplerStates:Samplers withRange:Range];
			}
			break;
		default:
			check(false);
			break;
	}
}

bool FMetalCommandEncoder::ValidateArgumentBindings(EShaderFrequency const Frequency, MTLRenderPipelineReflection* Reflection)
{
    bool bOK = true;
    
	NSArray<MTLArgument*>* Arguments = nil;
	switch(Frequency)
	{
		case SF_Vertex:
		{
			Arguments = Reflection.vertexArguments;
			break;
		}
		case SF_Pixel:
		{
			Arguments = Reflection.fragmentArguments;
			break;
		}
		default:
			check(false);
			break;
	}
	
	for (uint32 i = 0; i < Arguments.count; i++)
	{
		MTLArgument* Arg = [Arguments objectAtIndex:i];
		check(Arg);
		switch(Arg.type)
		{
			case MTLArgumentTypeBuffer:
			{
				checkf(Arg.index < ML_MaxBuffers, TEXT("Metal buffer index exceeded!"));
				if (ShaderBuffers[Frequency].Buffers[Arg.index] == nil || !(ShaderBuffers[Frequency].Bound & (1 << Arg.index)))
				{
                    bOK = false;
					UE_LOG(LogMetal, Warning, TEXT("Unbound buffer at Metal index: %u which will crash the driver."), (uint32)Arg.index);
				}
				break;
			}
   			case MTLArgumentTypeThreadgroupMemory:
			{
				break;
			}
    		case MTLArgumentTypeTexture:
			{
				checkf(Arg.index < ML_MaxTextures, TEXT("Metal texture index exceeded!"));
				if (ShaderTextures[Frequency].Textures[Arg.index] == nil || !(ShaderTextures[Frequency].Bound & (1 << Arg.index)))
                {
                    bOK = false;
					UE_LOG(LogMetal, Warning, TEXT("Unbound texture at Metal index: %u which will crash the driver."), (uint32)Arg.index);
				}
				else if (ShaderTextures[Frequency].Textures[Arg.index].textureType != Arg.textureType)
                {
                    bOK = false;
					UE_LOG(LogMetal, Warning, TEXT("Incorrect texture type bound at Metal index: %u which will crash the driver."), (uint32)Arg.index);
				}
				break;
			}
    		case MTLArgumentTypeSampler:
			{
				checkf(Arg.index < ML_MaxSamplers, TEXT("Metal sampler index exceeded!"));
				if (ShaderSamplers[Frequency].Samplers[Arg.index] == nil || !(ShaderSamplers[Frequency].Bound & (1 << Arg.index)))
                {
                    bOK = false;
					UE_LOG(LogMetal, Warning, TEXT("Unbound sampler at Metal index: %u which will crash the driver."), (uint32)Arg.index);
				}
				break;
			}
			default:
				check(false);
				break;
		}	
	}
    
    return bOK;
}

#pragma mark - Public Compute State Mutators -

void FMetalCommandEncoder::SetComputePipelineState(id<MTLComputePipelineState> const State)
{
	[State retain];
	[ComputePipelineState release];
	ComputePipelineState = State;
	if (ComputeCommandEncoder)
	{
		[ComputeCommandEncoder setComputePipelineState:State];
	}
}

#pragma mark - Public Extension Accessors -

#pragma mark - Public Extension Mutators -
	
#pragma mark - Public Support Functions -

void FMetalCommandEncoder::UnbindObject(id const Object)
{
	for(uint32 i = 0; i < SF_NumFrequencies; i++)
	{
		for(uint32 Index = 0; Index < ML_MaxBuffers; Index++)
		{
			if(ShaderBuffers[i].Buffers[Index] == Object)
			{
				ShaderBuffers[i].Buffers[Index] = nullptr;
				ShaderBuffers[i].Bound &= ~(1 << Index);
			}
		}
		for(uint32 Index = 0; Index < ML_MaxTextures; Index++)
		{
			if(ShaderTextures[i].Textures[Index] == Object)
			{
				ShaderTextures[i].Textures[Index] = nullptr;
				ShaderTextures[i].Bound &= ~(1 << Index);
			}
		}
		for(uint32 Index = 0; Index < ML_MaxSamplers; Index++)
		{
			if(ShaderSamplers[i].Samplers[Index] == Object)
			{
				ShaderSamplers[i].Samplers[Index] = nullptr;
				ShaderSamplers[i].Bound &= ~(1 << Index);
			}
		}
	}
}
