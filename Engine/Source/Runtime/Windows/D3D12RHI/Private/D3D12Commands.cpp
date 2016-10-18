// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Commands.cpp: D3D RHI commands implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "Windows/D3D12RHIPrivateUtil.h"
#include "StaticBoundShaderState.h"
#include "GlobalShader.h"
#include "OneColorShader.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "ShaderParameterUtils.h"
#include "ScreenRendering.h"

namespace D3D12RHI
{
	FGlobalBoundShaderState GD3D12ClearMRTBoundShaderState[8];
	TGlobalResource<FVector4VertexDeclaration> GD3D12Vector4VertexDeclaration;
}
using namespace D3D12RHI;

#define DECLARE_ISBOUNDSHADER(ShaderType) inline void ValidateBoundShader(FD3D12StateCache& InStateCache, F##ShaderType##RHIParamRef ShaderType##RHI) \
{ \
	FD3D12##ShaderType* CachedShader; \
	InStateCache.Get##ShaderType(&CachedShader); \
	FD3D12##ShaderType* ShaderType = FD3D12DynamicRHI::ResourceCast(ShaderType##RHI); \
	ensureMsgf(CachedShader == ShaderType, TEXT("Parameters are being set for a %s which is not currently bound"), TEXT( #ShaderType )); \
}

DECLARE_ISBOUNDSHADER(VertexShader)
DECLARE_ISBOUNDSHADER(PixelShader)
DECLARE_ISBOUNDSHADER(GeometryShader)
DECLARE_ISBOUNDSHADER(HullShader)
DECLARE_ISBOUNDSHADER(DomainShader)
DECLARE_ISBOUNDSHADER(ComputeShader)

#if DO_CHECK
#define VALIDATE_BOUND_SHADER(s) ValidateBoundShader(StateCache, s)
#else
#define VALIDATE_BOUND_SHADER(s)
#endif

void FD3D12DynamicRHI::SetupRecursiveResources()
{
	extern ENGINE_API FShaderCompilingManager* GShaderCompilingManager;
	check(FPlatformProperties::RequiresCookedData() || GShaderCompilingManager);

	FRHICommandList_RecursiveHazardous RHICmdList(RHIGetDefaultContext());
	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<TOneColorVS<true> > VertexShader(ShaderMap);
	GD3D12Vector4VertexDeclaration.InitRHI();

	for (int32 NumBuffers = 1; NumBuffers <= MaxSimultaneousRenderTargets; NumBuffers++)
	{
		FOneColorPS* PixelShader = NULL;

		if (NumBuffers <= 1)
		{
			TShaderMapRef<TOneColorPixelShaderMRT<1> > MRTPixelShader(ShaderMap);
			PixelShader = *MRTPixelShader;
		}
		else if (NumBuffers == 2)
		{
			TShaderMapRef<TOneColorPixelShaderMRT<2> > MRTPixelShader(ShaderMap);
			PixelShader = *MRTPixelShader;
		}
		else if (NumBuffers == 3)
		{
			TShaderMapRef<TOneColorPixelShaderMRT<3> > MRTPixelShader(ShaderMap);
			PixelShader = *MRTPixelShader;
		}
		else if (NumBuffers == 4)
		{
			TShaderMapRef<TOneColorPixelShaderMRT<4> > MRTPixelShader(ShaderMap);
			PixelShader = *MRTPixelShader;
		}
		else if (NumBuffers == 5)
		{
			TShaderMapRef<TOneColorPixelShaderMRT<5> > MRTPixelShader(ShaderMap);
			PixelShader = *MRTPixelShader;
		}
		else if (NumBuffers == 6)
		{
			TShaderMapRef<TOneColorPixelShaderMRT<6> > MRTPixelShader(ShaderMap);
			PixelShader = *MRTPixelShader;
		}
		else if (NumBuffers == 7)
		{
			TShaderMapRef<TOneColorPixelShaderMRT<7> > MRTPixelShader(ShaderMap);
			PixelShader = *MRTPixelShader;
		}
		else if (NumBuffers == 8)
		{
			TShaderMapRef<TOneColorPixelShaderMRT<8> > MRTPixelShader(ShaderMap);
			PixelShader = *MRTPixelShader;
		}

		SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, GD3D12ClearMRTBoundShaderState[NumBuffers - 1], GD3D12Vector4VertexDeclaration.VertexDeclarationRHI, *VertexShader, PixelShader);
	}

	// MSFT: Seb: Is this needed?
	//extern ENGINE_API TGlobalResource<FScreenVertexDeclaration> GScreenVertexDeclaration;

	//TShaderMapRef<FD3D12RHIResolveVS> ResolveVertexShader(ShaderMap);
	//TShaderMapRef<FD3D12RHIResolveDepthNonMSPS> ResolvePixelShader(ShaderMap);

	//SetGlobalBoundShaderState(RHICmdList, GMaxRHIFeatureLevel, ResolveBoundShaderState, GScreenVertexDeclaration.VertexDeclarationRHI, *ResolveVertexShader, *ResolvePixelShader);
}

// Vertex state.
void FD3D12CommandContext::RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBufferRHI, uint32 Stride, uint32 Offset)
{
	FD3D12VertexBuffer* VertexBuffer = FD3D12DynamicRHI::ResourceCast(VertexBufferRHI);

	StateCache.SetStreamSource(VertexBuffer ? VertexBuffer->ResourceLocation.GetReference() : nullptr, StreamIndex, Stride, Offset);
}

// Stream-Out state.
void FD3D12DynamicRHI::RHISetStreamOutTargets(uint32 NumTargets, const FVertexBufferRHIParamRef* VertexBuffers, const uint32* Offsets)
{
	FD3D12CommandContext& CmdContext = GetRHIDevice()->GetDefaultCommandContext();
	FD3D12Resource* D3DVertexBuffers[D3D12_SO_BUFFER_SLOT_COUNT] = { 0 };
	uint32 D3DOffsets[D3D12_SO_BUFFER_SLOT_COUNT] = { 0 };

	if (VertexBuffers)
	{
		for (uint32 BufferIndex = 0; BufferIndex < NumTargets; BufferIndex++)
		{
			D3DVertexBuffers[BufferIndex] = VertexBuffers[BufferIndex] ? ((FD3D12VertexBuffer*)VertexBuffers[BufferIndex])->ResourceLocation->GetResource() : NULL;
			D3DOffsets[BufferIndex] = Offsets[BufferIndex] + (VertexBuffers[BufferIndex] ? ((FD3D12VertexBuffer*)VertexBuffers[BufferIndex])->ResourceLocation->GetOffset() : 0);
		}
	}

	CmdContext.StateCache.SetStreamOutTargets(NumTargets, D3DVertexBuffers, D3DOffsets);
}

// Rasterizer state.
void FD3D12CommandContext::RHISetRasterizerState(FRasterizerStateRHIParamRef NewStateRHI)
{
	FD3D12RasterizerState* NewState = FD3D12DynamicRHI::ResourceCast(NewStateRHI);
	StateCache.SetRasterizerState(&NewState->Desc);
}

void FD3D12CommandContext::RHISetComputeShader(FComputeShaderRHIParamRef ComputeShaderRHI)
{
	SetCurrentComputeShader(ComputeShaderRHI);
}

void FD3D12CommandContext::RHIWaitComputeFence(FComputeFenceRHIParamRef InFenceRHI)
{
	FD3D12Fence* Fence = FD3D12DynamicRHI::ResourceCast(InFenceRHI);

	if (Fence)
	{
		checkf(Fence->GetWriteEnqueued(), TEXT("ComputeFence: %s waited on before being written. This will hang the GPU."), *Fence->GetName().ToString());
		Fence->GpuWait(GetCommandListManager().GetD3DCommandQueue(), Fence->GetSignalFence());
	}
}

void FD3D12CommandContext::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	FComputeShaderRHIParamRef ComputeShaderRHI = GetCurrentComputeShader();
	FD3D12ComputeShader* ComputeShader = FD3D12DynamicRHI::ResourceCast(ComputeShaderRHI);

	StateCache.SetComputeShader(ComputeShader);

	if (IsDefaultContext())
	{
		OwningRHI.RegisterGPUWork(1);
	}

	if (ComputeShader->ResourceCounts.bGlobalUniformBufferUsed)
	{
		CommitComputeShaderConstants();
	}
	CommitComputeResourceTables(ComputeShader);
	StateCache.ApplyState(true);

	numDispatches++;
	CommandListHandle->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	DEBUG_EXECUTE_COMMAND_LIST(this);

	StateCache.SetComputeShader(nullptr);
}

void FD3D12CommandContext::RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
	FComputeShaderRHIParamRef ComputeShaderRHI = GetCurrentComputeShader();
	FD3D12ComputeShader* ComputeShader = FD3D12DynamicRHI::ResourceCast(ComputeShaderRHI);
	FD3D12VertexBuffer* ArgumentBuffer = FD3D12DynamicRHI::ResourceCast(ArgumentBufferRHI);

	if (IsDefaultContext())
	{
		OwningRHI.RegisterGPUWork(1);
	}

	StateCache.SetComputeShader(ComputeShader);

	if (ComputeShader->ResourceCounts.bGlobalUniformBufferUsed)
	{
		CommitComputeShaderConstants();
	}
	CommitComputeResourceTables(ComputeShader);
	StateCache.ApplyState(true);

	FD3D12DynamicRHI::TransitionResource(CommandListHandle, ArgumentBuffer->ResourceLocation->GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);

	numDispatches++;
	CommandListHandle->ExecuteIndirect(
		GetParentDevice()->GetDispatchIndirectCommandSignature(),
		1,
		ArgumentBuffer->ResourceLocation->GetResource()->GetResource(),
		ArgumentBuffer->ResourceLocation->GetOffset() + ArgumentOffset,
		NULL,
		0
		);
	CommandListHandle.UpdateResidency(ArgumentBuffer->ResourceLocation->GetResource());

	DEBUG_EXECUTE_COMMAND_LIST(this);

	StateCache.SetComputeShader(nullptr);
}


void FD3D12CommandContext::RHITransitionResources(EResourceTransitionAccess TransitionType, FTextureRHIParamRef* InTextures, int32 NumTextures)
{
#if !USE_D3D12RHI_RESOURCE_STATE_TRACKING
	check(TransitionType == EResourceTransitionAccess::EReadable || TransitionType == EResourceTransitionAccess::EWritable || TransitionType == EResourceTransitionAccess::ERWSubResBarrier);
	// TODO: Remove this skip.
	// Skip for now because we don't have enough info about what mip to transition yet.
	// Note: This causes visual corruption.
	if (TransitionType == EResourceTransitionAccess::ERWSubResBarrier)
	{
		return;
	}

	static IConsoleVariable* CVarShowTransitions = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowTransitions"));
	const bool bShowTransitionEvents = CVarShowTransitions->GetInt() != 0;

	SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResources, bShowTransitionEvents, TEXT("TransitionTo: %s: %i Textures"), *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)TransitionType], NumTextures);

	// Determine the direction of the transitions.
	const D3D12_RESOURCE_STATES* pBefore = nullptr;
	const D3D12_RESOURCE_STATES* pAfter = nullptr;
	D3D12_RESOURCE_STATES WritableState;
	D3D12_RESOURCE_STATES ReadableState;
	switch (TransitionType)
	{
	case EResourceTransitionAccess::EReadable:
		// Write -> Read
		pBefore = &WritableState;
		pAfter = &ReadableState;
		break;

	case EResourceTransitionAccess::EWritable:
		// Read -> Write
		pBefore = &ReadableState;
		pAfter = &WritableState;
		break;

	default:
		check(false);
		break;
	}

	// Create the resource barrier descs for each texture to transition.
	for (int32 i = 0; i < NumTextures; ++i)
	{
		if (InTextures[i])
		{
			FD3D12Resource* Resource = GetD3D12TextureFromRHITexture(InTextures[i])->GetResource();
			check(Resource->RequiresResourceStateTracking());

			SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResourcesLoop, bShowTransitionEvents, TEXT("To:%i - %s"), i, *Resource->GetName().ToString());

			WritableState = Resource->GetWritableState();
			ReadableState = Resource->GetReadableState();

			CommandListHandle.AddTransitionBarrier(Resource, *pBefore, *pAfter, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

			DUMP_TRANSITION(Resource->GetName(), TransitionType);
		}
	}
#endif // !USE_D3D12RHI_RESOURCE_STATE_TRACKING
}


void FD3D12CommandContext::RHITransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 InNumUAVs, FComputeFenceRHIParamRef WriteComputeFenceRHI)
{
	static IConsoleVariable* CVarShowTransitions = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowTransitions"));
	const bool bShowTransitionEvents = CVarShowTransitions->GetInt() != 0;

	SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResources, bShowTransitionEvents, TEXT("TransitionTo: %s: %i UAVs"), *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)TransitionType], InNumUAVs);
	const bool bUAVTransition = (TransitionType == EResourceTransitionAccess::EReadable) || (TransitionType == EResourceTransitionAccess::EWritable || TransitionType == EResourceTransitionAccess::ERWBarrier);
	const bool bUAVBarrier = (TransitionType == EResourceTransitionAccess::ERWBarrier && TransitionPipeline == EResourceTransitionPipeline::EComputeToCompute);

	if (bUAVBarrier)
	{
		// UAV barrier between Dispatch() calls to ensure all R/W accesses are complete.
		StateCache.FlushComputeShaderCache(true);
	}
	else if (bUAVTransition)
	{
#if !USE_D3D12RHI_RESOURCE_STATE_TRACKING
		// Determine the direction of the transitions.
		// Note in this method, the writeable state is always UAV, regardless of the FD3D12Resource's Writeable state.
		const D3D12_RESOURCE_STATES* pBefore = nullptr;
		const D3D12_RESOURCE_STATES* pAfter = nullptr;
		const D3D12_RESOURCE_STATES WritableComputeState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		D3D12_RESOURCE_STATES WritableGraphicsState;
		D3D12_RESOURCE_STATES ReadableState;
		switch (TransitionType)
		{
		case EResourceTransitionAccess::EReadable:
			// Write -> Read
			pBefore = &WritableComputeState;
			pAfter = &ReadableState;
			break;

		case EResourceTransitionAccess::EWritable:
			// Read -> Write
			pBefore = &ReadableState;
			pAfter = &WritableComputeState;
			break;

		case EResourceTransitionAccess::ERWBarrier:
			// Write -> Write, but switching from Grfx to Compute.
			check(TransitionPipeline == EResourceTransitionPipeline::EGfxToCompute);
			pBefore = &WritableGraphicsState;
			pAfter = &WritableComputeState;
			break;

		default:
			check(false);
			break;
		}

		// Create the resource barrier descs for each texture to transition.
		for (int32 i = 0; i < InNumUAVs; ++i)
		{
			if (InUAVs[i])
			{
				FD3D12UnorderedAccessView* UnorderedAccessView = FD3D12DynamicRHI::ResourceCast(InUAVs[i]);
				FD3D12Resource* Resource = UnorderedAccessView->GetResource();
				check(Resource->RequiresResourceStateTracking());

				SCOPED_RHI_CONDITIONAL_DRAW_EVENTF(*this, RHITransitionResourcesLoop, bShowTransitionEvents, TEXT("To:%i - %s"), i, *Resource->GetName().ToString());

				// The writable compute state is always UAV.
				WritableGraphicsState = Resource->GetWritableState();
				ReadableState = Resource->GetReadableState();

				// Some ERWBarriers might have the same before and after states.
				if (*pBefore != *pAfter)
				{
					CommandListHandle.AddTransitionBarrier(Resource, *pBefore, *pAfter, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

					DUMP_TRANSITION(Resource->GetName(), TransitionType);
				}
			}
		}
#endif // !USE_D3D12RHI_RESOURCE_STATE_TRACKING
	}

	if (WriteComputeFenceRHI)
	{
		FD3D12Fence* Fence = FD3D12DynamicRHI::ResourceCast(WriteComputeFenceRHI);
		Fence->WriteFence();

		if (!bIsAsyncComputeContext)
		{
			Fence->Signal(GetParentDevice()->GetCommandListManager().GetD3DCommandQueue());
		}
		else
		{
			PendingFence = Fence;
		}
	}
}

void FD3D12CommandContext::RHISetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
{
	// These are the maximum viewport extents for D3D12. Exceeding them leads to badness.
	check(MinX <= (uint32)D3D12_VIEWPORT_BOUNDS_MAX);
	check(MinY <= (uint32)D3D12_VIEWPORT_BOUNDS_MAX);
	check(MaxX <= (uint32)D3D12_VIEWPORT_BOUNDS_MAX);
	check(MaxY <= (uint32)D3D12_VIEWPORT_BOUNDS_MAX);

	D3D12_VIEWPORT Viewport = { (float)MinX, (float)MinY, (float)(MaxX - MinX), (float)(MaxY - MinY), MinZ, MaxZ };
	//avoid setting a 0 extent viewport, which the debug runtime doesn't like
	if (Viewport.Width > 0 && Viewport.Height > 0)
	{
		StateCache.SetViewport(Viewport);
		SetScissorRectIfRequiredWhenSettingViewport(MinX, MinY, MaxX, MaxY);
	}
}

void FD3D12CommandContext::RHISetStereoViewport(uint32 LeftMinX, uint32 RightMinX, uint32 MinY, float MinZ, uint32 LeftMaxX, uint32 RightMaxX, uint32 MaxY, float MaxZ)
{
	UE_LOG(LogD3D12RHI, Fatal, TEXT("D3D12 RHI does not support set stereo viewport!"));
}

void FD3D12CommandContext::RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
{
	if (bEnable)
	{
		D3D12_RECT ScissorRect;
		ScissorRect.left = MinX;
		ScissorRect.right = MaxX;
		ScissorRect.top = MinY;
		ScissorRect.bottom = MaxY;
		StateCache.SetScissorRects(1, &ScissorRect);
	}
	else
	{
		D3D12_RECT ScissorRect;
		ScissorRect.left = 0;
		ScissorRect.right = GetMax2DTextureDimension();
		ScissorRect.top = 0;
		ScissorRect.bottom = GetMax2DTextureDimension();
		StateCache.SetScissorRects(1, &ScissorRect);
	}
}

/**
* Set bound shader state. This will set the vertex decl/shader, and pixel shader
* @param BoundShaderState - state resource
*/
void FD3D12CommandContext::RHISetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderStateRHI)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12SetShaderUniformBuffer);SCOPE_CYCLE_COUNTER(STAT_D3D12SetBoundShaderState);
	FD3D12BoundShaderState* BoundShaderState = FD3D12DynamicRHI::ResourceCast(BoundShaderStateRHI);

	StateCache.SetBoundShaderState(BoundShaderState);

	if (BoundShaderState->GetHullShader() && BoundShaderState->GetDomainShader())
	{
		bUsingTessellation = true;
	}
	else
	{
		bUsingTessellation = false;
	}

	// @TODO : really should only discard the constants if the shader state has actually changed.
	bDiscardSharedConstants = true;

	CurrentBoundShaderState = BoundShaderState;

	// Prevent transient bound shader states from being recreated for each use by keeping a history of the most recently used bound shader states.
	// The history keeps them alive, and the bound shader state cache allows them to be reused if needed.
	// MSFT: JSTANARD:  Is this still relevant?
	OwningRHI.AddBoundShaderState(BoundShaderState);

	// Shader changed so all resource tables are dirty
	DirtyUniformBuffers[SF_Vertex] = 0xffff;
	DirtyUniformBuffers[SF_Pixel] = 0xffff;
	DirtyUniformBuffers[SF_Hull] = 0xffff;
	DirtyUniformBuffers[SF_Domain] = 0xffff;
	DirtyUniformBuffers[SF_Geometry] = 0xffff;

	// To avoid putting bad samplers into the descriptor heap
	// Clear all sampler & SRV bindings here
	StateCache.ClearSamplers();
	StateCache.ClearSRVs();
}

void FD3D12CommandContext::RHISetShaderTexture(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	uint32 Start = FPlatformTime::Cycles();

	VALIDATE_BOUND_SHADER(VertexShaderRHI);

	FD3D12TextureBase* NewTexture = GetD3D12TextureFromRHITexture(NewTextureRHI);
	FD3D12ShaderResourceView* ShaderResourceView = NewTexture ? NewTexture->GetShaderResourceView() : NULL;
	FD3D12ResourceLocation* ResourceLocation = NewTexture ? NewTexture->ResourceLocation : nullptr;

	if ((NewTexture == NULL) || (NewTexture->GetRenderTargetView(0, 0) != NULL) || (NewTexture->HasDepthStencilView()))
		SetShaderResourceView<SF_Vertex>(ResourceLocation, ShaderResourceView, TextureIndex, FD3D12StateCache::SRV_Dynamic);
	else
		SetShaderResourceView<SF_Vertex>(ResourceLocation, ShaderResourceView, TextureIndex, FD3D12StateCache::SRV_Static);

	OwningRHI.IncrementSetShaderTextureCycles(FPlatformTime::Cycles() - Start);
	OwningRHI.IncrementSetShaderTextureCalls();
}

void FD3D12CommandContext::RHISetShaderTexture(FHullShaderRHIParamRef HullShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	uint32 Start = FPlatformTime::Cycles();
	VALIDATE_BOUND_SHADER(HullShaderRHI);

	FD3D12TextureBase* NewTexture = GetD3D12TextureFromRHITexture(NewTextureRHI);
	FD3D12ShaderResourceView* ShaderResourceView = NewTexture ? NewTexture->GetShaderResourceView() : NULL;
	FD3D12ResourceLocation* ResourceLocation = NewTexture ? NewTexture->ResourceLocation : nullptr;

	if ((NewTexture == NULL) || (NewTexture->GetRenderTargetView(0, 0) != NULL) || (NewTexture->HasDepthStencilView()))
		SetShaderResourceView<SF_Hull>(ResourceLocation, ShaderResourceView, TextureIndex, FD3D12StateCache::SRV_Dynamic);
	else
		SetShaderResourceView<SF_Hull>(ResourceLocation, ShaderResourceView, TextureIndex, FD3D12StateCache::SRV_Static);

	OwningRHI.IncrementSetShaderTextureCycles(FPlatformTime::Cycles() - Start);
	OwningRHI.IncrementSetShaderTextureCalls();
}

void FD3D12CommandContext::RHISetShaderTexture(FDomainShaderRHIParamRef DomainShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	uint32 Start = FPlatformTime::Cycles();

	VALIDATE_BOUND_SHADER(DomainShaderRHI);

	FD3D12TextureBase* NewTexture = GetD3D12TextureFromRHITexture(NewTextureRHI);
	FD3D12ShaderResourceView* ShaderResourceView = NewTexture ? NewTexture->GetShaderResourceView() : NULL;
	FD3D12ResourceLocation* ResourceLocation = NewTexture ? NewTexture->ResourceLocation : nullptr;

	if ((NewTexture == NULL) || (NewTexture->GetRenderTargetView(0, 0) != NULL) || (NewTexture->HasDepthStencilView()))
		SetShaderResourceView<SF_Domain>(ResourceLocation, ShaderResourceView, TextureIndex, FD3D12StateCache::SRV_Dynamic);
	else
		SetShaderResourceView<SF_Domain>(ResourceLocation, ShaderResourceView, TextureIndex, FD3D12StateCache::SRV_Static);

	OwningRHI.IncrementSetShaderTextureCycles(FPlatformTime::Cycles() - Start);
	OwningRHI.IncrementSetShaderTextureCalls();
}

void FD3D12CommandContext::RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	uint32 Start = FPlatformTime::Cycles();

	VALIDATE_BOUND_SHADER(GeometryShaderRHI);

	FD3D12TextureBase* NewTexture = GetD3D12TextureFromRHITexture(NewTextureRHI);
	FD3D12ShaderResourceView* ShaderResourceView = NewTexture ? NewTexture->GetShaderResourceView() : NULL;
	FD3D12ResourceLocation* ResourceLocation = NewTexture ? NewTexture->ResourceLocation : nullptr;

	if ((NewTexture == NULL) || (NewTexture->GetRenderTargetView(0, 0) != NULL) || (NewTexture->HasDepthStencilView()))
		SetShaderResourceView<SF_Geometry>(ResourceLocation, ShaderResourceView, TextureIndex, FD3D12StateCache::SRV_Dynamic);
	else
		SetShaderResourceView<SF_Geometry>(ResourceLocation, ShaderResourceView, TextureIndex, FD3D12StateCache::SRV_Static);

	OwningRHI.IncrementSetShaderTextureCycles(FPlatformTime::Cycles() - Start);
	OwningRHI.IncrementSetShaderTextureCalls();
}

void FD3D12CommandContext::RHISetShaderTexture(FPixelShaderRHIParamRef PixelShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	uint32 Start = FPlatformTime::Cycles();

	VALIDATE_BOUND_SHADER(PixelShaderRHI);

	FD3D12TextureBase* NewTexture = GetD3D12TextureFromRHITexture(NewTextureRHI);
	FD3D12ShaderResourceView* ShaderResourceView = NewTexture ? NewTexture->GetShaderResourceView() : NULL;
	FD3D12ResourceLocation* ResourceLocation = NewTexture ? NewTexture->ResourceLocation : nullptr;

	if ((NewTexture == NULL) ||  (NewTexture->GetRenderTargetView(0, 0) != NULL) || (NewTexture->HasDepthStencilView()))
		SetShaderResourceView<SF_Pixel>(ResourceLocation, ShaderResourceView, TextureIndex, FD3D12StateCache::SRV_Dynamic);
	else
		SetShaderResourceView<SF_Pixel>(ResourceLocation, ShaderResourceView, TextureIndex, FD3D12StateCache::SRV_Static);

	OwningRHI.IncrementSetShaderTextureCycles(FPlatformTime::Cycles() - Start);
	OwningRHI.IncrementSetShaderTextureCalls();
}

void FD3D12CommandContext::RHISetShaderTexture(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 TextureIndex, FTextureRHIParamRef NewTextureRHI)
{
	uint32 Start = FPlatformTime::Cycles();

	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);

	FD3D12TextureBase* NewTexture = GetD3D12TextureFromRHITexture(NewTextureRHI);
	FD3D12ShaderResourceView* ShaderResourceView = NewTexture ? NewTexture->GetShaderResourceView() : NULL;
	FD3D12ResourceLocation* ResourceLocation = NewTexture ? NewTexture->ResourceLocation : nullptr;

	if ( ( NewTexture == NULL) || ( NewTexture->GetRenderTargetView( 0, 0 ) !=NULL) || ( NewTexture->HasDepthStencilView()) )
	{
		SetShaderResourceView<SF_Compute>(ResourceLocation, ShaderResourceView, TextureIndex, FD3D12StateCache::SRV_Dynamic);
	}
	else
	{
		SetShaderResourceView<SF_Compute>(ResourceLocation, ShaderResourceView, TextureIndex, FD3D12StateCache::SRV_Static);
	}

	OwningRHI.IncrementSetShaderTextureCycles(FPlatformTime::Cycles() - Start);
	OwningRHI.IncrementSetShaderTextureCalls();
}

void FD3D12CommandContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAVRHI)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);

	FD3D12UnorderedAccessView* UAV = FD3D12DynamicRHI::ResourceCast(UAVRHI);

	if (UAV)
	{
		ConditionalClearShaderResource(UAV->GetResourceLocation());
	}

	uint32 InitialCount = -1;

	// Actually set the UAV
	StateCache.SetUAVs(SF_Compute, UAVIndex, 1, &UAV, &InitialCount);
}

void FD3D12CommandContext::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAVRHI, uint32 InitialCount)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);

	FD3D12UnorderedAccessView* UAV = FD3D12DynamicRHI::ResourceCast(UAVRHI);

	if (UAV)
	{
		ConditionalClearShaderResource(UAV->GetResourceLocation());
	}

	StateCache.SetUAVs(SF_Compute, UAVIndex, 1, &UAV, &InitialCount);
}

void FD3D12CommandContext::RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(PixelShaderRHI);

	FD3D12ShaderResourceView* SRV = FD3D12DynamicRHI::ResourceCast(SRVRHI);

	FD3D12ResourceLocation* ResourceLocation = nullptr;
	FD3D12ShaderResourceView* D3D12SRV = nullptr;

	if (SRV)
	{
		ResourceLocation = SRV->GetResourceLocation();
		D3D12SRV = SRV;
	}

	SetShaderResourceView<SF_Pixel>(ResourceLocation, D3D12SRV, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(VertexShaderRHI);

	FD3D12ShaderResourceView* SRV = FD3D12DynamicRHI::ResourceCast(SRVRHI);

	FD3D12ResourceLocation* ResourceLocation = nullptr;
	FD3D12ShaderResourceView* D3D12SRV = nullptr;

	if (SRV)
	{
		ResourceLocation = SRV->GetResourceLocation();
		D3D12SRV = SRV;
	}

	SetShaderResourceView<SF_Vertex>(ResourceLocation, D3D12SRV, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);

	FD3D12ShaderResourceView* SRV = FD3D12DynamicRHI::ResourceCast(SRVRHI);

	FD3D12ResourceLocation* ResourceLocation = nullptr;
	FD3D12ShaderResourceView* D3D12SRV = nullptr;

	if (SRV)
	{
		ResourceLocation = SRV->GetResourceLocation();
		D3D12SRV = SRV;
	}

	SetShaderResourceView<SF_Compute>(ResourceLocation, D3D12SRV, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(HullShaderRHI);

	FD3D12ShaderResourceView* SRV = FD3D12DynamicRHI::ResourceCast(SRVRHI);

	FD3D12ResourceLocation* ResourceLocation = nullptr;
	FD3D12ShaderResourceView* D3D12SRV = nullptr;

	if (SRV)
	{
		ResourceLocation = SRV->GetResourceLocation();
		D3D12SRV = SRV;
	}

	SetShaderResourceView<SF_Hull>(ResourceLocation, D3D12SRV, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(DomainShaderRHI);

	FD3D12ShaderResourceView* SRV = FD3D12DynamicRHI::ResourceCast(SRVRHI);

	FD3D12ResourceLocation* ResourceLocation = nullptr;
	FD3D12ShaderResourceView* D3D12SRV = nullptr;

	if (SRV)
	{
		ResourceLocation = SRV->GetResourceLocation();
		D3D12SRV = SRV;
	}

	SetShaderResourceView<SF_Domain>(ResourceLocation, D3D12SRV, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 TextureIndex, FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(GeometryShaderRHI);

	FD3D12ShaderResourceView* SRV = FD3D12DynamicRHI::ResourceCast(SRVRHI);

	FD3D12ResourceLocation* ResourceLocation = nullptr;
	FD3D12ShaderResourceView* D3D12SRV = nullptr;

	if (SRV)
	{
		ResourceLocation = SRV->GetResourceLocation();
		D3D12SRV = SRV;
	}

	SetShaderResourceView<SF_Geometry>(ResourceLocation, D3D12SRV, TextureIndex);
}

void FD3D12CommandContext::RHISetShaderSampler(FVertexShaderRHIParamRef VertexShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(VertexShaderRHI);

	FD3D12VertexShader* VertexShader = FD3D12DynamicRHI::ResourceCast(VertexShaderRHI);
	FD3D12SamplerState* NewState = FD3D12DynamicRHI::ResourceCast(NewStateRHI);

	StateCache.SetSamplerState<SF_Vertex>(NewState, SamplerIndex);
}

void FD3D12CommandContext::RHISetShaderSampler(FHullShaderRHIParamRef HullShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(HullShaderRHI);

	FD3D12HullShader* HullShader = FD3D12DynamicRHI::ResourceCast(HullShaderRHI);
	FD3D12SamplerState* NewState = FD3D12DynamicRHI::ResourceCast(NewStateRHI);

	StateCache.SetSamplerState<SF_Hull>(NewState, SamplerIndex);
}

void FD3D12CommandContext::RHISetShaderSampler(FDomainShaderRHIParamRef DomainShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(DomainShaderRHI);

	FD3D12DomainShader* DomainShader = FD3D12DynamicRHI::ResourceCast(DomainShaderRHI);
	FD3D12SamplerState* NewState = FD3D12DynamicRHI::ResourceCast(NewStateRHI);

	StateCache.SetSamplerState<SF_Domain>(NewState, SamplerIndex);
}

void FD3D12CommandContext::RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(GeometryShaderRHI);

	FD3D12GeometryShader* GeometryShader = FD3D12DynamicRHI::ResourceCast(GeometryShaderRHI);
	FD3D12SamplerState* NewState = FD3D12DynamicRHI::ResourceCast(NewStateRHI);

	StateCache.SetSamplerState<SF_Geometry>(NewState, SamplerIndex);
}

void FD3D12CommandContext::RHISetShaderSampler(FPixelShaderRHIParamRef PixelShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(PixelShaderRHI);

	FD3D12PixelShader* PixelShader = FD3D12DynamicRHI::ResourceCast(PixelShaderRHI);
	FD3D12SamplerState* NewState = FD3D12DynamicRHI::ResourceCast(NewStateRHI);

	StateCache.SetSamplerState<SF_Pixel>(NewState, SamplerIndex);
}

void FD3D12CommandContext::RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 SamplerIndex, FSamplerStateRHIParamRef NewStateRHI)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);
	FD3D12ComputeShader* ComputeShader = FD3D12DynamicRHI::ResourceCast(ComputeShaderRHI);
	FD3D12SamplerState* NewState = FD3D12DynamicRHI::ResourceCast(NewStateRHI);

	StateCache.SetSamplerState<SF_Compute>(NewState, SamplerIndex);
}

void FD3D12CommandContext::RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12SetShaderUniformBuffer);
	VALIDATE_BOUND_SHADER(VertexShader);
	FD3D12UniformBuffer* Buffer = FD3D12DynamicRHI::ResourceCast(BufferRHI);

	StateCache.SetConstantBuffer<SF_Vertex>(BufferIndex, nullptr, Buffer);

	BoundUniformBuffers[SF_Vertex][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Vertex] |= (1 << BufferIndex);
}

void FD3D12CommandContext::RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12SetShaderUniformBuffer);
	VALIDATE_BOUND_SHADER(HullShader);
	FD3D12UniformBuffer* Buffer = FD3D12DynamicRHI::ResourceCast(BufferRHI);

	StateCache.SetConstantBuffer<SF_Hull>(BufferIndex, nullptr, Buffer);

	BoundUniformBuffers[SF_Hull][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Hull] |= (1 << BufferIndex);
}

void FD3D12CommandContext::RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12SetShaderUniformBuffer);
	VALIDATE_BOUND_SHADER(DomainShader);
	FD3D12UniformBuffer* Buffer = FD3D12DynamicRHI::ResourceCast(BufferRHI);
	
	StateCache.SetConstantBuffer<SF_Domain>(BufferIndex, nullptr, Buffer);

	BoundUniformBuffers[SF_Domain][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Domain] |= (1 << BufferIndex);
}

void FD3D12CommandContext::RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12SetShaderUniformBuffer);
	VALIDATE_BOUND_SHADER(GeometryShader);
	FD3D12UniformBuffer* Buffer = FD3D12DynamicRHI::ResourceCast(BufferRHI);

	StateCache.SetConstantBuffer<SF_Geometry>(BufferIndex, nullptr, Buffer);

	BoundUniformBuffers[SF_Geometry][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Geometry] |= (1 << BufferIndex);
}

void FD3D12CommandContext::RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12SetShaderUniformBuffer);
	VALIDATE_BOUND_SHADER(PixelShader);
	FD3D12UniformBuffer* Buffer = FD3D12DynamicRHI::ResourceCast(BufferRHI);

	StateCache.SetConstantBuffer<SF_Pixel>(BufferIndex, nullptr, Buffer);

	BoundUniformBuffers[SF_Pixel][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Pixel] |= (1 << BufferIndex);
}

void FD3D12CommandContext::RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShader, uint32 BufferIndex, FUniformBufferRHIParamRef BufferRHI)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12SetShaderUniformBuffer);
	//VALIDATE_BOUND_SHADER(ComputeShader);
	FD3D12UniformBuffer* Buffer = FD3D12DynamicRHI::ResourceCast(BufferRHI);

	StateCache.SetConstantBuffer<SF_Compute>(BufferIndex, nullptr, Buffer);

	BoundUniformBuffers[SF_Compute][BufferIndex] = BufferRHI;
	DirtyUniformBuffers[SF_Compute] |= (1 << BufferIndex);
}

void FD3D12CommandContext::RHISetShaderParameter(FHullShaderRHIParamRef HullShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	VALIDATE_BOUND_SHADER(HullShaderRHI);
	checkSlow(HSConstantBuffers[BufferIndex]);
	HSConstantBuffers[BufferIndex]->UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
}

void FD3D12CommandContext::RHISetShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	VALIDATE_BOUND_SHADER(DomainShaderRHI);
	checkSlow(DSConstantBuffers[BufferIndex]);
	DSConstantBuffers[BufferIndex]->UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
}

void FD3D12CommandContext::RHISetShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	VALIDATE_BOUND_SHADER(VertexShaderRHI);
	checkSlow(VSConstantBuffers[BufferIndex]);
	VSConstantBuffers[BufferIndex]->UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
}

void FD3D12CommandContext::RHISetShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	VALIDATE_BOUND_SHADER(PixelShaderRHI);
	checkSlow(PSConstantBuffers[BufferIndex]);
	PSConstantBuffers[BufferIndex]->UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
}

void FD3D12CommandContext::RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	VALIDATE_BOUND_SHADER(GeometryShaderRHI);
	checkSlow(GSConstantBuffers[BufferIndex]);
	GSConstantBuffers[BufferIndex]->UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
}

void FD3D12CommandContext::RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShaderRHI, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
{
	//VALIDATE_BOUND_SHADER(ComputeShaderRHI);
	checkSlow(CSConstantBuffers[BufferIndex]);
	CSConstantBuffers[BufferIndex]->UpdateConstant((const uint8*)NewValue, BaseIndex, NumBytes);
}

void FD3D12CommandContext::ValidateExclusiveDepthStencilAccess(FExclusiveDepthStencil RequestedAccess) const
{
	const bool bSrcDepthWrite = RequestedAccess.IsDepthWrite();
	const bool bSrcStencilWrite = RequestedAccess.IsStencilWrite();

	if (bSrcDepthWrite || bSrcStencilWrite)
	{
		// New Rule: You have to call SetRenderTarget[s]() before
		ensure(CurrentDepthTexture);

		const bool bDstDepthWrite = CurrentDSVAccessType.IsDepthWrite();
		const bool bDstStencilWrite = CurrentDSVAccessType.IsStencilWrite();

		// requested access is not possible, fix SetRenderTarget EExclusiveDepthStencil or request a different one
		check(!bSrcDepthWrite || bDstDepthWrite);
		check(!bSrcStencilWrite || bDstStencilWrite);
	}
}

void FD3D12CommandContext::RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewStateRHI, uint32 StencilRef)
{
	FD3D12DepthStencilState* NewState = FD3D12DynamicRHI::ResourceCast(NewStateRHI);

	ValidateExclusiveDepthStencilAccess(NewState->AccessType);

	StateCache.SetDepthStencilState(&NewState->Desc, StencilRef);
}

void FD3D12CommandContext::RHISetBlendState(FBlendStateRHIParamRef NewStateRHI, const FLinearColor& BlendFactor)
{
	FD3D12BlendState* NewState = FD3D12DynamicRHI::ResourceCast(NewStateRHI);
	StateCache.SetBlendState(&NewState->Desc, (const float*)&BlendFactor, 0xffffffff);
}

void FD3D12CommandContext::CommitRenderTargetsAndUAVs()
{
	FD3D12RenderTargetView* RTArray[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];

	for (uint32 RenderTargetIndex = 0;RenderTargetIndex < NumSimultaneousRenderTargets;++RenderTargetIndex)
	{
		RTArray[RenderTargetIndex] = CurrentRenderTargets[RenderTargetIndex];
	}
	FD3D12UnorderedAccessView* UAVArray[D3D12_PS_CS_UAV_REGISTER_COUNT];

	uint32 UAVInitialCountArray[D3D12_PS_CS_UAV_REGISTER_COUNT];
	for (uint32 UAVIndex = 0;UAVIndex < NumUAVs;++UAVIndex)
	{
		UAVArray[UAVIndex] = CurrentUAVs[UAVIndex];
		// Using the value that indicates to keep the current UAV counter
		UAVInitialCountArray[UAVIndex] = -1;
	}

	StateCache.SetRenderTargets(NumSimultaneousRenderTargets,
		RTArray,
		CurrentDepthStencilTarget
		);

	if (NumUAVs > 0)
	{
		StateCache.SetUAVs(SF_Pixel, NumSimultaneousRenderTargets,
			NumUAVs,
			UAVArray,
			UAVInitialCountArray
			);
	}
}

struct FRTVDesc
{
	uint32 Width;
	uint32 Height;
	DXGI_SAMPLE_DESC SampleDesc;
};

// Return an FRTVDesc structure whose
// Width and height dimensions are adjusted for the RTV's miplevel.
FRTVDesc GetRenderTargetViewDesc(FD3D12RenderTargetView* RenderTargetView)
{
	const D3D12_RENDER_TARGET_VIEW_DESC &TargetDesc = RenderTargetView->GetDesc();

	FD3D12Resource* BaseResource = RenderTargetView->GetResource();
	uint32 MipIndex = 0;
	FRTVDesc ret;
	memset(&ret, 0, sizeof(ret));

	switch (TargetDesc.ViewDimension)
	{
	case D3D12_RTV_DIMENSION_TEXTURE2D:
	case D3D12_RTV_DIMENSION_TEXTURE2DMS:
	case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
	case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
	{
		D3D12_RESOURCE_DESC const& Desc = BaseResource->GetDesc();
		ret.Width = (uint32)Desc.Width;
		ret.Height = Desc.Height;
		ret.SampleDesc = Desc.SampleDesc;
		if (TargetDesc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2D || TargetDesc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DARRAY)
		{
			// All the non-multisampled texture types have their mip-slice in the same position.
			MipIndex = TargetDesc.Texture2D.MipSlice;
		}
		break;
	}
	case D3D12_RTV_DIMENSION_TEXTURE3D:
	{
		D3D12_RESOURCE_DESC const& Desc = BaseResource->GetDesc();
		ret.Width = (uint32)Desc.Width;
		ret.Height = Desc.Height;
		ret.SampleDesc.Count = 1;
		ret.SampleDesc.Quality = 0;
		MipIndex = TargetDesc.Texture3D.MipSlice;
		break;
	}
	default:
	{
		// not expecting 1D targets.
		checkNoEntry();
	}
	}
	ret.Width >>= MipIndex;
	ret.Height >>= MipIndex;
	return ret;
}

void FD3D12CommandContext::RHISetRenderTargets(
	uint32 NewNumSimultaneousRenderTargets,
	const FRHIRenderTargetView* NewRenderTargetsRHI,
	const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI,
	uint32 NewNumUAVs,
	const FUnorderedAccessViewRHIParamRef* UAVs
	)
{
	FD3D12TextureBase* NewDepthStencilTarget = GetD3D12TextureFromRHITexture(NewDepthStencilTargetRHI ? NewDepthStencilTargetRHI->Texture : nullptr);

#if CHECK_SRV_TRANSITIONS
	// if the depth buffer is writable then it counts as unresolved.
	if (NewDepthStencilTargetRHI && NewDepthStencilTargetRHI->GetDepthStencilAccess() == FExclusiveDepthStencil::DepthWrite_StencilWrite && NewDepthStencilTarget)
	{
		UnresolvedTargets.Add(NewDepthStencilTarget->GetResource(), FUnresolvedRTInfo(NewDepthStencilTargetRHI->Texture->GetName(), 0, 1, -1, 1));
	}
#endif

	check(NewNumSimultaneousRenderTargets + NewNumUAVs <= MaxSimultaneousRenderTargets);

	bool bTargetChanged = false;

	// Set the appropriate depth stencil view depending on whether depth writes are enabled or not
	FD3D12DepthStencilView* DepthStencilView = NULL;
	if (NewDepthStencilTarget)
	{
		CurrentDSVAccessType = NewDepthStencilTargetRHI->GetDepthStencilAccess();
		DepthStencilView = NewDepthStencilTarget->GetDepthStencilView(CurrentDSVAccessType);

		// Unbind any shader views of the depth stencil target that are bound.
		ConditionalClearShaderResource(NewDepthStencilTarget->ResourceLocation);
	}

	// Check if the depth stencil target is different from the old state.
	if (CurrentDepthStencilTarget != DepthStencilView)
	{
		CurrentDepthTexture = NewDepthStencilTarget;
		CurrentDepthStencilTarget = DepthStencilView;
		bTargetChanged = true;
	}

	// Gather the render target views for the new render targets.
	FD3D12RenderTargetView* NewRenderTargetViews[MaxSimultaneousRenderTargets];
	for (uint32 RenderTargetIndex = 0;RenderTargetIndex < MaxSimultaneousRenderTargets;++RenderTargetIndex)
	{
		FD3D12RenderTargetView* RenderTargetView = NULL;
		if (RenderTargetIndex < NewNumSimultaneousRenderTargets && NewRenderTargetsRHI[RenderTargetIndex].Texture != nullptr)
		{
			int32 RTMipIndex = NewRenderTargetsRHI[RenderTargetIndex].MipIndex;
			int32 RTSliceIndex = NewRenderTargetsRHI[RenderTargetIndex].ArraySliceIndex;
			FD3D12TextureBase* NewRenderTarget = GetD3D12TextureFromRHITexture(NewRenderTargetsRHI[RenderTargetIndex].Texture);
			RenderTargetView = NewRenderTarget->GetRenderTargetView(RTMipIndex, RTSliceIndex);

			ensureMsgf(RenderTargetView, TEXT("Texture being set as render target has no RTV"));
#if CHECK_SRV_TRANSITIONS			
			if (RenderTargetView)
			{
				// remember this target as having been bound for write.
				ID3D12Resource* RTVResource;
				RenderTargetView->GetResource(&RTVResource);
				UnresolvedTargets.Add(RTVResource, FUnresolvedRTInfo(NewRenderTargetsRHI[RenderTargetIndex].Texture->GetName(), RTMipIndex, 1, RTSliceIndex, 1));
				RTVResource->Release();
			}
#endif

			// Unbind any shader views of the render target that are bound.
			ConditionalClearShaderResource(NewRenderTarget->ResourceLocation);
#if UE_BUILD_DEBUG	
			// A check to allow you to pinpoint what is using mismatching targets
			// We filter our d3ddebug spew that checks for this as the d3d runtime's check is wrong.
			// For filter code, see D3D12Device.cpp look for "OMSETRENDERTARGETS_INVALIDVIEW"
			if (RenderTargetView && DepthStencilView)
			{
				FRTVDesc RTTDesc = GetRenderTargetViewDesc(RenderTargetView);

				FD3D12Resource* DepthTargetTexture = DepthStencilView->GetResource();

				D3D12_RESOURCE_DESC DTTDesc = DepthTargetTexture->GetDesc();

				// enforce color target is <= depth and MSAA settings match
				if (RTTDesc.Width > DTTDesc.Width || RTTDesc.Height > DTTDesc.Height ||
					RTTDesc.SampleDesc.Count != DTTDesc.SampleDesc.Count ||
					RTTDesc.SampleDesc.Quality != DTTDesc.SampleDesc.Quality)
				{
					UE_LOG(LogD3D12RHI, Fatal, TEXT("RTV(%i,%i c=%i,q=%i) and DSV(%i,%i c=%i,q=%i) have mismatching dimensions and/or MSAA levels!"),
						RTTDesc.Width, RTTDesc.Height, RTTDesc.SampleDesc.Count, RTTDesc.SampleDesc.Quality,
						DTTDesc.Width, DTTDesc.Height, DTTDesc.SampleDesc.Count, DTTDesc.SampleDesc.Quality);
				}
			}
#endif
		}

		NewRenderTargetViews[RenderTargetIndex] = RenderTargetView;

		// Check if the render target is different from the old state.
		if (CurrentRenderTargets[RenderTargetIndex] != RenderTargetView)
		{
			CurrentRenderTargets[RenderTargetIndex] = RenderTargetView;
			bTargetChanged = true;
		}
	}
	if (NumSimultaneousRenderTargets != NewNumSimultaneousRenderTargets)
	{
		NumSimultaneousRenderTargets = NewNumSimultaneousRenderTargets;
		bTargetChanged = true;
	}

	// Gather the new UAVs.
	for (uint32 UAVIndex = 0;UAVIndex < MaxSimultaneousUAVs;++UAVIndex)
	{
		FD3D12UnorderedAccessView* RHIUAV = NULL;
		if (UAVIndex < NewNumUAVs && UAVs[UAVIndex] != NULL)
		{
			RHIUAV = (FD3D12UnorderedAccessView*)UAVs[UAVIndex];
			FD3D12DynamicRHI::TransitionResource(CommandListHandle, RHIUAV, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			// Unbind any shader views of the UAV's resource.
			ConditionalClearShaderResource(RHIUAV->GetResourceLocation());
		}

		if (CurrentUAVs[UAVIndex] != RHIUAV)
		{
			CurrentUAVs[UAVIndex] = RHIUAV;
			bTargetChanged = true;
		}
	}
	if (NumUAVs != NewNumUAVs)
	{
		NumUAVs = NewNumUAVs;
		bTargetChanged = true;
	}

	// Only make the D3D call to change render targets if something actually changed.
	if (bTargetChanged)
	{
		CommitRenderTargetsAndUAVs();
	}

	// Set the viewport to the full size of render target 0.
	if (NewRenderTargetViews[0])
	{
		// check target 0 is valid
		check(0 < NewNumSimultaneousRenderTargets && NewRenderTargetsRHI[0].Texture != nullptr);
		FRTVDesc RTTDesc = GetRenderTargetViewDesc(NewRenderTargetViews[0]);
		RHISetViewport(0, 0, 0.0f, RTTDesc.Width, RTTDesc.Height, 1.0f);
	}
	else if (DepthStencilView)
	{
		FD3D12Resource* DepthTargetTexture = DepthStencilView->GetResource();
		D3D12_RESOURCE_DESC const& DTTDesc = DepthTargetTexture->GetDesc();
		RHISetViewport(0, 0, 0.0f, (uint32)DTTDesc.Width, DTTDesc.Height, 1.0f);
	}
}

void FD3D12DynamicRHI::RHIDiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask)
{
	// Could support in DX12 via ID3D12CommandList::DiscardResource function.
}

void FD3D12CommandContext::RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	this->RHISetRenderTargets(RenderTargetsInfo.NumColorRenderTargets,
		RenderTargetsInfo.ColorRenderTarget,
		&RenderTargetsInfo.DepthStencilRenderTarget,
		0,
		nullptr);
	if (RenderTargetsInfo.bClearColor || RenderTargetsInfo.bClearStencil || RenderTargetsInfo.bClearDepth)
	{
		FLinearColor ClearColors[MaxSimultaneousRenderTargets];
		float DepthClear = 0.0;
		uint32 StencilClear = 0;

		if (RenderTargetsInfo.bClearColor)
		{
			for (int32 i = 0; i < RenderTargetsInfo.NumColorRenderTargets; ++i)
			{
				const FClearValueBinding& ClearValue = RenderTargetsInfo.ColorRenderTarget[i].Texture->GetClearBinding();
				checkf(ClearValue.ColorBinding == EClearBinding::EColorBound, TEXT("Texture: %s does not have a color bound for fast clears"), *RenderTargetsInfo.ColorRenderTarget[i].Texture->GetName().GetPlainNameString());
				ClearColors[i] = ClearValue.GetClearColor();
			}
		}
		if (RenderTargetsInfo.bClearDepth || RenderTargetsInfo.bClearStencil)
		{
			const FClearValueBinding& ClearValue = RenderTargetsInfo.DepthStencilRenderTarget.Texture->GetClearBinding();
			checkf(ClearValue.ColorBinding == EClearBinding::EDepthStencilBound, TEXT("Texture: %s does not have a DS value bound for fast clears"), *RenderTargetsInfo.DepthStencilRenderTarget.Texture->GetName().GetPlainNameString());
			ClearValue.GetDepthStencil(DepthClear, StencilClear);
		}

		this->RHIClearMRTImpl(RenderTargetsInfo.bClearColor, RenderTargetsInfo.NumColorRenderTargets, ClearColors, RenderTargetsInfo.bClearDepth, DepthClear, RenderTargetsInfo.bClearStencil, StencilClear, FIntRect());
	}
}

// Occlusion/Timer queries.
void FD3D12CommandContext::RHIBeginRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	FD3D12OcclusionQuery* Query = FD3D12DynamicRHI::ResourceCast(QueryRHI);

	if (Query->Type == RQT_Occlusion)
	{
		Query->bResultIsCached = false;
		Query->HeapIndex = GetParentDevice()->GetQueryHeap()->BeginQuery(*this, D3D12_QUERY_TYPE_OCCLUSION);
		Query->OwningContext = this;
	}
	else
	{
		// not supported/needed for RQT_AbsoluteTime
		check(0);
	}

#if EXECUTE_DEBUG_COMMAND_LISTS
	GIsDoingQuery = true;
#endif
}

void FD3D12CommandContext::RHIEndRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	FD3D12OcclusionQuery* Query = FD3D12DynamicRHI::ResourceCast(QueryRHI);

	switch (Query->Type)
	{
	case RQT_Occlusion:
	{
		// End the query
		check(IsDefaultContext());
		FD3D12QueryHeap* pQueryHeap = GetParentDevice()->GetQueryHeap();
		pQueryHeap->EndQuery(*this, D3D12_QUERY_TYPE_OCCLUSION, Query->HeapIndex);

		// Note: This occlusion query result really isn't ready until it's resolved. 
		// This code assumes it will be resolved on the same command list.
		Query->CLSyncPoint = CommandListHandle;

		CommandListHandle.UpdateResidency(pQueryHeap->GetResultBuffer());
		check(Query->OwningContext == this);
		break;
	}

	case RQT_AbsoluteTime:
	{
		Query->bResultIsCached = false;
		Query->CLSyncPoint = CommandListHandle;
		Query->OwningContext = this;
		this->otherWorkCounter += 2;	// +2 For the EndQuery and the ResolveQueryData
		CommandListHandle->EndQuery(Query->QueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, Query->HeapIndex);
		CommandListHandle->ResolveQueryData(Query->QueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, Query->HeapIndex, 1, Query->ResultBuffer, sizeof(uint64) * Query->HeapIndex);
		break;
	}

	default:
		check(false);
	}

#if EXECUTE_DEBUG_COMMAND_LISTS
	GIsDoingQuery = false;
#endif
}

// Primitive drawing.

static D3D_PRIMITIVE_TOPOLOGY GetD3D12PrimitiveType(uint32 PrimitiveType, bool bUsingTessellation)
{
	if (bUsingTessellation)
	{
		switch (PrimitiveType)
		{
		case PT_1_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
		case PT_2_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST;

			// This is the case for tessellation without AEN or other buffers, so just flip to 3 CPs
		case PT_TriangleList: return D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;

		case PT_LineList:
		case PT_TriangleStrip:
		case PT_QuadList:
		case PT_PointList:
			UE_LOG(LogD3D12RHI, Fatal, TEXT("Invalid type specified for tessellated render, probably missing a case in FSkeletalMeshSceneProxy::DrawDynamicElementsByMaterial or FStaticMeshSceneProxy::GetMeshElement"));
			break;
		default:
			// Other cases are valid.
			break;
		};
	}

	switch (PrimitiveType)
	{
	case PT_TriangleList: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case PT_TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case PT_LineList: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	case PT_PointList: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;

		// ControlPointPatchList types will pretend to be TRIANGLELISTS with a stride of N 
		// (where N is the number of control points specified), so we can return them for
		// tessellation and non-tessellation. This functionality is only used when rendering a 
		// default material with something that claims to be tessellated, generally because the 
		// tessellation material failed to compile for some reason.
	case PT_3_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
	case PT_4_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
	case PT_5_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST;
	case PT_6_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST;
	case PT_7_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST;
	case PT_8_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST;
	case PT_9_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST;
	case PT_10_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST;
	case PT_11_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST;
	case PT_12_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST;
	case PT_13_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST;
	case PT_14_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST;
	case PT_15_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST;
	case PT_16_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST;
	case PT_17_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST;
	case PT_18_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST;
	case PT_19_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST;
	case PT_20_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST;
	case PT_21_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST;
	case PT_22_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST;
	case PT_23_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST;
	case PT_24_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST;
	case PT_25_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST;
	case PT_26_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST;
	case PT_27_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST;
	case PT_28_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST;
	case PT_29_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST;
	case PT_30_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST;
	case PT_31_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST;
	case PT_32_ControlPointPatchList: return D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST;
	default: UE_LOG(LogD3D12RHI, Fatal, TEXT("Unknown primitive type: %u"), PrimitiveType);
	};

	return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void FD3D12CommandContext::CommitNonComputeShaderConstants()
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12CommitGraphicsConstants);

	FD3D12BoundShaderState* RESTRICT CurrentBoundShaderStatePtr = CurrentBoundShaderState.GetReference();

	check(CurrentBoundShaderStatePtr);

	// Only set the constant buffer if this shader needs the global constant buffer bound
	// Otherwise we will overwrite a different constant buffer
	if (CurrentBoundShaderStatePtr->bShaderNeedsGlobalConstantBuffer[SF_Vertex])
	{
		// Commit and bind vertex shader constants
		for (uint32 i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
		{
			FD3D12ConstantBuffer* ConstantBuffer = VSConstantBuffers[i];
			FD3DRHIUtil::CommitConstants<SF_Vertex>(ConstantsAllocator, ConstantBuffer, StateCache, i, bDiscardSharedConstants);
		}
	}

	// Skip HS/DS CB updates in cases where tessellation isn't being used
	// Note that this is *potentially* unsafe because bDiscardSharedConstants is cleared at the
	// end of the function, however we're OK for now because bDiscardSharedConstants
	// is always reset whenever bUsingTessellation changes in SetBoundShaderState()
	if (bUsingTessellation)
	{
		if (CurrentBoundShaderStatePtr->bShaderNeedsGlobalConstantBuffer[SF_Hull])
		{
			// Commit and bind hull shader constants
			for (uint32 i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
			{
				FD3D12ConstantBuffer* ConstantBuffer = HSConstantBuffers[i];
				FD3DRHIUtil::CommitConstants<SF_Hull>(ConstantsAllocator, ConstantBuffer, StateCache, i, bDiscardSharedConstants);
			}
		}

		if (CurrentBoundShaderStatePtr->bShaderNeedsGlobalConstantBuffer[SF_Domain])
		{
			// Commit and bind domain shader constants
			for (uint32 i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
			{
				FD3D12ConstantBuffer* ConstantBuffer = DSConstantBuffers[i];
				FD3DRHIUtil::CommitConstants<SF_Domain>(ConstantsAllocator, ConstantBuffer, StateCache, i, bDiscardSharedConstants);
			}
		}
	}

	if (CurrentBoundShaderStatePtr->bShaderNeedsGlobalConstantBuffer[SF_Geometry])
	{
		// Commit and bind geometry shader constants
		for (uint32 i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
		{
			FD3D12ConstantBuffer* ConstantBuffer = GSConstantBuffers[i];
			FD3DRHIUtil::CommitConstants<SF_Geometry>(ConstantsAllocator, ConstantBuffer, StateCache, i, bDiscardSharedConstants);
		}
	}

	if (CurrentBoundShaderStatePtr->bShaderNeedsGlobalConstantBuffer[SF_Pixel])
	{
		// Commit and bind pixel shader constants
		for (uint32 i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
		{
			FD3D12ConstantBuffer* ConstantBuffer = PSConstantBuffers[i];
			FD3DRHIUtil::CommitConstants<SF_Pixel>(ConstantsAllocator, ConstantBuffer, StateCache, i, bDiscardSharedConstants);
		}
	}

	bDiscardSharedConstants = false;
}

void FD3D12CommandContext::CommitComputeShaderConstants()
{
	bool bLocalDiscardSharedConstants = true;

	// Commit and bind compute shader constants
	for (uint32 i=0;i<MAX_CONSTANT_BUFFER_SLOTS; i++)
	{
		FD3D12ConstantBuffer* ConstantBuffer = CSConstantBuffers[i];
		FD3DRHIUtil::CommitConstants<SF_Compute>(ConstantsAllocator, ConstantBuffer, StateCache, i, bDiscardSharedConstants);
	}
}

template <EShaderFrequency Frequency>
FORCEINLINE void SetResource(FD3D12CommandContext& CmdContext, uint32 BindIndex, FD3D12ResourceLocation* RESTRICT ShaderResource, FD3D12ShaderResourceView* RESTRICT SRV)
{
	// We set the resource through the RHI to track state for the purposes of unbinding SRVs when a UAV or RTV is bound.
	// todo: need to support SRV_Static for faster calls when possible
	CmdContext.SetShaderResourceView<Frequency>(ShaderResource, SRV, BindIndex, FD3D12StateCache::SRV_Unknown);
}

template <EShaderFrequency Frequency>
FORCEINLINE void SetResource(FD3D12CommandContext& CmdContext, uint32 BindIndex, FD3D12SamplerState* RESTRICT SamplerState)
{
	CmdContext.StateCache.SetSamplerState<Frequency>(SamplerState, BindIndex);
}

template <EShaderFrequency ShaderFrequency>
inline int32 SetShaderResourcesFromBuffer_Surface(FD3D12CommandContext& CmdContext, FD3D12UniformBuffer* RESTRICT Buffer, const uint32 * RESTRICT ResourceMap, int32 BufferIndex)
{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	float CurrentTime = FApp::GetCurrentTime();
	int32 NumSetCalls = 0;
	uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			FD3D12BaseShaderResource* ShaderResource = nullptr;
			FD3D12ShaderResourceView* D3D12Resource = nullptr;

			FRHITexture* TextureRHI = (FRHITexture*)Resources[ResourceIndex].GetReference();
			TextureRHI->SetLastRenderTime(CurrentTime);

			FD3D12TextureBase* TextureD3D12 = GetD3D12TextureFromRHITexture(TextureRHI);
			ShaderResource = TextureD3D12->GetBaseShaderResource();
			D3D12Resource = TextureD3D12->GetShaderResourceView();

			SetResource<ShaderFrequency>(CmdContext, BindIndex, ShaderResource->ResourceLocation, D3D12Resource);
			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}
	return NumSetCalls;
}

template <EShaderFrequency ShaderFrequency>
inline int32 SetShaderResourcesFromBuffer_SRV(FD3D12CommandContext& CmdContext, FD3D12UniformBuffer* RESTRICT Buffer, const uint32 * RESTRICT ResourceMap, int32 BufferIndex)
{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	int32 NumSetCalls = 0;
	uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			FD3D12ShaderResourceView* ShaderResourceViewRHI = (FD3D12ShaderResourceView*)Resources[ResourceIndex].GetReference();

			SetResource<ShaderFrequency>(CmdContext, BindIndex, ShaderResourceViewRHI->GetResourceLocation(), ShaderResourceViewRHI);
			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}
	return NumSetCalls;
}

template <EShaderFrequency ShaderFrequency>
inline int32 SetShaderResourcesFromBuffer_Sampler(FD3D12CommandContext& CmdContext, FD3D12UniformBuffer* RESTRICT Buffer, const uint32 * RESTRICT ResourceMap, int32 BufferIndex)

{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	int32 NumSetCalls = 0;
	uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			FD3D12SamplerState* D3D12Resource = (FD3D12SamplerState*)Resources[ResourceIndex].GetReference();

			SetResource<ShaderFrequency>(CmdContext, BindIndex, D3D12Resource);
			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}
	return NumSetCalls;
}

template <class ShaderType>
void FD3D12CommandContext::SetResourcesFromTables(const ShaderType* RESTRICT Shader)
{
	checkSlow(Shader);

	// Mask the dirty bits by those buffers from which the shader has bound resources.
	uint32 DirtyBits = Shader->ShaderResourceTable.ResourceTableBits & DirtyUniformBuffers[ShaderType::StaticFrequency];
	uint32 NumSetCalls = 0;
	while (DirtyBits)
	{
		// Scan for the lowest set bit, compute its index, clear it in the set of dirty bits.
		const uint32 LowestBitMask = (DirtyBits)& (-(int32)DirtyBits);
		const int32 BufferIndex = FMath::FloorLog2(LowestBitMask); // todo: This has a branch on zero, we know it could never be zero...
		DirtyBits ^= LowestBitMask;
		FD3D12UniformBuffer* Buffer = (FD3D12UniformBuffer*)BoundUniformBuffers[ShaderType::StaticFrequency][BufferIndex].GetReference();
		check(Buffer);
		check(BufferIndex < Shader->ShaderResourceTable.ResourceTableLayoutHashes.Num());
		check(Buffer->GetLayout().GetHash() == Shader->ShaderResourceTable.ResourceTableLayoutHashes[BufferIndex]);

		// todo: could make this two pass: gather then set
		NumSetCalls += SetShaderResourcesFromBuffer_Surface<(EShaderFrequency)ShaderType::StaticFrequency>(*this, Buffer, Shader->ShaderResourceTable.TextureMap.GetData(), BufferIndex);
		NumSetCalls += SetShaderResourcesFromBuffer_SRV<(EShaderFrequency)ShaderType::StaticFrequency>(*this, Buffer, Shader->ShaderResourceTable.ShaderResourceViewMap.GetData(), BufferIndex);
		NumSetCalls += SetShaderResourcesFromBuffer_Sampler<(EShaderFrequency)ShaderType::StaticFrequency>(*this, Buffer, Shader->ShaderResourceTable.SamplerMap.GetData(), BufferIndex);

	}
	DirtyUniformBuffers[ShaderType::StaticFrequency] = 0;
	OwningRHI.IncrementSetTextureInTableCalls(NumSetCalls);
}

void FD3D12CommandContext::CommitGraphicsResourceTables()
{
	uint32 Start = FPlatformTime::Cycles();

	FD3D12BoundShaderState* RESTRICT CurrentBoundShaderStatePtr = CurrentBoundShaderState.GetReference();

	check(CurrentBoundShaderStatePtr);

	if (auto* Shader = CurrentBoundShaderStatePtr->GetVertexShader())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = CurrentBoundShaderStatePtr->GetPixelShader())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = CurrentBoundShaderStatePtr->GetHullShader())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = CurrentBoundShaderStatePtr->GetDomainShader())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = CurrentBoundShaderStatePtr->GetGeometryShader())
	{
		SetResourcesFromTables(Shader);
	}

	OwningRHI.IncrementCommitComputeResourceTables(FPlatformTime::Cycles() - Start);
}

void FD3D12CommandContext::CommitComputeResourceTables(FD3D12ComputeShader* InComputeShader)
{
	FD3D12ComputeShader* RESTRICT ComputeShader = InComputeShader;
	check(ComputeShader);
	SetResourcesFromTables(ComputeShader);
}

void FD3D12CommandContext::RHIDrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	RHI_DRAW_CALL_STATS(PrimitiveType, NumInstances*NumPrimitives);

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	uint32 VertexCount = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);

	if (IsDefaultContext())
	{
		OwningRHI.RegisterGPUWork(NumPrimitives * NumInstances, VertexCount * NumInstances);
	}

	StateCache.SetPrimitiveTopology(GetD3D12PrimitiveType(PrimitiveType, bUsingTessellation));

	StateCache.ApplyState();
	numDraws++;
	CommandListHandle->DrawInstanced(VertexCount, FMath::Max<uint32>(1, NumInstances), BaseVertexIndex, 0);
#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);

}

void FD3D12CommandContext::RHIDrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
	FD3D12VertexBuffer* ArgumentBuffer = FD3D12DynamicRHI::ResourceCast(ArgumentBufferRHI);

	RHI_DRAW_CALL_INC();

	if (IsDefaultContext())
	{
		OwningRHI.RegisterGPUWork(0);
	}

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	StateCache.SetPrimitiveTopology(GetD3D12PrimitiveType(PrimitiveType, bUsingTessellation));

	StateCache.ApplyState();

	FD3D12DynamicRHI::TransitionResource(CommandListHandle, ArgumentBuffer->ResourceLocation->GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);

	numDraws++;
	CommandListHandle->ExecuteIndirect(
		GetParentDevice()->GetDrawIndirectCommandSignature(),
		1,
		ArgumentBuffer->ResourceLocation->GetResource()->GetResource(),
		ArgumentBuffer->ResourceLocation->GetOffset() + ArgumentOffset,
		NULL,
		0
		);

	CommandListHandle.UpdateResidency(ArgumentBuffer->ResourceLocation->GetResource());

#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);

}

void FD3D12CommandContext::RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances)
{
	FD3D12IndexBuffer* IndexBuffer = FD3D12DynamicRHI::ResourceCast(IndexBufferRHI);
	FD3D12StructuredBuffer* ArgumentsBuffer = FD3D12DynamicRHI::ResourceCast(ArgumentsBufferRHI);

	RHI_DRAW_CALL_INC();

	if (IsDefaultContext())
	{
		OwningRHI.RegisterGPUWork(1);
	}

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	// determine 16bit vs 32bit indices
	uint32 SizeFormat = sizeof(DXGI_FORMAT);
	const DXGI_FORMAT Format = (IndexBuffer->GetStride() == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);

	StateCache.SetIndexBuffer(IndexBuffer->ResourceLocation.GetReference(), Format, 0);
	StateCache.SetPrimitiveTopology(GetD3D12PrimitiveType(PrimitiveType, bUsingTessellation));
	StateCache.ApplyState();


	FD3D12DynamicRHI::TransitionResource(CommandListHandle, ArgumentsBuffer->Resource, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);

	numDraws++;
	CommandListHandle->ExecuteIndirect(
		GetParentDevice()->GetDrawIndexedIndirectCommandSignature(),
		1,
		ArgumentsBuffer->Resource->GetResource(),
		ArgumentsBuffer->ResourceLocation->GetOffset() + DrawArgumentsIndex * ArgumentsBuffer->GetStride(),
		NULL,
		0
		);

	CommandListHandle.UpdateResidency(ArgumentsBuffer->Resource);

#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);
}

void FD3D12CommandContext::RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	FD3D12IndexBuffer* IndexBuffer = FD3D12DynamicRHI::ResourceCast(IndexBufferRHI);

	// called should make sure the input is valid, this avoid hidden bugs
	ensure(NumPrimitives > 0);

	RHI_DRAW_CALL_STATS(PrimitiveType, NumInstances*NumPrimitives);

	if (IsDefaultContext())
	{
		OwningRHI.RegisterGPUWork(NumPrimitives * NumInstances, NumVertices * NumInstances);
	}

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	// determine 16bit vs 32bit indices
	uint32 SizeFormat = sizeof(DXGI_FORMAT);
	const DXGI_FORMAT Format = (IndexBuffer->GetStride() == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);

	uint32 IndexCount = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);

	// Verify that we are not trying to read outside the index buffer range
	// test is an optimized version of: StartIndex + IndexCount <= IndexBuffer->GetSize() / IndexBuffer->GetStride() 
	checkf((StartIndex + IndexCount) * IndexBuffer->GetStride() <= IndexBuffer->GetSize(),
		TEXT("Start %u, Count %u, Type %u, Buffer Size %u, Buffer stride %u"), StartIndex, IndexCount, PrimitiveType, IndexBuffer->GetSize(), IndexBuffer->GetStride());

	StateCache.SetIndexBuffer(IndexBuffer->ResourceLocation.GetReference(), Format, 0);
	StateCache.SetPrimitiveTopology(GetD3D12PrimitiveType(PrimitiveType, bUsingTessellation));
	StateCache.ApplyState();

	numDraws++;
	CommandListHandle->DrawIndexedInstanced(IndexCount, FMath::Max<uint32>(1, NumInstances), StartIndex, BaseVertexIndex, FirstInstance);
#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);

}

void FD3D12CommandContext::RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType, FIndexBufferRHIParamRef IndexBufferRHI, FVertexBufferRHIParamRef ArgumentBufferRHI, uint32 ArgumentOffset)
{
	FD3D12IndexBuffer* IndexBuffer = FD3D12DynamicRHI::ResourceCast(IndexBufferRHI);
	FD3D12VertexBuffer* ArgumentBuffer = FD3D12DynamicRHI::ResourceCast(ArgumentBufferRHI);

	RHI_DRAW_CALL_INC();

	if (IsDefaultContext())
	{
		OwningRHI.RegisterGPUWork(0);
	}

	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();

	// Set the index buffer.
	const uint32 SizeFormat = sizeof(DXGI_FORMAT);
	const DXGI_FORMAT Format = (IndexBuffer->GetStride() == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);
	StateCache.SetIndexBuffer(IndexBuffer->ResourceLocation.GetReference(), Format, 0);
	StateCache.SetPrimitiveTopology(GetD3D12PrimitiveType(PrimitiveType, bUsingTessellation));
	StateCache.ApplyState();

	FD3D12DynamicRHI::TransitionResource(CommandListHandle, ArgumentBuffer->ResourceLocation->GetResource(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_BARRIER_TYPE_TRANSITION);

	numDraws++;
	CommandListHandle->ExecuteIndirect(
		GetParentDevice()->GetDrawIndexedIndirectCommandSignature(),
		1,
		ArgumentBuffer->ResourceLocation->GetResource()->GetResource(),
		ArgumentBuffer->ResourceLocation->GetOffset() + ArgumentOffset,
		NULL,
		0
		);

	CommandListHandle.UpdateResidency(ArgumentBuffer->ResourceLocation->GetResource());

#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);
}

/**
* Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawPrimitiveUP
* @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
* @param NumPrimitives The number of primitives in the VertexData buffer
* @param NumVertices The number of vertices to be written
* @param VertexDataStride Size of each vertex
* @param OutVertexData Reference to the allocated vertex memory
*/
void FD3D12CommandContext::RHIBeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData)
{
	checkSlow(PendingNumVertices == 0);

	// Remember the parameters for the draw call.
	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingNumVertices = NumVertices;
	PendingVertexDataStride = VertexDataStride;

	// Map the dynamic buffer.
	OutVertexData = DynamicVB.Lock(NumVertices * VertexDataStride);
}

/**
* Draw a primitive using the vertex data populated since RHIBeginDrawPrimitiveUP and clean up any memory as needed
*/
void FD3D12CommandContext::RHIEndDrawPrimitiveUP()
{
	RHI_DRAW_CALL_STATS(PendingPrimitiveType, PendingNumPrimitives);

	checkSlow(!bUsingTessellation || PendingPrimitiveType == PT_TriangleList);

	if (IsDefaultContext())
	{
		OwningRHI.RegisterGPUWork(PendingNumPrimitives, PendingNumVertices);
	}

	// Unmap the dynamic vertex buffer.
	FD3D12ResourceLocation* BufferLocation = DynamicVB.Unlock();
	uint32 VBOffset = 0;

	// Issue the draw call.
	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();
	StateCache.SetStreamSource(BufferLocation, 0, PendingVertexDataStride, VBOffset);
	StateCache.SetPrimitiveTopology(GetD3D12PrimitiveType(PendingPrimitiveType, bUsingTessellation));
	StateCache.ApplyState();
	numDraws++;
	CommandListHandle->DrawInstanced(PendingNumVertices, 1, 0, 0);
#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);

	// Clear these parameters.
	PendingPrimitiveType = 0;
	PendingNumPrimitives = 0;
	PendingNumVertices = 0;
	PendingVertexDataStride = 0;
}

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
void FD3D12CommandContext::RHIBeginDrawIndexedPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData)
{
	checkSlow((sizeof(uint16) == IndexDataStride) || (sizeof(uint32) == IndexDataStride));

	// Store off information needed for the draw call.
	PendingPrimitiveType = PrimitiveType;
	PendingNumPrimitives = NumPrimitives;
	PendingMinVertexIndex = MinVertexIndex;
	PendingIndexDataStride = IndexDataStride;
	PendingNumVertices = NumVertices;
	PendingNumIndices = NumIndices;
	PendingVertexDataStride = VertexDataStride;

	// Map dynamic vertex and index buffers.
	OutVertexData = DynamicVB.Lock(NumVertices * VertexDataStride);
	OutIndexData = DynamicIB.Lock(NumIndices * IndexDataStride);
}

/**
* Draw a primitive using the vertex and index data populated since RHIBeginDrawIndexedPrimitiveUP and clean up any memory as needed
*/
void FD3D12CommandContext::RHIEndDrawIndexedPrimitiveUP()
{
	// tessellation only supports trilists
	checkSlow(!bUsingTessellation || PendingPrimitiveType == PT_TriangleList);

	RHI_DRAW_CALL_STATS(PendingPrimitiveType, PendingNumPrimitives);

	if (IsDefaultContext())
	{
		OwningRHI.RegisterGPUWork(PendingNumPrimitives, PendingNumVertices);
	}

	// Unmap the dynamic buffers.
	FD3D12ResourceLocation* VertexBufferLocation = DynamicVB.Unlock();
	FD3D12ResourceLocation* IndexBufferLocation = DynamicIB.Unlock();
	uint32 VBOffset = 0;

	// Issue the draw call.
	CommitGraphicsResourceTables();
	CommitNonComputeShaderConstants();
	StateCache.SetStreamSource(VertexBufferLocation, 0, PendingVertexDataStride, VBOffset);
	StateCache.SetIndexBuffer(IndexBufferLocation, PendingIndexDataStride == sizeof(uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
	StateCache.SetPrimitiveTopology(GetD3D12PrimitiveType(PendingPrimitiveType, bUsingTessellation));
	StateCache.ApplyState();

	numDraws++;
	CommandListHandle->DrawIndexedInstanced(PendingNumIndices, 1, 0, PendingMinVertexIndex, 0);
#if UE_BUILD_DEBUG	
	OwningRHI.DrawCount++;
#endif
	DEBUG_EXECUTE_COMMAND_LIST(this);

	//It's important to release the locations so the fast alloc page can be freed
	DynamicVB.ReleaseResourceLocation();
	DynamicIB.ReleaseResourceLocation();

	// Clear these parameters.
	PendingPrimitiveType = 0;
	PendingNumPrimitives = 0;
	PendingMinVertexIndex = 0;
	PendingIndexDataStride = 0;
	PendingNumVertices = 0;
	PendingNumIndices = 0;
	PendingVertexDataStride = 0;
}

// Raster operations.
void FD3D12CommandContext::RHIClear(bool bClearColor, const FLinearColor& Color, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntRect ExcludeRect)
{
	RHIClearMRTImpl(bClearColor, 1, &Color, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
}

void FD3D12CommandContext::RHIClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntRect ExcludeRect)
{
	RHIClearMRTImpl(bClearColor, NumClearColors, ClearColorArray, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
}

void FD3D12CommandContext::RHIClearMRTImpl(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntRect ExcludeRect)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12ClearMRT);

	uint32 NumViews = 1;
	D3D12_VIEWPORT Viewport;
	StateCache.GetViewports(&NumViews, &Viewport);

	if (ExcludeRect.Min.X == 0 && ExcludeRect.Width() == Viewport.Width && ExcludeRect.Min.Y == 0 && ExcludeRect.Height() == Viewport.Height)
	{
		// no need to do anything
		return;
	}

	D3D12_RECT ScissorRect;
	StateCache.GetScissorRect(&ScissorRect);

	if (ScissorRect.left >= ScissorRect.right || ScissorRect.top >= ScissorRect.bottom)
	{
		return;
	}

	FD3D12RenderTargetView* RenderTargetViews[MaxSimultaneousRenderTargets];
	FD3D12DepthStencilView* DSView = nullptr;
	uint32 NumSimultaneousRTs = 0;
	StateCache.GetRenderTargets(RenderTargetViews, &NumSimultaneousRTs, &DSView);
	FD3D12BoundRenderTargets BoundRenderTargets(RenderTargetViews, NumSimultaneousRTs, DSView);
	FD3D12DepthStencilView* DepthStencilView = BoundRenderTargets.GetDepthStencilView();

	// Use rounding for when the number can't be perfectly represented by a float
	const LONG Width = static_cast<LONG>(FMath::RoundToInt(Viewport.Width));
	const LONG Height = static_cast<LONG>(FMath::RoundToInt(Viewport.Height));

	// When clearing we must pay attention to the currently set scissor rect
	bool bClearCoversEntireSurface = false;
	if (ScissorRect.left <= 0 && ScissorRect.top <= 0 &&
		ScissorRect.right >= Width && ScissorRect.bottom >= Height)
	{
		bClearCoversEntireSurface = true;
	}

	// Must specify enough clear colors for all active RTs
	check(!bClearColor || NumClearColors >= BoundRenderTargets.GetNumActiveTargets());

	const bool bSupportsFastClear = true;
	uint32 ClearRectCount = 0;
	D3D12_RECT* pClearRects = nullptr;
	D3D12_RECT ClearRects[4];

	// Only pass a rect down to the driver if we specifically want to clear a sub-rect
	if (!bSupportsFastClear || !bClearCoversEntireSurface)
	{
// TODO: Exclude rects are an optional optimzation, need to make them work with the scissor rect as well
#if 0
		// Add clear rects only if fast clears are not supported.
		if (ExcludeRect.Width() > 0 && ExcludeRect.Height() > 0)
		{
			if (ExcludeRect.Min.Y > 0)
			{
				ClearRects[ClearRectCount] = CD3DX12_RECT(0, 0, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION, ExcludeRect.Min.Y);
				ClearRectCount++;
			}

			ClearRects[ClearRectCount] = CD3DX12_RECT(0, ExcludeRect.Max.Y, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION);
			ClearRectCount++;

			if (ExcludeRect.Min.X > 0)
			{
				ClearRects[ClearRectCount] = CD3DX12_RECT(0, ExcludeRect.Min.Y, ExcludeRect.Min.X, ExcludeRect.Max.Y);
				ClearRectCount++;
			}

			ClearRects[ClearRectCount] = CD3DX12_RECT(ExcludeRect.Max.X, ExcludeRect.Min.Y, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION, ExcludeRect.Max.Y);
			ClearRectCount++;
		}
		else
#endif
		{
			ClearRects[ClearRectCount] = ScissorRect;
			ClearRectCount++;
		}

		pClearRects = ClearRects;

		static const bool bSpewPerfWarnings = false;

		if (bSpewPerfWarnings)
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("RHIClearMRTImpl: Using non-fast clear path! This has performance implications"));
			UE_LOG(LogD3D12RHI, Warning, TEXT("       Viewport: Width %d, Height: %d"), static_cast<LONG>(FMath::RoundToInt(Viewport.Width)), static_cast<LONG>(FMath::RoundToInt(Viewport.Height)));
			UE_LOG(LogD3D12RHI, Warning, TEXT("   Scissor Rect: Width %d, Height: %d"), ScissorRect.right, ScissorRect.bottom);
		}
	}

	const bool ClearRTV = bClearColor && BoundRenderTargets.GetNumActiveTargets() > 0;
	const bool ClearDSV = (bClearDepth || bClearStencil) && DepthStencilView;

	if (ClearRTV)
	{
		for (int32 TargetIndex = 0; TargetIndex < BoundRenderTargets.GetNumActiveTargets(); TargetIndex++)
		{
			FD3D12RenderTargetView* RTView = BoundRenderTargets.GetRenderTargetView(TargetIndex);

			FD3D12DynamicRHI::TransitionResource(CommandListHandle, RTView, D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
	}

	uint32 ClearFlags = 0;
	if (ClearDSV)
	{
		if (bClearDepth && DepthStencilView->HasDepth())
		{
			ClearFlags |= D3D12_CLEAR_FLAG_DEPTH;
		}
		else if (bClearDepth)
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("RHIClearMRTImpl: Asking to clear a DSV that does not store depth."));
		}

		if (bClearStencil && DepthStencilView->HasStencil())
		{
			ClearFlags |= D3D12_CLEAR_FLAG_STENCIL;
		}
		else if (bClearStencil)
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("RHIClearMRTImpl: Asking to clear a DSV that does not store stencil."));
		}

		if (bClearDepth && (!DepthStencilView->HasStencil() || bClearStencil))
		{
			// Transition the entire view (Both depth and stencil planes if applicable)
			// Some DSVs don't have stencil bits.
			FD3D12DynamicRHI::TransitionResource(CommandListHandle, DepthStencilView, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}
		else
		{
			if (bClearDepth)
			{
				// Transition just the depth plane
				check(bClearDepth && !bClearStencil);
				FD3D12DynamicRHI::TransitionResource(CommandListHandle, DepthStencilView->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, DepthStencilView->GetDepthOnlyViewSubresourceSubset());
			}
			else
			{
				// Transition just the stencil plane
				check(!bClearDepth && bClearStencil);
				FD3D12DynamicRHI::TransitionResource(CommandListHandle, DepthStencilView->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, DepthStencilView->GetStencilOnlyViewSubresourceSubset());
			}
		}
	}

	if (ClearRTV || ClearDSV)
	{
		CommandListHandle.FlushResourceBarriers();

		if (ClearRTV)
		{
			for (int32 TargetIndex = 0; TargetIndex < BoundRenderTargets.GetNumActiveTargets(); TargetIndex++)
			{
				FD3D12RenderTargetView* RTView = BoundRenderTargets.GetRenderTargetView(TargetIndex);

				numClears++;
				CommandListHandle->ClearRenderTargetView(RTView->GetView(), (float*)&ClearColorArray[TargetIndex], ClearRectCount, pClearRects);
				CommandListHandle.UpdateResidency(RTView->GetResource());
			}
		}

		if (ClearDSV)
		{
			numClears++;
			CommandListHandle->ClearDepthStencilView(DepthStencilView->GetView(), (D3D12_CLEAR_FLAGS)ClearFlags, Depth, Stencil, ClearRectCount, pClearRects);
			CommandListHandle.UpdateResidency(DepthStencilView->GetResource());
		}
	}

	if (IsDefaultContext())
	{
		OwningRHI.RegisterGPUWork(0);
	}

	DEBUG_EXECUTE_COMMAND_LIST(this);
}

void FD3D12CommandContext::RHIBindClearMRTValues(bool bClearColor, bool bClearDepth, bool bClearStencil)
{
	// Not necessary for d3d.
}

// Blocks the CPU until the GPU catches up and goes idle.
void FD3D12DynamicRHI::RHIBlockUntilGPUIdle()
{
	GetRHIDevice()->GetCommandListManager().WaitForCommandQueueFlush();
	GetRHIDevice()->GetCopyCommandListManager().WaitForCommandQueueFlush();
	GetRHIDevice()->GetAsyncCommandListManager().WaitForCommandQueueFlush();
}

/*
* Returns the total GPU time taken to render the last frame. Same metric as FPlatformTime::Cycles().
*/
uint32 FD3D12DynamicRHI::RHIGetGPUFrameCycles()
{
	return GGPUFrameTime;
}

void FD3D12DynamicRHI::RHIExecuteCommandList(FRHICommandList* CmdList)
{
	check(0); // this path has gone stale and needs updated methods, starting at ERCT_SetScissorRect
}

// NVIDIA Depth Bounds Test interface
void FD3D12CommandContext::RHIEnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth)
{
	static bool bOnce = false;
	if (!bOnce)
	{
		UE_LOG(LogD3D12RHI, Warning, TEXT("RHIEnableDepthBoundsTest not supported on DX12."));
		bOnce = true;
	}
}

void FD3D12CommandContext::RHISubmitCommandsHint()
{
	if (bIsAsyncComputeContext)
	{
		GetParentDevice()->GetDefaultCommandContext().RHISubmitCommandsHint();
	}

	// Submit the work we have so far, and start a new command list.
	FlushCommands();

	if (PendingFence.GetReference())
	{
		// PendingFence always occurs on async context		
		PendingFence->Signal(GetCommandListManager().GetD3DCommandQueue());
		PendingFence = nullptr;
	}
}
