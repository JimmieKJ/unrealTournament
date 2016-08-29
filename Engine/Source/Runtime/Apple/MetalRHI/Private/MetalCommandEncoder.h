// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>

class FMetalCommandList;

/**
 * FMetalCommandEncoder:
 *	Wraps the details of switching between different command encoders on the command-buffer, allowing for restoration of the render encoder if needed.
 * 	UE4 expects the API to serialise commands in-order, but Metal expects applications to work with command-buffers directly so we need to implement 
 *	the RHI semantics by switching between encoder types. This class hides the ugly details. Eventually it might be possible to move some of the operations
 *	into pre- & post- command-buffers so that we avoid encoder switches but that will take changes to the RHI and high-level code too, so it won't happen soon.
 */
class FMetalCommandEncoder
{
public:
#pragma mark - Public C++ Boilerplate -

	/** Default constructor */
	FMetalCommandEncoder(FMetalCommandList& CmdList);
	
	/** Destructor */
	~FMetalCommandEncoder(void);
	
	/** Reset cached state for reuse */
	void Reset(void);
	
#pragma mark - Public Command Buffer Mutators -

	/**
	 * Start encoding to CommandBuffer. It is an error to call this with any outstanding command encoders or current command buffer.
	 * Instead call EndEncoding & CommitCommandBuffer before calling this.
	 * @param CommandBuffer The new command buffer to begin encoding to.
	 */
	void StartCommandBuffer(id<MTLCommandBuffer> const CommandBuffer);
	
	/**
	 * Commit the existing command buffer if there is one & optionally waiting for completion, if there isn't a current command buffer this is a no-op.
	 * @param bWait If true will wait for command buffer completion, otherwise the function returns immediately.
 	 */
	void CommitCommandBuffer(bool const bWait);

#pragma mark - Public Command Encoder Accessors -
	
	/** @returns True if and only if there is an active render command encoder, otherwise false. */
	bool IsRenderCommandEncoderActive(void) const;
	
	/** @returns True if and only if there is an active compute command encoder, otherwise false. */
	bool IsComputeCommandEncoderActive(void) const;
	
	/** @returns True if and only if there is an active blit command encoder, otherwise false. */
	bool IsBlitCommandEncoderActive(void) const;
	
	/**
	 * True iff the command-encoder submits immediately to the command-queue, false if it performs any buffering.
	 * @returns True iff the command-list submits immediately to the command-queue, false if it performs any buffering.
	 */
	bool IsImmediate(void) const;

	/** @returns True if and only if there is valid render pass descriptor set on the encoder, otherwise false. */
	bool IsRenderPassDescriptorValid(void) const;
	
	/** @returns The active render command encoder or nil if there isn't one. */
	id<MTLRenderCommandEncoder> GetRenderCommandEncoder(void) const;
	
	/** @returns The active compute command encoder or nil if there isn't one. */
	id<MTLComputeCommandEncoder> GetComputeCommandEncoder(void) const;
	
	/** @returns The active blit command encoder or nil if there isn't one. */
	id<MTLBlitCommandEncoder> GetBlitCommandEncoder(void) const;
	
#pragma mark - Public Command Encoder Mutators -

	/**
 	 * Begins encoding rendering commands into the current command buffer. No other encoder may be active & the MTLRenderPassDescriptor must previously have been set.
	 */
	void BeginRenderCommandEncoding(void);
	
	/** Restore the render command encoder from whatever state we were in. Any previous encoder must first be terminated with EndEncoding. */
	void RestoreRenderCommandEncoding(void);
	
	/** Restores the render command state into a new render command encoder. */		
	void RestoreRenderCommandEncodingState(void);
	
	/** Begins encoding compute commands into the current command buffer. No other encoder may be active. */
	void BeginComputeCommandEncoding(void);
	
	/** Restores the compute command state into a new compute command encoder. */
	void RestoreComputeCommandEncodingState(void);
	
	/** Begins encoding blit commands into the current command buffer. No other encoder may be active. */
	void BeginBlitCommandEncoding(void);
	
	/** Declare that all command generation from this encoder is complete, and detach from the MTLCommandBuffer if there is an encoder active or does nothing if there isn't. */
	void EndEncoding(void);

#pragma mark - Public Debug Support -
	
	/*
	 * Inserts a debug string into the command buffer.  This does not change any API behavior, but can be useful when debugging.
	 * @param string The name of the signpost. 
	 */
	void InsertDebugSignpost(NSString* const String);
	
	/*
	 * Push a new named string onto a stack of string labels.
	 * @param string The name of the debug group. 
	 */
	void PushDebugGroup(NSString* const String);
	
	/* Pop the latest named string off of the stack. */
	void PopDebugGroup(void);
	
#pragma mark - Public Render State Mutators -

	/**
	 * Set the render pass descriptor - no encoder may be active when this function is called.
	 * @param RenderPass The render pass descriptor to set. May be nil.
	 * @param bReset Whether to reset existing state.
	 */
	void SetRenderPassDescriptor(MTLRenderPassDescriptor* const RenderPass, bool const bReset);
	
	/*
	 * Sets the current render pipeline state object.
	 * @param PipelineState The pipeline state to set. Must not be nil.
	 */
	void SetRenderPipelineState(id<MTLRenderPipelineState> const PipelineState);
	
	/*
	 * Set the viewport, which is used to transform vertexes from normalized device coordinates to window coordinates.  Fragments that lie outside of the viewport are clipped, and optionally clamped for fragments outside of znear/zfar.
	 * @param Viewport The viewport dimensions to use.
	 */
	void SetViewport(MTLViewport const& Viewport);
	
	/*
	 * The winding order of front-facing primitives.
	 * @param FrontFacingWinding The front face winding.
	 */
	void SetFrontFacingWinding(MTLWinding const FrontFacingWinding);
	
	/*
	 * Controls if primitives are culled when front facing, back facing, or not culled at all.
	 * @param CullMode The cull mode.
	 */
	void SetCullMode(MTLCullMode const CullMode);

#if METAL_API_1_1 && PLATFORM_MAC
	/*
	 * Controls what is done with fragments outside of the near or far planes.
	 * @param DepthClipMode the clip mode.
	 */
	void SetDepthClipMode(MTLDepthClipMode const DepthClipMode);
#endif
	
	/*
	 * Depth Bias.
	 * @param DepthBias The depth-bias value.
	 * @param SlopeScale The slope-scale to apply.
	 * @param Clamp The value to clamp to.
	 */
	void SetDepthBias(float const DepthBias, float const SlopeScale, float const Clamp);
	
	/*
	 * Specifies a rectangle for a fragment scissor test.  All fragments outside of this rectangle are discarded.
	 * @param Rect The scissor rect dimensions.
	 */
	void SetScissorRect(MTLScissorRect const& Rect);
	
	/*
	 * Set how to rasterize triangle and triangle strip primitives.
	 * @param FillMode The fill mode.
	 */
	void SetTriangleFillMode(MTLTriangleFillMode const FillMode);
	
	/*
	 * Set the constant blend color used across all blending on all render targets
	 * @param Red The value for the red channel in 0-1.
	 * @param Green The value for the green channel in 0-1.
	 * @param Blue The value for the blue channel in 0-1.
	 * @param Alpha The value for the alpha channel in 0-1.
	 */
	void SetBlendColor(float const Red, float const Green, float const Blue, float const Alpha);
	
	/*
	 * Set the DepthStencil state object.
	 * @param DepthStencilState The depth-stencil state, must not be nil.
	 */
	void SetDepthStencilState(id<MTLDepthStencilState> const DepthStencilState);
	
	/*
	 * Set the stencil reference value for both the back and front stencil buffers.
	 * @param ReferenceValue The stencil ref value to use.
	 */
	void SetStencilReferenceValue(uint32 const ReferenceValue);

	/*
	 * Set the stencil reference value for the back and front stencil buffers independently.
	 * @param FrontReferenceValue The front face stencil ref value.
	 * @param BackReferenceValue The back face stencil ref value.
	 */
	void SetStencilReferenceValue(uint32 const FrontReferenceValue, uint32 const BackReferenceValue);
	
	/*
	 * Monitor if samples pass the depth and stencil tests.
	 * @param Mode Controls if the counter is disabled or moniters passing samples.
	 * @param Offset The offset relative to the occlusion query buffer provided when the command encoder was created.  offset must be a multiple of 8.
	 */
	void SetVisibilityResultMode(MTLVisibilityResultMode const Mode, NSUInteger const Offset);
	
#pragma mark - Public Shader Resource Mutators -
	
	/*
	 * Set a global buffer for the specified shader frequency at the given bind point index.
	 * @param Frequency The shader frequency to modify.
	 * @param Buffer The buffer to bind or nil to clear.
	 * @param Offset The offset in the buffer or 0 when Buffer is nil.
	 * @param Index The index to modify.
	 */
	void SetShaderBuffer(EShaderFrequency const Frequency, id<MTLBuffer> const Buffer, NSUInteger const Offset, NSUInteger const Index);
	
	/*
	 * Conditionally set a global buffer for the specified shader frequency at the given bind point index.
	 * @param Frequency The shader frequency to modify.
	 * @param Buffer The buffer to bind or nil to clear.
	 * @param Offset The offset in the buffer or 0 when Buffer is nil.
	 * @param Index The index to modify.
	 * @returns True if the buffer was set because there was no existing binding otherwise false as the buffer was not set. 
	 */
	bool SetShaderBufferConditional(EShaderFrequency const Frequency, id<MTLBuffer> const Buffer, NSUInteger const Offset, NSUInteger const Index);
	
	/*
	 * Set the offset for the buffer bound on the specified shader frequency at the given bind point index.
	 * @param Frequency The shader frequency to modify.
	 * @param Offset The offset in the buffer or 0 when Buffer is nil.
	 * @param Index The index to modify.
	 */
	void SetShaderBufferOffset(EShaderFrequency const Frequency, NSUInteger const Offset, NSUInteger const Index);
	
	/*
	 * Set an array of global buffers for the specified shader frequency with the given bind point range.
	 * @param Frequency The shader frequency to modify.
	 * @param Buffer sThe buffers to bind or nil to clear.
	 * @param Offset The offset in the buffer or 0 when Buffer is nil.
	 * @param Range The start point and number of indices to modify.
	 */
	void SetShaderBuffers(EShaderFrequency const Frequency, const id<MTLBuffer> Buffers[], const NSUInteger Offset[], NSRange const& Range);
	
	/*
	 * Set a global texture for the specified shader frequency at the given bind point index.
	 * @param Frequency The shader frequency to modify.
	 * @param Texture The texture to bind or nil to clear.
	 * @param Index The index to modify.
	 */
	void SetShaderTexture(EShaderFrequency const Frequency, id<MTLTexture> const Texture, NSUInteger const Index);
	
	/*
	 * Set an array of global textures for the specified shader frequency with the given bind point range.
	 * @param Frequency The shader frequency to modify.
	 * @param Textures The textures to bind or nil to clear.
	 * @param Range The start point and number of indices to modify.
	 */
	void SetShaderTextures(EShaderFrequency const Frequency, const id<MTLTexture> Textures[], NSRange const& Range);
	
	/*
	 * Set a global sampler for the specified shader frequency at the given bind point index.
	 * @param Frequency The shader frequency to modify.
	 * @param Sampler The sampler state to bind or nil to clear.
	 * @param Index The index to modify.
	 */
	void SetShaderSamplerState(EShaderFrequency const Frequency, id<MTLSamplerState> const Sampler, NSUInteger const Index);
	
	/*
	 * Set an array of global samplers for the specified shader frequency with the given bind point range.
	 * @param Frequency The shader frequency to modify.
	 * @param Samplers The sampler states to bind or nil to clear.
	 * @param Range The start point and number of indices to modify.
	 */
	void SetShaderSamplerStates(EShaderFrequency const Frequency, const id<MTLSamplerState> Samplers[], NSRange const& Range);
	
	/*
	 * Validate the argument binding state for the given shader frequency and report whether the current bindings are sufficient.
	 * @param Frequency The shader frequency to validate.
	 * @param Reflection The shader reflection data to validate against.
	 * @returns True if and only if the current binding state satisfies the reflection data, otherwise false.
	 */
	bool ValidateArgumentBindings(EShaderFrequency const Frequency, MTLRenderPipelineReflection* Reflection);
	
#pragma mark - Public Compute State Mutators -
	
	/*
	 * Set the compute pipeline state that will be used.
	 * @param State The state to set - must not be nil.
	 */
	void SetComputePipelineState(id<MTLComputePipelineState> const State);

#pragma mark - Public Extension Accessors -
	
#pragma mark - Public Extension Mutators -
	
#pragma mark - Public Support Functions -

	/*
	 * Unbinds Object from the cached state so that it cannot be restored accidentally.
	 * @param Object The object to remove from the state caching.
	 */
	void UnbindObject(id const Object);
	
private:
#pragma mark - Private Per-Platform Defines -

#if PLATFORM_IOS
	#define METAL_MAX_TEXTURES 31
	typedef uint32 FMetalTextureMask;
#elif PLATFORM_MAC
	#define METAL_MAX_TEXTURES 128
	typedef __uint128_t FMetalTextureMask;
#else
	#error "Unsupported Platform!"
#endif

#pragma mark - Private Type Declarations -

	/**
	 * The sampler, buffer and texture resource limits as defined here:
	 * https://developer.apple.com/library/ios/documentation/Miscellaneous/Conceptual/MetalProgrammingGuide/Render-Ctx/Render-Ctx.html
	 */
	enum EMetalLimits
	{
		ML_MaxSamplers = 16, /** Maximum number of samplers */
		ML_MaxBuffers = 31, /** Maximum number of buffers */
		ML_MaxTextures = METAL_MAX_TEXTURES /** Maximum number of textures - there are more textures available on Mac than iOS */
	};
	
	/** A structure of arrays for the current buffer binding settings. */
	struct FMetalBufferBindings
	{
		/** The bound buffers or nil. */
		id<MTLBuffer> Buffers[ML_MaxBuffers];
		/** The bound buffer offsets or 0. */
		NSUInteger Offsets[ML_MaxBuffers];
		/** A bitmask for which buffers were bound by the application where a bit value of 1 is bound and 0 is unbound. */
        uint32 Bound;
	};

	/** A structure of arrays for the current texture binding settings. */
	struct FMetalTextureBindings
	{
		/** The bound textures or nil. */
		id<MTLTexture> Textures[ML_MaxTextures];
		/** A bitmask for which textures were bound by the application where a bit value of 1 is bound and 0 is unbound. */
		FMetalTextureMask Bound;
	};

	/** A structure of arrays for the current sampler binding settings. */
	struct FMetalSamplerBindings
	{
		/** The bound sampler states or nil. */
		id<MTLSamplerState> Samplers[ML_MaxSamplers];
		/** A bitmask for which samplers were bound by the application where a bit value of 1 is bound and 0 is unbound. */
		uint16 Bound;
	};
	
#pragma mark - Private Member Variables -

	FMetalBufferBindings ShaderBuffers[SF_NumFrequencies];
	FMetalTextureBindings ShaderTextures[SF_NumFrequencies];
	FMetalSamplerBindings ShaderSamplers[SF_NumFrequencies];
	
	FMetalCommandList& CommandList;
	
	MTLViewport Viewport;
	MTLWinding FrontFacingWinding;
	MTLCullMode CullMode;
#if METAL_API_1_1
	MTLDepthClipMode DepthClipMode;
#endif
	float DepthBias[3];
	MTLScissorRect ScissorRect;
	MTLTriangleFillMode FillMode;
	float BlendColor[4];
	
	id<MTLDepthStencilState> DepthStencilState;
	uint32 StencilRef[2];
	
	MTLVisibilityResultMode VisibilityMode;
	NSUInteger VisibilityOffset;
	
	id<MTLBuffer> PipelineStatsBuffer;
	NSUInteger PipelineStatsOffset;
	NSUInteger PipelineStatsMask;
	
	MTLRenderPassDescriptor* RenderPassDesc;
	NSUInteger RenderPassDescApplied;
	
	id<MTLCommandBuffer> CommandBuffer;
	id<MTLRenderCommandEncoder> RenderCommandEncoder;
	id<MTLRenderPipelineState> RenderPipelineState;
	id<MTLComputeCommandEncoder> ComputeCommandEncoder;
	id<MTLComputePipelineState> ComputePipelineState;
	id<MTLBlitCommandEncoder> BlitCommandEncoder;
	
	NSMutableArray* DebugGroups;
};
