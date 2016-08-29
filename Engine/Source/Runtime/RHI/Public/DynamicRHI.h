// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DynamicRHI.h: Dynamically bound Render Hardware Interface definitions.
=============================================================================*/

#pragma once

#include "ModuleInterface.h"

//
// Statically bound RHI resource reference type definitions for the dynamically bound RHI.
//

/** Context that is capable of doing Compute work.  Can be async or compute on the gfx pipe. */
class IRHIComputeContext
{
public:
	/**
	* Compute queue will wait for the fence to be written before continuing.
	*/
	virtual void RHIWaitComputeFence(FComputeFenceRHIParamRef InFence) = 0;

	/**
	*Sets the current compute shader.
	*/
	virtual void RHISetComputeShader(FComputeShaderRHIParamRef ComputeShader) = 0;

	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) = 0;

	virtual void RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) = 0;

	virtual void RHISetAsyncComputeBudget(EAsyncComputeBudget Budget) = 0;

	/**
	* Explicitly transition a UAV from readable -> writable by the GPU or vice versa.
	* Also explicitly states which pipeline the UAV can be used on next.  For example, if a Compute job just wrote this UAV for a Pixel shader to read
	* you would do EResourceTransitionAccess::Readable and EResourceTransitionPipeline::EComputeToGfx
	*
	* @param TransitionType - direction of the transition
	* @param EResourceTransitionPipeline - How this UAV is transitioning between Gfx and Compute, if at all.
	* @param InUAVs - array of UAV objects to transition
	* @param NumUAVs - number of UAVs to transition
	* @param WriteComputeFence - Optional ComputeFence to write as part of this transition
	*/
	virtual void RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteComputeFence) = 0;

	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FComputeShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) = 0;

	/**
	* Sets sampler state.
	* @param GeometryShader	The geometry shader to set the sampler for.
	* @param SamplerIndex		The index of the sampler.
	* @param NewState			The new sampler state.
	*/
	virtual void RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) = 0;

	/**
	* Sets a compute shader UAV parameter.
	* @param ComputeShader	The compute shader to set the UAV for.
	* @param UAVIndex		The index of the UAVIndex.
	* @param UAV			The new UAV.
	*/
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV) = 0;

	/**
	* Sets a compute shader counted UAV parameter and initial count
	* @param ComputeShader	The compute shader to set the UAV for.
	* @param UAVIndex		The index of the UAVIndex.
	* @param UAV			The new UAV.
	* @param InitialCount	The initial number of items in the UAV.
	*/
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount) = 0;

	virtual void RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) = 0;

	virtual void RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) = 0;

	virtual void RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) = 0;

	virtual void RHIPushEvent(const TCHAR* Name, FColor Color) = 0;

	virtual void RHIPopEvent() = 0;

	/**
	* Submit the current command buffer to the GPU if possible.
	*/
	virtual void RHISubmitCommandsHint() = 0;
};

/** The interface RHI command context. Sometimes the RHI handles these. On platforms that can processes command lists in parallel, it is a separate object. */
class IRHICommandContext : public IRHIComputeContext
{
public:
	virtual ~IRHICommandContext()
	{
	}

	/////// RHI Context Methods

	/**
	* Compute queue will wait for the fence to be written before continuing.
	*/
	virtual void RHIWaitComputeFence(FComputeFenceRHIParamRef InFence) override
	{
		if (InFence)
		{
			checkf(InFence->GetWriteEnqueued(), TEXT("ComputeFence: %s waited on before being written. This will hang the GPU."), *InFence->GetName().ToString());
		}
	}

	/**
	 *Sets the current compute shader.  Mostly for compliance with platforms
	 *that require shader setting before resource binding.
	 */
	virtual void RHISetComputeShader(FComputeShaderRHIParamRef ComputeShader) = 0;

	virtual void RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ) = 0;

	virtual void RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) = 0;
	
	virtual void RHISetAsyncComputeBudget(EAsyncComputeBudget Budget) override
	{
	}

	virtual void RHIAutomaticCacheFlushAfterComputeShader(bool bEnable) = 0;

	virtual void RHIFlushComputeShaderCache() = 0;

	// Useful when used with geometry shader (emit polygons to different viewports), otherwise SetViewPort() is simpler
	// @param Count >0
	// @param Data must not be 0
	virtual void RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) = 0;

	/** Clears a UAV to the multi-component value provided. */
	virtual void RHIClearUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32* Values) = 0;

	/**
	* Resolves from one texture to another.
	* @param SourceTexture - texture to resolve from, 0 is silenty ignored
	* @param DestTexture - texture to resolve to, 0 is silenty ignored
	* @param bKeepOriginalSurface - true if the original surface will still be used after this function so must remain valid
	* @param ResolveParams - optional resolve params
	*/
	virtual void RHICopyToResolveTarget(FTextureRHIParamRef SourceTexture, FTextureRHIParamRef DestTexture, bool bKeepOriginalSurface, const FResolveParams& ResolveParams) = 0;

	/**
	 * Explicitly transition a texture resource from readable -> writable by the GPU or vice versa.
	 * We know rendertargets are only used as rendered targets on the Gfx pipeline, so these transitions are assumed to be implemented such
	 * Gfx->Gfx and Gfx->Compute pipeline transitions are both handled by this call by the RHI implementation.  Hence, no pipeline parameter on this call.
	 *
	 * @param TransitionType - direction of the transition	 
	 * @param InTextures - array of texture objects to transition	 
	 * @param NumTextures - number of textures to transition
	 */	
	virtual void RHITransitionResources(EResourceTransitionAccess TransitionType, FTextureRHIParamRef* InTextures, int32 NumTextures)
	{
		if (TransitionType == EResourceTransitionAccess::EReadable)
		{
			const FResolveParams ResolveParams;
			for (int32 i = 0; i < NumTextures; ++i)
			{
				RHICopyToResolveTarget(InTextures[i], InTextures[i], true, ResolveParams);
			}			
		}
	}

	/**
	* Explicitly transition a UAV from readable -> writable by the GPU or vice versa.
	* Also explicitly states which pipeline the UAV can be used on next.  For example, if a Compute job just wrote this UAV for a Pixel shader to read
	* you would do EResourceTransitionAccess::Readable and EResourceTransitionPipeline::EComputeToGfx
	*
	* @param TransitionType - direction of the transition
	* @param EResourceTransitionPipeline - How this UAV is transitioning between Gfx and Compute, if at all.
	* @param InUAVs - array of UAV objects to transition
	* @param NumUAVs - number of UAVs to transition
	* @param WriteComputeFence - Optional ComputeFence to write as part of this transition
	*/
	virtual void RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteComputeFence)
	{
		if (WriteComputeFence)
		{
			WriteComputeFence->WriteFence();
		}
	}

	void RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs)
	{
		RHITransitionResources(TransitionType, TransitionPipeline, InUAVs, NumUAVs, nullptr);
	}


	virtual void RHIBeginRenderQuery(FRenderQueryRHIParamRef RenderQuery) = 0;

	virtual void RHIEndRenderQuery(FRenderQueryRHIParamRef RenderQuery) = 0;

	virtual void RHIBeginOcclusionQueryBatch() = 0;

	virtual void RHIEndOcclusionQueryBatch() = 0;

	virtual void RHISubmitCommandsHint() = 0;

	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIBeginDrawingViewport(FViewportRHIParamRef Viewport, FTextureRHIParamRef RenderTargetRHI) = 0;

	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIEndDrawingViewport(FViewportRHIParamRef Viewport, bool bPresent, bool bLockToVsync) = 0;

	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIBeginFrame() = 0;

	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIEndFrame() = 0;

	/**
		* Signals the beginning of scene rendering. The RHI makes certain caching assumptions between
		* calls to BeginScene/EndScene. Currently the only restriction is that you can't update texture
		* references.
		*/
	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIBeginScene() = 0;

	/**
		* Signals the end of scene rendering. See RHIBeginScene.
		*/
	// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
	virtual void RHIEndScene() = 0;

	virtual void RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint32 Offset) = 0;

	virtual void RHISetRasterizerState(FRasterizerStateRHIParamRef NewState) = 0;

	// @param MinX including like Win32 RECT
	// @param MinY including like Win32 RECT
	// @param MaxX excluding like Win32 RECT
	// @param MaxY excluding like Win32 RECT
	virtual void RHISetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ) = 0;

	virtual void RHISetStereoViewport(uint32 LeftMinX, uint32 RightMinX, uint32 MinY, float MinZ, uint32 LeftMaxX, uint32 RightMaxX, uint32 MaxY, float MaxZ) = 0;

	// @param MinX including like Win32 RECT
	// @param MinY including like Win32 RECT
	// @param MaxX excluding like Win32 RECT
	// @param MaxY excluding like Win32 RECT
	virtual void RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY) = 0;

	/**
	 * Set bound shader state. This will set the vertex decl/shader, and pixel shader
	 * @param BoundShaderState - state resource
	 */
	virtual void RHISetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderState) = 0;

	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FVertexShaderRHIParamRef VertexShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) = 0;

	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FHullShaderRHIParamRef HullShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) = 0;

	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FDomainShaderRHIParamRef DomainShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) = 0;

	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) = 0;

	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FPixelShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) = 0;

	/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
	virtual void RHISetShaderTexture(FComputeShaderRHIParamRef PixelShader, uint32 TextureIndex, FTextureRHIParamRef NewTexture) = 0;

	/**
	 * Sets sampler state.
	 * @param GeometryShader	The geometry shader to set the sampler for.
	 * @param SamplerIndex		The index of the sampler.
	 * @param NewState			The new sampler state.
	 */
	virtual void RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) = 0;

	/**
	 * Sets sampler state.
	 * @param GeometryShader	The geometry shader to set the sampler for.
	 * @param SamplerIndex		The index of the sampler.
	 * @param NewState			The new sampler state.
	 */
	virtual void RHISetShaderSampler(FVertexShaderRHIParamRef VertexShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) = 0;

	/**
	 * Sets sampler state.
	 * @param GeometryShader	The geometry shader to set the sampler for.
	 * @param SamplerIndex		The index of the sampler.
	 * @param NewState			The new sampler state.
	 */
	virtual void RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) = 0;

	/**
	 * Sets sampler state.
	 * @param GeometryShader	The geometry shader to set the sampler for.
	 * @param SamplerIndex		The index of the sampler.
	 * @param NewState			The new sampler state.
	 */
	virtual void RHISetShaderSampler(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) = 0;

	/**
	 * Sets sampler state.
	 * @param GeometryShader	The geometry shader to set the sampler for.
	 * @param SamplerIndex		The index of the sampler.
	 * @param NewState			The new sampler state.
	 */
	virtual void RHISetShaderSampler(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) = 0;

	/**
	 * Sets sampler state.
	 * @param GeometryShader	The geometry shader to set the sampler for.
	 * @param SamplerIndex		The index of the sampler.
	 * @param NewState			The new sampler state.
	 */
	virtual void RHISetShaderSampler(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FSamplerStateRHIParamRef NewState) = 0;
	
	/**
	* Sets a compute shader UAV parameter.
	* @param ComputeShader	The compute shader to set the UAV for.
	* @param UAVIndex		The index of the UAVIndex.
	* @param UAV			The new UAV.	
	*/
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV) = 0;
	
	/**
	* Sets a compute shader counted UAV parameter and initial count
	* @param ComputeShader	The compute shader to set the UAV for.
	* @param UAVIndex		The index of the UAVIndex.
	* @param UAV			The new UAV.
	* @param InitialCount	The initial number of items in the UAV.	
	*/
	virtual void RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount) = 0;

	virtual void RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) = 0;

	virtual void RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) = 0;

	virtual void RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) = 0;

	virtual void RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) = 0;

	virtual void RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) = 0;

	virtual void RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV) = 0;

	virtual void RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) = 0;

	virtual void RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) = 0;

	virtual void RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) = 0;

	virtual void RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) = 0;

	virtual void RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) = 0;

	virtual void RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef Buffer) = 0;

	virtual void RHISetShaderParameter(FVertexShaderRHIParamRef VertexShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) = 0;

	virtual void RHISetShaderParameter(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) = 0;

	virtual void RHISetShaderParameter(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) = 0;

	virtual void RHISetShaderParameter(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) = 0;

	virtual void RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) = 0;

	virtual void RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue) = 0;

	virtual void RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewState, uint32 StencilRef) = 0;

	// Allows to set the blend state, parameter can be created with RHICreateBlendState()
	virtual void RHISetBlendState(FBlendStateRHIParamRef NewState, const FLinearColor& BlendFactor) = 0;

	virtual void RHISetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets, const FRHIDepthRenderTargetView* NewDepthStencilTarget, uint32 NumUAVs, const FUnorderedAccessViewRHIParamRef* UAVs) = 0;

	virtual void RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo) = 0;

	// Bind the clear state of the currently set rendertargets.  This is used by platforms which
	// need the state of the target when finalizing a hardware clear or a resource transition to SRV
	// The explicit bind is needed to support parallel rendering (propagate state between contexts).
	virtual void RHIBindClearMRTValues(bool bClearColor, bool bClearDepth, bool bClearStencil){}

	virtual void RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances) = 0;

	virtual void RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) = 0;

	virtual void RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances) = 0;

	// @param NumPrimitives need to be >0 
	virtual void RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBuffer, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances) = 0;

	virtual void RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType, FIndexBufferRHIParamRef IndexBuffer, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset) = 0;

	/**
	 * Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawPrimitiveUP
	 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
	 * @param NumPrimitives The number of primitives in the VertexData buffer
	 * @param NumVertices The number of vertices to be written
	 * @param VertexDataStride Size of each vertex 
	 * @param OutVertexData Reference to the allocated vertex memory
	 */
	virtual void RHIBeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData) = 0;

	/**
	 * Draw a primitive using the vertex data populated since RHIBeginDrawPrimitiveUP and clean up any memory as needed
	 */
	virtual void RHIEndDrawPrimitiveUP() = 0;

	/**
	 * Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawIndexedPrimitiveUP
	 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
	 * @param NumPrimitives The number of primitives in the VertexData buffer
	 * @param NumVertices The number of vertices to be written
	 * @param VertexDataStride Size of each vertex
	 * @param OutVertexData Reference to the allocated vertex memory
	 * @param MinVertexIndex The lowest vertex index used by the index buffer
	 * @param NumIndices Number of indices to be written
	 * @param IndexDataStride Size of each index (either 2 or 4 bytes)
	 * @param OutIndexData Reference to the allocated index memory
	 */
	virtual void RHIBeginDrawIndexedPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData) = 0;

	/**
	 * Draw a primitive using the vertex and index data populated since RHIBeginDrawIndexedPrimitiveUP and clean up any memory as needed
	 */
	virtual void RHIEndDrawIndexedPrimitiveUP() = 0;

	/*
	 * This method clears all MRT's, but to only one color value
	 * @param ExcludeRect within the viewport in pixels, is only a hint to optimize - if a fast clear can be done this is preferred
	 */
	virtual void RHIClear(bool bClearColor, const FLinearColor& Color, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntRect ExcludeRect) = 0;

	/*
	 * This method clears all MRT's to potentially different color values
	 * @param ExcludeRect within the viewport in pixels, is only a hint to optimize - if a fast clear can be done this is preferred
	 */
	virtual void RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntRect ExcludeRect) = 0;

	/**
	 * Enabled/Disables Depth Bounds Testing with the given min/max depth.
	 * @param bEnable	Enable(non-zero)/disable(zero) the depth bounds test
	 * @param MinDepth	The minimum depth for depth bounds test
	 * @param MaxDepth	The maximum depth for depth bounds test.
	 *					The valid values for fMinDepth and fMaxDepth are such that 0 <= fMinDepth <= fMaxDepth <= 1
	 */
	virtual void RHIEnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth) = 0;

	virtual void RHIPushEvent(const TCHAR* Name, FColor Color) = 0;

	virtual void RHIPopEvent() = 0;

	virtual void RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRef, FTextureRHIParamRef NewTexture) = 0;
};

/** The interface which is implemented by the dynamically bound RHI. */
class RHI_API FDynamicRHI
{
public:

	/** Declare a virtual destructor, so the dynamic RHI can be deleted without knowing its type. */
	virtual ~FDynamicRHI() {}

	/** Initializes the RHI; separate from IDynamicRHIModule::CreateRHI so that GDynamicRHI is set when it is called. */
	virtual void Init() = 0;

	/** Called after the RHI is initialized; before the render thread is started. */
	virtual void PostInit() {}

	/** Shutdown the RHI; handle shutdown and resource destruction before the RHI's actual destructor is called (so that all resources of the RHI are still available for shutdown). */
	virtual void Shutdown() = 0;


	/////// RHI Methods

	// FlushType: Thread safe
	virtual FSamplerStateRHIRef RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer) = 0;

	// FlushType: Thread safe
	virtual FRasterizerStateRHIRef RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer) = 0;

	// FlushType: Thread safe
	virtual FDepthStencilStateRHIRef RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer) = 0;

	// FlushType: Thread safe
	virtual FBlendStateRHIRef RHICreateBlendState(const FBlendStateInitializerRHI& Initializer) = 0;

	// FlushType: Wait RHI Thread
	virtual FVertexDeclarationRHIRef RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements) = 0;

	// FlushType: Wait RHI Thread
	virtual FPixelShaderRHIRef RHICreatePixelShader(const TArray<uint8>& Code) = 0;

	// FlushType: Wait RHI Thread
	virtual FVertexShaderRHIRef RHICreateVertexShader(const TArray<uint8>& Code) = 0;

	// FlushType: Wait RHI Thread
	virtual FHullShaderRHIRef RHICreateHullShader(const TArray<uint8>& Code) = 0;

	// FlushType: Wait RHI Thread
	virtual FDomainShaderRHIRef RHICreateDomainShader(const TArray<uint8>& Code) = 0;

	// FlushType: Wait RHI Thread
	virtual FGeometryShaderRHIRef RHICreateGeometryShader(const TArray<uint8>& Code) = 0;

	/** Creates a geometry shader with stream output ability, defined by ElementList. */
	// FlushType: Wait RHI Thread
	virtual FGeometryShaderRHIRef RHICreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream) = 0;

	// Some RHIs can have pending messages/logs for error tracking, or debug modes
	virtual void FlushPendingLogs() {}

	// FlushType: Wait RHI Thread
	virtual FComputeShaderRHIRef RHICreateComputeShader(const TArray<uint8>& Code) = 0;

	/**
	* Creates a compute fence.  Compute fences are named GPU fences which can be written to once before resetting.
	* A command to write the fence must be enqueued before any commands to wait on them.  This is enforced on the CPU to avoid GPU hangs.
	* @param Name - Friendly name for the Fence.  e.g. ReflectionEnvironmentComplete
	* @return The new Fence.
	*/
	// FlushType: Thread safe, but varies depending on the RHI	
	virtual FComputeFenceRHIRef RHICreateComputeFence(const FName& Name)
	{
		return new FRHIComputeFence(Name);
	}

	/**
	 * Creates a bound shader state instance which encapsulates a decl, vertex shader, hull shader, domain shader and pixel shader
	 * CAUTION: Even though this is marked as threadsafe, it is only valid to call from the render thread or the RHI thread. It need not be threadsafe unless the RHI support parallel translation.
	 * CAUTION: Platforms that support RHIThread but don't actually have a threadsafe implementation must flush internally with FScopedRHIThreadStaller StallRHIThread(FRHICommandListExecutor::GetImmediateCommandList()); when the call is from the render thread
	 * @param VertexDeclaration - existing vertex decl
	 * @param VertexShader - existing vertex shader
	 * @param HullShader - existing hull shader
	 * @param DomainShader - existing domain shader
	 * @param GeometryShader - existing geometry shader
	 * @param PixelShader - existing pixel shader
	 */
	// FlushType: Thread safe, but varies depending on the RHI
	virtual FBoundShaderStateRHIRef RHICreateBoundShaderState(FVertexDeclarationRHIParamRef VertexDeclaration, FVertexShaderRHIParamRef VertexShader, FHullShaderRHIParamRef HullShader, FDomainShaderRHIParamRef DomainShader, FPixelShaderRHIParamRef PixelShader, FGeometryShaderRHIParamRef GeometryShader) = 0;

	/**
	 * Creates a uniform buffer.  The contents of the uniform buffer are provided in a parameter, and are immutable.
	 * CAUTION: Even though this is marked as threadsafe, it is only valid to call from the render thread or the RHI thread. Thus is need not be threadsafe on platforms that do not support or aren't using an RHIThread
	 * @param Contents - A pointer to a memory block of size NumBytes that is copied into the new uniform buffer.
	 * @param NumBytes - The number of bytes the uniform buffer should contain.
	 * @return The new uniform buffer.
	 */
	// FlushType: Thread safe, but varies depending on the RHI
	virtual FUniformBufferRHIRef RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage) = 0;

	// FlushType: Wait RHI Thread
	virtual FIndexBufferRHIRef RHICreateIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo) = 0;

	// FlushType: Flush RHI Thread
	virtual void* RHILockIndexBuffer(FIndexBufferRHIParamRef IndexBuffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode) = 0;

	// FlushType: Flush RHI Thread
	virtual void RHIUnlockIndexBuffer(FIndexBufferRHIParamRef IndexBuffer) = 0;

	/**
	 * @param ResourceArray - An optional pointer to a resource array containing the resource's data.
	 */
	// FlushType: Wait RHI Thread
	virtual FVertexBufferRHIRef RHICreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo) = 0;

	// FlushType: Flush RHI Thread
	virtual void* RHILockVertexBuffer(FVertexBufferRHIParamRef VertexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode) = 0;

	// FlushType: Flush RHI Thread
	virtual void RHIUnlockVertexBuffer(FVertexBufferRHIParamRef VertexBuffer) = 0;

	/** Copies the contents of one vertex buffer to another vertex buffer.  They must have identical sizes. */
	// FlushType: Flush Immediate (seems dangerous)
	virtual void RHICopyVertexBuffer(FVertexBufferRHIParamRef SourceBuffer, FVertexBufferRHIParamRef DestBuffer) = 0;

	/**
	 * @param ResourceArray - An optional pointer to a resource array containing the resource's data.
	 */
	// FlushType: Wait RHI Thread
	virtual FStructuredBufferRHIRef RHICreateStructuredBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo) = 0;

	// FlushType: Flush RHI Thread
	virtual void* RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode) = 0;

	// FlushType: Flush RHI Thread
	virtual void RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBuffer) = 0;

	/** Creates an unordered access view of the given structured buffer. */
	// FlushType: Wait RHI Thread
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBuffer, bool bUseUAVCounter, bool bAppendBuffer) = 0;

	/** Creates an unordered access view of the given texture. */
	// FlushType: Wait RHI Thread
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FTextureRHIParamRef Texture, uint32 MipLevel) = 0;

	/** Creates an unordered access view of the given vertex buffer. */
	// FlushType: Wait RHI Thread
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FVertexBufferRHIParamRef VertexBuffer, uint8 Format) = 0;

	/** Creates a shader resource view of the given structured buffer. */
	// FlushType: Wait RHI Thread
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FStructuredBufferRHIParamRef StructuredBuffer) = 0;

	/** Creates a shader resource view of the given vertex buffer. */
	// FlushType: Wait RHI Thread
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint8 Format) = 0;

	/** Creates a shader resource view of the given index buffer. */
	// FlushType: Wait RHI Thread
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FIndexBufferRHIParamRef Buffer) = 0;

	/**
	 * Computes the total size of a 2D texture with the specified parameters.
	 *
	 * @param SizeX - width of the texture to compute
	 * @param SizeY - height of the texture to compute
	 * @param Format - EPixelFormat texture format
	 * @param NumMips - number of mips to compute or 0 for full mip pyramid
	 * @param NumSamples - number of MSAA samples, usually 1
	 * @param Flags - ETextureCreateFlags creation flags
	 * @param OutAlign - Alignment required for this texture.  Output parameter.
	 */
	// FlushType: Thread safe
	virtual uint64 RHICalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign) = 0;

	/**
	* Computes the total size of a 3D texture with the specified parameters.
	* @param SizeX - width of the texture to create
	* @param SizeY - height of the texture to create
	* @param SizeZ - depth of the texture to create
	* @param Format - EPixelFormat texture format
	* @param NumMips - number of mips to generate or 0 for full mip pyramid
	* @param Flags - ETextureCreateFlags creation flags
	* @param OutAlign - Alignment required for this texture.  Output parameter.
	*/
	// FlushType: Thread safe
	virtual uint64 RHICalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign) = 0;

	/**
	* Computes the total size of a cube texture with the specified parameters.
	* @param Size - width/height of the texture to create
	* @param Format - EPixelFormat texture format
	* @param NumMips - number of mips to generate or 0 for full mip pyramid
	* @param Flags - ETextureCreateFlags creation flags
	* @param OutAlign - Alignment required for this texture.  Output parameter.
	*/
	// FlushType: Thread safe
	virtual uint64 RHICalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign) = 0;

	/**
	 * Retrieves texture memory stats.
	 * safe to call on the main thread
	 */
	// FlushType: Thread safe
	virtual void RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats) = 0;

	/**
	 * Fills a texture with to visualize the texture pool memory.
	 *
	 * @param	TextureData		Start address
	 * @param	SizeX			Number of pixels along X
	 * @param	SizeY			Number of pixels along Y
	 * @param	Pitch			Number of bytes between each row
	 * @param	PixelSize		Number of bytes each pixel represents
	 *
	 * @return true if successful, false otherwise
	 */
	// FlushType: Flush Immediate
	virtual bool RHIGetTextureMemoryVisualizeData(FColor* TextureData, int32 SizeX, int32 SizeY, int32 Pitch, int32 PixelSize) = 0;

	// FlushType: Wait RHI Thread
	virtual FTextureReferenceRHIRef RHICreateTextureReference(FLastRenderTimeContainer* LastRenderTime) = 0;

	/**
	* Creates a 2D RHI texture resource
	* @param SizeX - width of the texture to create
	* @param SizeY - height of the texture to create
	* @param Format - EPixelFormat texture format
	* @param NumMips - number of mips to generate or 0 for full mip pyramid
	* @param NumSamples - number of MSAA samples, usually 1
	* @param Flags - ETextureCreateFlags creation flags
	*/
	// FlushType: Wait RHI Thread
	virtual FTexture2DRHIRef RHICreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo) = 0;

	/**
	* Creates an FStructuredBuffer for the RT write mask of a render target 
	* @param RenderTarget - the RT to create the buffer for
	*/
	virtual FStructuredBufferRHIRef RHICreateRTWriteMaskBuffer(FTexture2DRHIParamRef RenderTarget)
	{
		return nullptr;
	}

	/**
	 * Thread-safe function that can be used to create a texture outside of the
	 * rendering thread. This function can ONLY be called if GRHISupportsAsyncTextureCreation
	 * is true.  Cannot create rendertargets with this method.
	 * @param SizeX - width of the texture to create
	 * @param SizeY - height of the texture to create
	 * @param Format - EPixelFormat texture format
	 * @param NumMips - number of mips to generate or 0 for full mip pyramid
	 * @param Flags - ETextureCreateFlags creation flags
	 * @param InitialMipData - pointers to mip data with which to create the texture
	 * @param NumInitialMips - how many mips are provided in InitialMipData
	 * @returns a reference to a 2D texture resource
	 */
	// FlushType: Thread safe
	virtual FTexture2DRHIRef RHIAsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, void** InitialMipData, uint32 NumInitialMips) = 0;

	/**
	 * Copies shared mip levels from one texture to another. The textures must have
	 * full mip chains, share the same format, and have the same aspect ratio. This
	 * copy will not cause synchronization with the GPU.
	 * @param DestTexture2D - destination texture
	 * @param SrcTexture2D - source texture
	 */
	// FlushType: Flush RHI Thread
	virtual void RHICopySharedMips(FTexture2DRHIParamRef DestTexture2D, FTexture2DRHIParamRef SrcTexture2D) = 0;

	/**
	* Creates a Array RHI texture resource
	* @param SizeX - width of the texture to create
	* @param SizeY - height of the texture to create
	* @param SizeZ - depth of the texture to create
	* @param Format - EPixelFormat texture format
	* @param NumMips - number of mips to generate or 0 for full mip pyramid
	* @param Flags - ETextureCreateFlags creation flags
	*/
	// FlushType: Wait RHI Thread
	virtual FTexture2DArrayRHIRef RHICreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo) = 0;

	/**
	* Creates a 3d RHI texture resource
	* @param SizeX - width of the texture to create
	* @param SizeY - height of the texture to create
	* @param SizeZ - depth of the texture to create
	* @param Format - EPixelFormat texture format
	* @param NumMips - number of mips to generate or 0 for full mip pyramid
	* @param Flags - ETextureCreateFlags creation flags
	*/
	// FlushType: Wait RHI Thread
	virtual FTexture3DRHIRef RHICreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo) = 0;

	/**
	 * @param Ref may be 0
	 */
	// FlushType: Thread safe
	virtual void RHIGetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo) = 0;

	/**
	* Creates a shader resource view for a 2d texture, viewing only a single
	* mip level. Useful when rendering to one mip while sampling from another.
	*/
	// FlushType: Wait RHI Thread
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel) = 0;

	/**FRHIResourceInfo
	* Creates a shader resource view for a 2d texture, with a different
	* format from the original.  Useful when sampling stencil.
	*/
	// FlushType: Wait RHI Thread
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format) = 0;

	/**
	* Creates a shader resource view for a 3d texture, viewing only a single
	* mip level.
	*/
	// FlushType: Wait RHI Thread
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel) = 0;

	/**
	* Creates a shader resource view for a 2d texture array, viewing only a single
	* mip level.
	*/
	// FlushType: Wait RHI Thread
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel) = 0;

	/**
	* Creates a shader resource view for a cube texture, viewing only a single
	* mip level.
	*/
	// FlushType: Wait RHI Thread
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView(FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel) = 0;

	/**
	* Generates mip maps for a texture.
	*/
	// FlushType: Flush Immediate (NP: this should be queued on the command list for RHI thread execution, not flushed)
	virtual void RHIGenerateMips(FTextureRHIParamRef Texture) = 0;

	/**
	 * Computes the size in memory required by a given texture.
	 *
	 * @param	TextureRHI		- Texture we want to know the size of, 0 is safely ignored
	 * @return					- Size in Bytes
	 */
	// FlushType: Thread safe
	virtual uint32 RHIComputeMemorySize(FTextureRHIParamRef TextureRHI) = 0;

	/**
	 * Starts an asynchronous texture reallocation. It may complete immediately if the reallocation
	 * could be performed without any reshuffling of texture memory, or if there isn't enough memory.
	 * The specified status counter will be decremented by 1 when the reallocation is complete (success or failure).
	 *
	 * Returns a new reference to the texture, which will represent the new mip count when the reallocation is complete.
	 * RHIFinalizeAsyncReallocateTexture2D() must be called to complete the reallocation.
	 *
	 * @param Texture2D		- Texture to reallocate
	 * @param NewMipCount	- New number of mip-levels
	 * @param NewSizeX		- New width, in pixels
	 * @param NewSizeY		- New height, in pixels
	 * @param RequestStatus	- Will be decremented by 1 when the reallocation is complete (success or failure).
	 * @return				- New reference to the texture, or an invalid reference upon failure
	 */
	// FlushType: Flush RHI Thread
	// NP: Note that no RHI currently implements this as an async call, we should simplify the API.
	virtual FTexture2DRHIRef RHIAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus) = 0;

	/**
	 * Finalizes an async reallocation request.
	 * If bBlockUntilCompleted is false, it will only poll the status and finalize if the reallocation has completed.
	 *
	 * @param Texture2D				- Texture to finalize the reallocation for
	 * @param bBlockUntilCompleted	- Whether the function should block until the reallocation has completed
	 * @return						- Current reallocation status:
	 *	TexRealloc_Succeeded	Reallocation succeeded
	 *	TexRealloc_Failed		Reallocation failed
	 *	TexRealloc_InProgress	Reallocation is still in progress, try again later
	 */
	// FlushType: Wait RHI Thread
	virtual ETextureReallocationStatus RHIFinalizeAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted) = 0;

	/**
	 * Cancels an async reallocation for the specified texture.
	 * This should be called for the new texture, not the original.
	 *
	 * @param Texture				Texture to cancel
	 * @param bBlockUntilCompleted	If true, blocks until the cancellation is fully completed
	 * @return						Reallocation status
	 */
	// FlushType: Wait RHI Thread
	virtual ETextureReallocationStatus RHICancelAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted) = 0;

	/**
	* Locks an RHI texture's mip-map for read/write operations on the CPU
	* @param Texture - the RHI texture resource to lock, must not be 0
	* @param MipIndex - index of the mip level to lock
	* @param LockMode - Whether to lock the texture read-only instead of write-only
	* @param DestStride - output to retrieve the textures row stride (pitch)
	* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
	* @return pointer to the CPU accessible resource data
	*/
	// FlushType: Flush RHI Thread
	virtual void* RHILockTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) = 0;

	/**
	* Unlocks a previously locked RHI texture resource
	* @param Texture - the RHI texture resource to unlock, must not be 0
	* @param MipIndex - index of the mip level to unlock
	* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
	*/
	// FlushType: Flush RHI Thread
	virtual void RHIUnlockTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, bool bLockWithinMiptail) = 0;

	/**
	* Locks an RHI texture's mip-map for read/write operations on the CPU
	* @param Texture - the RHI texture resource to lock
	* @param MipIndex - index of the mip level to lock
	* @param LockMode - Whether to lock the texture read-only instead of write-only
	* @param DestStride - output to retrieve the textures row stride (pitch)
	* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
	* @return pointer to the CPU accessible resource data
	*/
	// FlushType: Flush RHI Thread
	virtual void* RHILockTexture2DArray(FTexture2DArrayRHIParamRef Texture, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) = 0;

	/**
	* Unlocks a previously locked RHI texture resource
	* @param Texture - the RHI texture resource to unlock
	* @param MipIndex - index of the mip level to unlock
	* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
	*/
	// FlushType: Flush RHI Thread
	virtual void RHIUnlockTexture2DArray(FTexture2DArrayRHIParamRef Texture, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail) = 0;

	/**
	* Updates a region of a 2D texture from system memory
	* @param Texture - the RHI texture resource to update
	* @param MipIndex - mip level index to be modified
	* @param UpdateRegion - The rectangle to copy source image data from
	* @param SourcePitch - size in bytes of each row of the source image
	* @param SourceData - source image data, starting at the upper left corner of the source rectangle (in same pixel format as texture)
	*/
	// FlushType: Flush RHI Thread
	virtual void RHIUpdateTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData) = 0;

	/**
	* Updates a region of a 3D texture from system memory
	* @param Texture - the RHI texture resource to update
	* @param MipIndex - mip level index to be modified
	* @param UpdateRegion - The rectangle to copy source image data from
	* @param SourceRowPitch - size in bytes of each row of the source image, usually Bpp * SizeX
	* @param SourceDepthPitch - size in bytes of each depth slice of the source image, usually Bpp * SizeX * SizeY
	* @param SourceData - source image data, starting at the upper left corner of the source rectangle (in same pixel format as texture)
	*/
	// FlushType: Flush RHI Thread
	virtual void RHIUpdateTexture3D(FTexture3DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData) = 0;

	/**
	* Creates a Cube RHI texture resource
	* @param Size - width/height of the texture to create
	* @param Format - EPixelFormat texture format
	* @param NumMips - number of mips to generate or 0 for full mip pyramid
	* @param Flags - ETextureCreateFlags creation flags
	*/
	// FlushType: Wait RHI Thread
	virtual FTextureCubeRHIRef RHICreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo) = 0;

	/**
	* Creates a Cube Array RHI texture resource
	* @param Size - width/height of the texture to create
	* @param ArraySize - number of array elements of the texture to create
	* @param Format - EPixelFormat texture format
	* @param NumMips - number of mips to generate or 0 for full mip pyramid
	* @param Flags - ETextureCreateFlags creation flags
	*/
	// FlushType: Wait RHI Thread
	virtual FTextureCubeRHIRef RHICreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo) = 0;

	/**
	* Locks an RHI texture's mip-map for read/write operations on the CPU
	* @param Texture - the RHI texture resource to lock
	* @param MipIndex - index of the mip level to lock
	* @param LockMode - Whether to lock the texture read-only instead of write-only.
	* @param DestStride - output to retrieve the textures row stride (pitch)
	* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
	* @return pointer to the CPU accessible resource data
	*/
	// FlushType: Flush RHI Thread
	virtual void* RHILockTextureCubeFace(FTextureCubeRHIParamRef Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail) = 0;

	/**
	* Unlocks a previously locked RHI texture resource
	* @param Texture - the RHI texture resource to unlock
	* @param MipIndex - index of the mip level to unlock
	* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
	*/
	// FlushType: Flush RHI Thread
	virtual void RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail) = 0;

	// FlushType: Thread safe
	virtual void RHIBindDebugLabelName(FTextureRHIParamRef Texture, const TCHAR* Name) = 0;
	virtual void RHIBindDebugLabelName(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const TCHAR* Name) {}

	/**
	 * Reads the contents of a texture to an output buffer (non MSAA and MSAA) and returns it as a FColor array.
	 * If the format or texture type is unsupported the OutData array will be size 0
	 */
	// FlushType: Flush Immediate (seems wrong)
	virtual void RHIReadSurfaceData(FTextureRHIParamRef Texture, FIntRect Rect, TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags) = 0;

	// FlushType: Flush Immediate (seems wrong)
	virtual void RHIReadSurfaceData(FTextureRHIParamRef Texture, FIntRect Rect, TArray<FLinearColor>& OutData, FReadSurfaceDataFlags InFlags) {}

	/** Watch out for OutData to be 0 (can happen on DXGI_ERROR_DEVICE_REMOVED), don't call RHIUnmapStagingSurface in that case. */
	// FlushType: Flush Immediate (seems wrong)
	virtual void RHIMapStagingSurface(FTextureRHIParamRef Texture, void*& OutData, int32& OutWidth, int32& OutHeight) = 0;

	/** call after a succesful RHIMapStagingSurface() call */
	// FlushType: Flush Immediate (seems wrong)
	virtual void RHIUnmapStagingSurface(FTextureRHIParamRef Texture) = 0;

	// FlushType: Flush Immediate (seems wrong)
	virtual void RHIReadSurfaceFloatData(FTextureRHIParamRef Texture, FIntRect Rect, TArray<FFloat16Color>& OutData, ECubeFace CubeFace, int32 ArrayIndex, int32 MipIndex) = 0;

	// FlushType: Flush Immediate (seems wrong)
	virtual void RHIRead3DSurfaceFloatData(FTextureRHIParamRef Texture, FIntRect Rect, FIntPoint ZMinMax, TArray<FFloat16Color>& OutData) = 0;

	// FlushType: Wait RHI Thread
	virtual FRenderQueryRHIRef RHICreateRenderQuery(ERenderQueryType QueryType) = 0;

	// CAUTION: Even though this is marked as threadsafe, it is only valid to call from the render thread. It is need not be threadsafe on platforms that do not support or aren't using an RHIThread
	// FlushType: Thread safe, but varies by RHI
	virtual bool RHIGetRenderQueryResult(FRenderQueryRHIParamRef RenderQuery, uint64& OutResult, bool bWait) = 0;

	// With RHI thread, this is the current backbuffer from the perspective of the render thread.
	// FlushType: Thread safe
	virtual FTexture2DRHIRef RHIGetViewportBackBuffer(FViewportRHIParamRef Viewport) = 0;

	// Only relevant with an RHI thread, this advances the backbuffer for the purpose of GetViewportBackBuffer
	// FlushType: Thread safe
	virtual void RHIAdvanceFrameForGetViewportBackBuffer() = 0;

	/*
	 * Acquires or releases ownership of the platform-specific rendering context for the calling thread
	 */
	// FlushType: Flush RHI Thread
	virtual void RHIAcquireThreadOwnership() = 0;

	// FlushType: Flush RHI Thread
	virtual void RHIReleaseThreadOwnership() = 0;

	// Flush driver resources. Typically called when switching contexts/threads
	// FlushType: Flush RHI Thread
	virtual void RHIFlushResources() = 0;

	/*
	 * Returns the total GPU time taken to render the last frame. Same metric as FPlatformTime::Cycles().
	 */
	// FlushType: Thread safe
	virtual uint32 RHIGetGPUFrameCycles() = 0;

	//  must be called from the main thread.
	// FlushType: Thread safe
	virtual FViewportRHIRef RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) = 0;

	//  must be called from the main thread.
	// FlushType: Thread safe
	virtual void RHIResizeViewport(FViewportRHIParamRef Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen) = 0;

	//  must be called from the main thread.
	// FlushType: Thread safe
	virtual void RHITick(float DeltaTime) = 0;

	/** Sets stream output targets, for use with a geometry shader created with RHICreateGeometryShaderWithStreamOutput. */
	//@todo this should be a CMDLIST method
	// FlushType: Flush Immediate (seems wrong)
	virtual void RHISetStreamOutTargets(uint32 NumTargets, const FVertexBufferRHIParamRef* VertexBuffers, const uint32* Offsets) = 0;

	// Each RHI should flush if it needs to when implementing this method.
	virtual void RHIDiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask) = 0;

	// Blocks the CPU until the GPU catches up and goes idle.
	// FlushType: Flush Immediate (seems wrong)
	virtual void RHIBlockUntilGPUIdle() = 0;

	// Operations to suspend title rendering and yield control to the system
	// FlushType: Thread safe
	virtual void RHISuspendRendering() {};

	// FlushType: Thread safe
	virtual void RHIResumeRendering() {};

	// FlushType: Flush Immediate
	virtual bool RHIIsRenderingSuspended() { return false; };

	// FlushType: Flush Immediate
	virtual bool RHIEnqueueDecompress(uint8_t* SrcBuffer, uint8_t* DestBuffer, int CompressedSize, void* ErrorCodeBuffer) { return false; }
	virtual bool RHIEnqueueCompress(uint8_t* SrcBuffer, uint8_t* DestBuffer, int UnCompressedSize, void* ErrorCodeBuffer) { return false; }

	// FlushType: Flush Immediate
	virtual void RHIRecreateRecursiveBoundShaderStates() {}

	/**
	 *	Retrieve available screen resolutions.
	 *
	 *	@param	Resolutions			TArray<FScreenResolutionRHI> parameter that will be filled in.
	 *	@param	bIgnoreRefreshRate	If true, ignore refresh rates.
	 *
	 *	@return	bool				true if successfully filled the array
	 */
	// FlushType: Thread safe
	virtual bool RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate) = 0;

	/**
	 * Returns a supported screen resolution that most closely matches input.
	 * @param Width - Input: Desired resolution width in pixels. Output: A width that the platform supports.
	 * @param Height - Input: Desired resolution height in pixels. Output: A height that the platform supports.
	 */
	// FlushType: Thread safe
	virtual void RHIGetSupportedResolution(uint32& Width, uint32& Height) = 0;

	/**
	 * Function that is used to allocate / free space used for virtual texture mip levels.
	 * Make sure you also update the visible mip levels.
	 * @param Texture - the texture to update, must have been created with TexCreate_Virtual
	 * @param FirstMip - the first mip that should be in memory
	 */
	// FlushType: Wait RHI Thread
	virtual void RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef Texture, uint32 FirstMip) = 0;

	/**
	 * Function that can be used to update which is the first visible mip to the GPU.
	 * @param Texture - the texture to update, must have been created with TexCreate_Virtual
	 * @param FirstMip - the first mip that should be visible to the GPU
	 */
	// FlushType: Wait RHI Thread
	virtual void RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef Texture, uint32 FirstMip) = 0;

	// FlushType: Wait RHI Thread
	virtual void RHIExecuteCommandList(FRHICommandList* CmdList) = 0;

	/**
	 * Provides access to the native device. Generally this should be avoided but is useful for third party plugins.
	 */
	// FlushType: Flush RHI Thread
	virtual void* RHIGetNativeDevice() = 0;

	// FlushType: Thread safe
	virtual IRHICommandContext* RHIGetDefaultContext() = 0;

	// FlushType: Thread safe
	virtual IRHIComputeContext* RHIGetDefaultAsyncComputeContext()
	{
		IRHIComputeContext* ComputeContext = RHIGetDefaultContext();
		// On platforms that support non-async compute we set this to the normal context.  It won't be async, but the high level
		// code can be agnostic if it wants to be.
		return ComputeContext;
	}

	// FlushType: Thread safe
	virtual class IRHICommandContextContainer* RHIGetCommandContextContainer() = 0;

	///////// Pass through functions that allow RHIs to optimize certain calls.
	virtual FVertexBufferRHIRef CreateAndLockVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer);
	virtual FIndexBufferRHIRef CreateAndLockIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer);

	virtual FVertexBufferRHIRef CreateVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo);
	virtual FShaderResourceViewRHIRef CreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint8 Format);
	virtual FShaderResourceViewRHIRef CreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef Buffer);
	virtual void* LockVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode);
	virtual void UnlockVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer);
	virtual FTexture2DRHIRef AsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus);
	virtual ETextureReallocationStatus FinalizeAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted);
	virtual ETextureReallocationStatus CancelAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted);
	virtual FIndexBufferRHIRef CreateIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo);
	virtual void* LockIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef IndexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode);
	virtual void UnlockIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef IndexBuffer);
	virtual FVertexDeclarationRHIRef CreateVertexDeclaration_RenderThread(class FRHICommandListImmediate& RHICmdList, const FVertexDeclarationElementList& Elements);
	virtual void UpdateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData);
	virtual void* LockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush = true);
	virtual void UnlockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush = true);

	virtual FTexture2DRHIRef RHICreateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo);
	virtual FTexture2DArrayRHIRef RHICreateTexture2DArray_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo);
	virtual FTexture3DRHIRef RHICreateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo);
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FStructuredBufferRHIParamRef StructuredBuffer, bool bUseUAVCounter, bool bAppendBuffer);
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTextureRHIParamRef Texture, uint32 MipLevel);
	virtual FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint8 Format);
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel);
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format);
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel);
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel);
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel);
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint8 Format);
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FIndexBufferRHIParamRef Buffer);
	virtual FShaderResourceViewRHIRef RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FStructuredBufferRHIParamRef StructuredBuffer);
	virtual FTextureCubeRHIRef RHICreateTextureCube_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo);
	virtual FTextureCubeRHIRef RHICreateTextureCubeArray_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo);
	virtual FRenderQueryRHIRef RHICreateRenderQuery_RenderThread(class FRHICommandListImmediate& RHICmdList, ERenderQueryType QueryType);

	//Utilities
	virtual void EnableIdealGPUCaptureOptions(bool bEnable);

	/* Copy the source box pixels in the destination box texture, return true if implemented for the current platform*/
	virtual bool RHICopySubTextureRegion(FTexture2DRHIParamRef SourceTexture, FTexture2DRHIParamRef DestinationTexture, FBox2D SourceBox, FBox2D DestinationBox) { return false; }
};

/** A global pointer to the dynamically bound RHI implementation. */
extern RHI_API FDynamicRHI* GDynamicRHI;

FORCEINLINE FSamplerStateRHIRef RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
	return GDynamicRHI->RHICreateSamplerState(Initializer);
}

FORCEINLINE FRasterizerStateRHIRef RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
	return GDynamicRHI->RHICreateRasterizerState(Initializer);
}

FORCEINLINE FDepthStencilStateRHIRef RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
{
	return GDynamicRHI->RHICreateDepthStencilState(Initializer);
}

FORCEINLINE FBlendStateRHIRef RHICreateBlendState(const FBlendStateInitializerRHI& Initializer)
{
	return GDynamicRHI->RHICreateBlendState(Initializer);
}

FORCEINLINE FBoundShaderStateRHIRef RHICreateBoundShaderState(FVertexDeclarationRHIParamRef VertexDeclaration, FVertexShaderRHIParamRef VertexShader, FHullShaderRHIParamRef HullShader, FDomainShaderRHIParamRef DomainShader, FPixelShaderRHIParamRef PixelShader, FGeometryShaderRHIParamRef GeometryShader)
{
	return GDynamicRHI->RHICreateBoundShaderState(VertexDeclaration, VertexShader, HullShader, DomainShader, PixelShader, GeometryShader);
}

FORCEINLINE FUniformBufferRHIRef RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage)
{
	return GDynamicRHI->RHICreateUniformBuffer(Contents, Layout, Usage);
}

FORCEINLINE uint64 RHICalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign)
{
	return GDynamicRHI->RHICalcTexture2DPlatformSize(SizeX, SizeY, Format, NumMips, NumSamples, Flags, OutAlign);
}

FORCEINLINE uint64 RHICalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	return GDynamicRHI->RHICalcTexture3DPlatformSize(SizeX, SizeY, SizeZ, Format, NumMips, Flags, OutAlign);
}

FORCEINLINE uint64 RHICalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	return GDynamicRHI->RHICalcTextureCubePlatformSize(Size, Format, NumMips, Flags, OutAlign);
}

FORCEINLINE void RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats)
{
	 GDynamicRHI->RHIGetTextureMemoryStats(OutStats);
}

FORCEINLINE void RHIGetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo)
{
	return GDynamicRHI->RHIGetResourceInfo(Ref, OutInfo);
}

FORCEINLINE uint32 RHIComputeMemorySize(FTextureRHIParamRef TextureRHI)
{
	return GDynamicRHI->RHIComputeMemorySize(TextureRHI);
}

FORCEINLINE void RHIBindDebugLabelName(FTextureRHIParamRef Texture, const TCHAR* Name)
{
	 GDynamicRHI->RHIBindDebugLabelName(Texture, Name);
}

FORCEINLINE void RHIBindDebugLabelName(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const TCHAR* Name)
{
	GDynamicRHI->RHIBindDebugLabelName(UnorderedAccessViewRHI, Name);
}

FORCEINLINE bool RHIGetRenderQueryResult(FRenderQueryRHIParamRef RenderQuery, uint64& OutResult, bool bWait)
{
	return GDynamicRHI->RHIGetRenderQueryResult(RenderQuery, OutResult, bWait);
}

FORCEINLINE FTexture2DRHIRef RHIGetViewportBackBuffer(FViewportRHIParamRef Viewport)
{
	return GDynamicRHI->RHIGetViewportBackBuffer(Viewport);
}

FORCEINLINE void RHIAdvanceFrameForGetViewportBackBuffer()
{
	return GDynamicRHI->RHIAdvanceFrameForGetViewportBackBuffer();
}

FORCEINLINE uint32 RHIGetGPUFrameCycles()
{
	return GDynamicRHI->RHIGetGPUFrameCycles();
}

FORCEINLINE FViewportRHIRef RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
{
	return GDynamicRHI->RHICreateViewport(WindowHandle, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat);
}

FORCEINLINE void RHIResizeViewport(FViewportRHIParamRef Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen)
{
	 GDynamicRHI->RHIResizeViewport(Viewport, SizeX, SizeY, bIsFullscreen);
}

FORCEINLINE void RHITick(float DeltaTime)
{
	 GDynamicRHI->RHITick(DeltaTime);
}

FORCEINLINE void RHISuspendRendering()
{
	 GDynamicRHI->RHISuspendRendering();
}

FORCEINLINE void RHIResumeRendering()
{
	 GDynamicRHI->RHIResumeRendering();
}

FORCEINLINE bool RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
	return GDynamicRHI->RHIGetAvailableResolutions(Resolutions, bIgnoreRefreshRate);
}

FORCEINLINE void RHIGetSupportedResolution(uint32& Width, uint32& Height)
{
	 GDynamicRHI->RHIGetSupportedResolution(Width, Height);
}

FORCEINLINE class IRHICommandContext* RHIGetDefaultContext()
{
	return GDynamicRHI->RHIGetDefaultContext();
}

FORCEINLINE class IRHIComputeContext* RHIGetDefaultAsyncComputeContext()
{
	return GDynamicRHI->RHIGetDefaultAsyncComputeContext();
}



FORCEINLINE class IRHICommandContextContainer* RHIGetCommandContextContainer()
{
	return GDynamicRHI->RHIGetCommandContextContainer();
}





/**
 * Defragment the texture pool.
 */
inline void appDefragmentTexturePool() {}

/**
 * Checks if the texture data is allocated within the texture pool or not.
 */
inline bool appIsPoolTexture( FTextureRHIParamRef TextureRHI ) { return false; }

/**
 * Log the current texture memory stats.
 *
 * @param Message	This text will be included in the log
 */
inline void appDumpTextureMemoryStats(const TCHAR* /*Message*/) {}


/** Defines the interface of a module implementing a dynamic RHI. */
class IDynamicRHIModule : public IModuleInterface
{
public:

	/** Checks whether the RHI is supported by the current system. */
	virtual bool IsSupported() = 0;

	/** Creates a new instance of the dynamic RHI implemented by the module. */
	virtual FDynamicRHI* CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel = ERHIFeatureLevel::Num) = 0;
};

/**
 *	Each platform that utilizes dynamic RHIs should implement this function
 *	Called to create the instance of the dynamic RHI.
 */
FDynamicRHI* PlatformCreateDynamicRHI();

