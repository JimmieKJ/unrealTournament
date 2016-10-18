// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// Implementation of Device Context State Caching to improve draw
//	thread performance by removing redundant device context calls.

//-----------------------------------------------------------------------------
//	Include Files
//-----------------------------------------------------------------------------
#include "D3D12RHIPrivate.h"
#include "Windows/D3D12StateCache.h"
#include <emmintrin.h>

extern bool D3D12RHI_ShouldCreateWithD3DDebug();

inline bool operator!=(D3D12_CPU_DESCRIPTOR_HANDLE lhs, D3D12_CPU_DESCRIPTOR_HANDLE rhs)
{
	return lhs.ptr != rhs.ptr;
}

#if D3D12_STATE_CACHE_RUNTIME_TOGGLE

// Default the state caching system to on.
bool GD3D12SkipStateCaching = false;

// A self registering exec helper to check for the TOGGLESTATECACHE command.
class FD3D12ToggleStateCacheExecHelper : public FSelfRegisteringExec
{
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
	{
		if (FParse::Command(&Cmd, TEXT("TOGGLESTATECACHE")))
		{
			GD3D12SkipStateCaching = !GD3D12SkipStateCaching;
			Ar.Log(FString::Printf(TEXT("D3D12 State Caching: %s"), GD3D12SkipStateCaching ? TEXT("OFF") : TEXT("ON")));
			return true;
		}
		return false;
	}
};
static FD3D12ToggleStateCacheExecHelper GD3D12ToggleStateCacheExecHelper;

#endif	// D3D12_STATE_CACHE_RUNTIME_TOGGLE

void FD3D12StateCacheBase::Init(FD3D12Device* InParent, FD3D12CommandContext* InCmdContext, const FD3D12StateCacheBase* AncestralState, FD3D12SubAllocatedOnlineHeap::SubAllocationDesc& SubHeapDesc, bool bInAlwaysSetIndexBuffers)
{
	Parent = InParent;
	CmdContext = InCmdContext;

	// Cache the resource binding tier
	ResourceBindingTier = GetParentDevice()->GetResourceBindingTier();

	// Init the descriptor heaps
	const uint32 MaxDescriptorsForTier = (ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_1) ? NUM_VIEW_DESCRIPTORS_TIER_1 :
		NUM_VIEW_DESCRIPTORS_TIER_2;
	check(GLOBAL_VIEW_HEAP_SIZE <= MaxDescriptorsForTier)

	const uint32 NumSamplerDescriptors = NUM_SAMPLER_DESCRIPTORS;
	DescriptorCache.Init(InParent, InCmdContext, GLOBAL_VIEW_HEAP_SIZE, NumSamplerDescriptors, SubHeapDesc);

	if (AncestralState)
	{
		InheritState(*AncestralState);
	}
	else
	{
		ClearState();
	}

	bAlwaysSetIndexBuffers = bInAlwaysSetIndexBuffers;
}

void FD3D12StateCacheBase::Clear()
{
	ClearState();

	// Release references to cached objects
	DescriptorCache.Clear();
}

void FD3D12StateCacheBase::ClearSRVs()
{
	if (bSRVSCleared)
	{
		return;
	}

	PipelineState.Common.SRVCache.Clear();

	bSRVSCleared = true;
}

void FD3D12StateCacheBase::ClearSamplers()
{
	ZeroMemory(PipelineState.Common.CurrentSamplerStates, sizeof(PipelineState.Common.CurrentSamplerStates));
	for (uint32 ShaderFrequency = 0; ShaderFrequency < _countof(bNeedSetSamplersPerShaderStage); ShaderFrequency++)
	{
		bNeedSetSamplersPerShaderStage[ShaderFrequency] = true;
	}
	bNeedSetSamplers = true;
}

void FD3D12StateCacheBase::FlushComputeShaderCache(bool bForce)
{
	if (bAutoFlushComputeShaderCache || bForce)
	{
		FD3D12CommandListHandle& CommandList = CmdContext->CommandListHandle;
		CommandList.AddUAVBarrier();
	}
}

void FD3D12StateCacheBase::ClearState()
{
	// Shader Resource View State Cache
	bSRVSCleared = false;

	ClearSRVs();

	FMemory::Memzero(PipelineState.Common.CurrentShaderSamplerCounts, sizeof(PipelineState.Common.CurrentShaderSamplerCounts));
	FMemory::Memzero(PipelineState.Common.CurrentShaderSRVCounts, sizeof(PipelineState.Common.CurrentShaderSRVCounts));
	FMemory::Memzero(PipelineState.Common.CurrentShaderCBCounts, sizeof(PipelineState.Common.CurrentShaderCBCounts));
	FMemory::Memzero(PipelineState.Common.CurrentShaderUAVCounts, sizeof(PipelineState.Common.CurrentShaderUAVCounts));

	// Unordered Access View State Cache
	for (int i = 0; i < D3D12_PS_CS_UAV_REGISTER_COUNT; ++i)
	{
		PipelineState.Common.UnorderedAccessViewArray[SF_Pixel][i] = nullptr;
		PipelineState.Common.UnorderedAccessViewArray[SF_Compute][i] = nullptr;
	}
	PipelineState.Common.CurrentUAVStartSlot[SF_Pixel] = -1;
	PipelineState.Common.CurrentUAVStartSlot[SF_Compute] = -1;
	
	PipelineState.Graphics.HighLevelDesc.NumRenderTargets = 0;
	PipelineState.Graphics.CurrentNumberOfStreamOutTargets = 0;
	PipelineState.Graphics.CurrentNumberOfScissorRects = 0;

	// Rasterizer State Cache
	PipelineState.Graphics.HighLevelDesc.RasterizerState = nullptr;

	// Depth Stencil State Cache
	PipelineState.Graphics.CurrentReferenceStencil = 0;
	PipelineState.Graphics.HighLevelDesc.DepthStencilState = nullptr;
	PipelineState.Graphics.CurrentDepthStencilTarget = nullptr;

	// Shader Cache
	PipelineState.Graphics.HighLevelDesc.BoundShaderState = nullptr;
	PipelineState.Compute.CurrentComputeShader = nullptr;

	// Blend State Cache
	PipelineState.Graphics.CurrentBlendFactor[0] = 1.0f;
	PipelineState.Graphics.CurrentBlendFactor[1] = 1.0f;
	PipelineState.Graphics.CurrentBlendFactor[2] = 1.0f;
	PipelineState.Graphics.CurrentBlendFactor[3] = 1.0f;

	FMemory::Memset(&PipelineState.Graphics.CurrentViewport[0], 0, sizeof(D3D12_VIEWPORT) * D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
	PipelineState.Graphics.CurrentNumberOfViewports = 0;

	PipelineState.Graphics.bNeedRebuildPSO = true;
	PipelineState.Compute.bNeedRebuildPSO = true;
	PipelineState.Graphics.CurrentPipelineStateObject = nullptr;
	PipelineState.Compute.CurrentPipelineStateObject = nullptr;
	PipelineState.Common.CurrentPipelineStateObject = nullptr;
	FMemory::Memzero(PipelineState.Graphics.CurrentStreamOutTargets, sizeof(PipelineState.Graphics.CurrentStreamOutTargets));
	FMemory::Memzero(PipelineState.Graphics.CurrentSOOffsets, sizeof(PipelineState.Graphics.CurrentSOOffsets));

	FMemory::Memzero(PipelineState.Graphics.CurrentScissorRects, sizeof(PipelineState.Graphics.CurrentScissorRects));

	D3D12_RECT ScissorRect;
	ScissorRect.left = 0;
	ScissorRect.right = GetMax2DTextureDimension();
	ScissorRect.top = 0;
	ScissorRect.bottom = GetMax2DTextureDimension();
	SetScissorRects(1, &ScissorRect);

	PipelineState.Graphics.HighLevelDesc.SampleMask = 0xffffffff;
	PipelineState.Graphics.HighLevelDesc.BlendState = nullptr;

	for (int i = 0; i < D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++i)
	{
		PipelineState.Graphics.VBCache.CurrentVertexBufferResources[i] = nullptr;
	}

	FMemory::Memzero(PipelineState.Graphics.VBCache.CurrentVertexBufferViews, sizeof(PipelineState.Graphics.VBCache.CurrentVertexBufferViews));
	FMemory::Memzero(PipelineState.Graphics.VBCache.ResidencyHandles, sizeof(PipelineState.Graphics.VBCache.ResidencyHandles));

	PipelineState.Graphics.VBCache.MaxBoundVertexBufferIndex = INDEX_NONE;
	PipelineState.Graphics.VBCache.BoundVBMask = 0;

	FMemory::Memzero(PipelineState.Common.CurrentSamplerStates, sizeof(PipelineState.Common.CurrentSamplerStates));

	FMemory::Memzero(PipelineState.Graphics.RenderTargetArray, sizeof(PipelineState.Graphics.RenderTargetArray));

	FMemory::Memzero(PipelineState.Common.CurrentConstantBuffers, sizeof(PipelineState.Common.CurrentConstantBuffers));

	PipelineState.Graphics.IBCache.CurrentIndexBufferLocation = nullptr;
	PipelineState.Graphics.IBCache.ResidencyHandle = nullptr;
	FMemory::Memzero(&PipelineState.Graphics.IBCache.CurrentIndexBufferView, sizeof(PipelineState.Graphics.IBCache.CurrentIndexBufferView));

	PipelineState.Graphics.CurrentPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

	bAutoFlushComputeShaderCache = false;
}

void FD3D12StateCacheBase::RestoreState()
{
	// Mark bits dirty so the next call to ApplyState will set all this state again
	PipelineState.Common.bNeedSetPSO = true;
	bNeedSetVB = true;
	bNeedSetIB = true;
	bNeedSetSOs = true;
	bNeedSetRTs = true;
	bNeedSetViewports = true;
	bNeedSetScissorRects = true;
	bNeedSetPrimitiveTopology = true;
	bNeedSetBlendFactor = true;
	bNeedSetStencilRef = true;
	bNeedSetSamplers = true;
	bNeedSetSRVs = true;
	bNeedSetConstantBuffers = true;
	for (int i = 0; i < SF_NumFrequencies; i++)
	{
		bNeedSetSamplersPerShaderStage[i] = true;
		bNeedSetSRVsPerShaderStage[i] = true;
		bNeedSetConstantBuffersPerShaderStage[i] = true;
		bNeedSetUAVsPerShaderStage[i] = true;
	}
}

void FD3D12StateCacheBase::DirtyViewDescriptorTables()
{
	// Mark the CBV/SRV/UAV descriptor tables dirty for the current root signature.
	// Note: Descriptor table state is undefined at the beginning of a command list and after descriptor heaps are changed on a command list.
	// This will cause the next call to ApplyState to copy and set these descriptors again.

	// Could optimize here by only setting dirty flags for things that are valid with the current root signature.
	{
		bNeedSetSRVs = true;
		bNeedSetConstantBuffers = true;
		for (int i = 0; i < SF_NumFrequencies; i++)
		{
			bNeedSetSRVsPerShaderStage[i] = true;
			bNeedSetConstantBuffersPerShaderStage[i] = true;
			bNeedSetUAVsPerShaderStage[i] = true;
		}
	}
}

void FD3D12StateCacheBase::DirtySamplerDescriptorTables()
{
	// Mark the sampler descriptor tables dirty for the current root signature.
	// Note: Descriptor table state is undefined at the beginning of a command list and after descriptor heaps are changed on a command list.
	// This will cause the next call to ApplyState to copy and set these descriptors again.

	// Could optimize here by only setting dirty flags for things that are valid with the current root signature.
	{
		bNeedSetSamplers = true;
		for (int i = 0; i < SF_NumFrequencies; i++)
		{
			bNeedSetSamplersPerShaderStage[i] = true;
		}
	}
}

void FD3D12StateCacheBase::SetViewport(D3D12_VIEWPORT Viewport)
{
	if ((PipelineState.Graphics.CurrentNumberOfViewports != 1 || FMemory::Memcmp(&PipelineState.Graphics.CurrentViewport[0], &Viewport, sizeof(D3D12_VIEWPORT))) || GD3D12SkipStateCaching)
	{
		FMemory::Memcpy(&PipelineState.Graphics.CurrentViewport[0], &Viewport, sizeof(D3D12_VIEWPORT));
		PipelineState.Graphics.CurrentNumberOfViewports = 1;
		bNeedSetViewports = true;
		UpdateViewportScissorRects();
	}
}

void FD3D12StateCacheBase::SetViewports(uint32 Count, D3D12_VIEWPORT* Viewports)
{
	if ((PipelineState.Graphics.CurrentNumberOfViewports != Count || FMemory::Memcmp(&PipelineState.Graphics.CurrentViewport[0], Viewports, sizeof(D3D12_VIEWPORT)* Count)) || GD3D12SkipStateCaching)
	{
		FMemory::Memcpy(&PipelineState.Graphics.CurrentViewport[0], Viewports, sizeof(D3D12_VIEWPORT)* Count);
		PipelineState.Graphics.CurrentNumberOfViewports = Count;
		bNeedSetViewports = true;
		UpdateViewportScissorRects();
	}
}

void FD3D12StateCacheBase::UpdateViewportScissorRects()
{
	for (uint32 i = 0; i < PipelineState.Graphics.CurrentNumberOfScissorRects; ++i)
	{
		D3D12_VIEWPORT& Viewport		    = PipelineState.Graphics.CurrentViewport[FMath::Min(i, PipelineState.Graphics.CurrentNumberOfViewports)];
		D3D12_RECT&     ScissorRect         = PipelineState.Graphics.CurrentScissorRects[i];
		D3D12_RECT&     ViewportScissorRect = PipelineState.Graphics.CurrentViewportScissorRects[i];

		ViewportScissorRect.top    = FMath::Max(ScissorRect.top, (LONG)Viewport.TopLeftY);
		ViewportScissorRect.left   = FMath::Max(ScissorRect.left, (LONG)Viewport.TopLeftX);
		ViewportScissorRect.bottom = FMath::Min(ScissorRect.bottom, (LONG)Viewport.TopLeftY + (LONG)Viewport.Height);
		ViewportScissorRect.right  = FMath::Min(ScissorRect.right, (LONG)Viewport.TopLeftX + (LONG)Viewport.Width);
	}

	bNeedSetScissorRects = true;
}

void FD3D12StateCacheBase::SetScissorRect(D3D12_RECT ScissorRect)
{
	if ((PipelineState.Graphics.CurrentNumberOfScissorRects != 1 || FMemory::Memcmp(&PipelineState.Graphics.CurrentScissorRects[0], &ScissorRect, sizeof(D3D12_RECT))) || GD3D12SkipStateCaching)
	{
		FMemory::Memcpy(&PipelineState.Graphics.CurrentScissorRects[0], &ScissorRect, sizeof(D3D12_RECT));
		PipelineState.Graphics.CurrentNumberOfScissorRects = 1;
		UpdateViewportScissorRects();
	}
}

void FD3D12StateCacheBase::SetScissorRects(uint32 Count, D3D12_RECT* ScissorRects)
{
	if ((PipelineState.Graphics.CurrentNumberOfScissorRects != Count || FMemory::Memcmp(&PipelineState.Graphics.CurrentScissorRects[0], ScissorRects, sizeof(D3D12_RECT)* Count)) || GD3D12SkipStateCaching)
	{
		FMemory::Memcpy(&PipelineState.Graphics.CurrentScissorRects[0], ScissorRects, sizeof(D3D12_RECT)* Count);
		PipelineState.Graphics.CurrentNumberOfScissorRects = Count;
		UpdateViewportScissorRects();
	}
}

void FD3D12StateCacheBase::ApplyState(bool IsCompute)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12ApplyStateTime);
	const bool bForceState = false;

	FD3D12CommandListHandle& CommandList = CmdContext->CommandListHandle;
	FD3D12PipelineStateCache& PSOCache = GetParentDevice()->GetPSOCache();
	const FD3D12RootSignature* const pRootSignature = IsCompute ?
		PipelineState.Compute.CurrentComputeShader->pRootSignature : PipelineState.Graphics.HighLevelDesc.BoundShaderState->pRootSignature;

	// PSO
	if (IsCompute)
	{
		if (PipelineState.Compute.bNeedRebuildPSO || bForceState)
		{
			FD3D12ComputePipelineStateDesc psoDescCompute;
			FMemory::Memzero(&psoDescCompute, sizeof(psoDescCompute));

			psoDescCompute.pRootSignature = PipelineState.Compute.CurrentComputeShader->pRootSignature;
			psoDescCompute.Desc.pRootSignature = psoDescCompute.pRootSignature->GetRootSignature();
			psoDescCompute.Desc.CS = PipelineState.Compute.CurrentComputeShader->ShaderBytecode.GetShaderBytecode();
			psoDescCompute.CSHash = PipelineState.Compute.CurrentComputeShader->ShaderBytecode.GetHash();

			ID3D12PipelineState* psoCompute = PSOCache.FindCompute(psoDescCompute);

			// Save the current PSO
			PipelineState.Common.CurrentPipelineStateObject = psoCompute;
			PipelineState.Compute.CurrentPipelineStateObject = psoCompute;

			// Indicate we need to set the PSO on the command list
			PipelineState.Common.bNeedSetPSO = true;
			PipelineState.Compute.bNeedRebuildPSO = false;
		}

		// See if we need to set a compute root signature
		if (PipelineState.Compute.bNeedSetRootSignature)
		{
			CommandList->SetComputeRootSignature(pRootSignature->GetRootSignature());
			PipelineState.Compute.bNeedSetRootSignature = false;

			// After setting a root signature, all root parameters are undefined and must be set again.
			bNeedSetSRVs = pRootSignature->HasSRVs();
			bNeedSetConstantBuffers = pRootSignature->HasCBVs();
			bNeedSetSamplers = pRootSignature->HasSamplers();

			bNeedSetSamplersPerShaderStage[SF_Compute] = pRootSignature->MaxSamplerCount(SF_Compute) > 0;
			bNeedSetSRVsPerShaderStage[SF_Compute] = pRootSignature->MaxSRVCount(SF_Compute) > 0;
			bNeedSetConstantBuffersPerShaderStage[SF_Compute] = pRootSignature->MaxCBVCount(SF_Compute) > 0;
			bNeedSetUAVsPerShaderStage[SF_Compute] = pRootSignature->MaxUAVCount(SF_Compute) > 0;
		}
	}
	else
	{
		if (PipelineState.Graphics.bNeedRebuildPSO || bForceState)
		{
			// The desc is mostly initialized, just need to copy the RTV/DSV formats and sample properties in
			FD3D12HighLevelGraphicsPipelineStateDesc& psoDesc = PipelineState.Graphics.HighLevelDesc;

			FMemory::Memzero(psoDesc.RTVFormats, sizeof(psoDesc.RTVFormats));
			psoDesc.SampleDesc.Count = psoDesc.SampleDesc.Quality = 0;
			for (uint32 i = 0; i < psoDesc.NumRenderTargets; i++)
			{
				if (PipelineState.Graphics.RenderTargetArray[i] != NULL)
				{
					const D3D12_RENDER_TARGET_VIEW_DESC &desc = PipelineState.Graphics.RenderTargetArray[i]->GetDesc();
					D3D12_RESOURCE_DESC const& resDesc = PipelineState.Graphics.RenderTargetArray[i]->GetResource()->GetDesc();

					if (desc.Format == DXGI_FORMAT_UNKNOWN)
					{
						psoDesc.RTVFormats[i] = resDesc.Format;
					}
					else
					{
						psoDesc.RTVFormats[i] = desc.Format;
					}
					check(psoDesc.RTVFormats[i] != DXGI_FORMAT_UNKNOWN);

					if (psoDesc.SampleDesc.Count == 0)
					{
						psoDesc.SampleDesc.Count = resDesc.SampleDesc.Count;
						psoDesc.SampleDesc.Quality = resDesc.SampleDesc.Quality;
					}
				}
			}

			psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
			if (PipelineState.Graphics.CurrentDepthStencilTarget)
			{
				const D3D12_DEPTH_STENCIL_VIEW_DESC &dsvDesc = PipelineState.Graphics.CurrentDepthStencilTarget->GetDesc();
				D3D12_RESOURCE_DESC const& resDesc = PipelineState.Graphics.CurrentDepthStencilTarget->GetResource()->GetDesc();

				psoDesc.DSVFormat = dsvDesc.Format;
				if (psoDesc.NumRenderTargets == 0 || psoDesc.SampleDesc.Count == 0)
				{
					psoDesc.SampleDesc.Count = resDesc.SampleDesc.Count;
					psoDesc.SampleDesc.Quality = resDesc.SampleDesc.Quality;
				}
			}

			ID3D12PipelineState* psoGraphics =
				PSOCache.FindGraphics(PipelineState.Graphics.HighLevelDesc);

			// Save the current PSO
			PipelineState.Common.CurrentPipelineStateObject = psoGraphics;
			PipelineState.Graphics.CurrentPipelineStateObject = psoGraphics;

			// Indicate we need to set the PSO on the command list
			PipelineState.Common.bNeedSetPSO = true;
			PipelineState.Graphics.bNeedRebuildPSO = false;
		}

		// See if we need to set a graphics root signature
		if (PipelineState.Graphics.bNeedSetRootSignature)
		{
			CommandList->SetGraphicsRootSignature(pRootSignature->GetRootSignature());
			PipelineState.Graphics.bNeedSetRootSignature = false;

			// After setting a root signature, all root parameters are undefined and must be set again.
			bNeedSetSRVs = pRootSignature->HasSRVs();
			bNeedSetConstantBuffers = pRootSignature->HasCBVs();
			bNeedSetSamplers = pRootSignature->HasSamplers();

			for (uint32 Stage = 0; Stage < SF_Compute; ++Stage)
			{
				bNeedSetSamplersPerShaderStage[Stage] = pRootSignature->MaxSamplerCount(static_cast<EShaderFrequency>(Stage)) > 0;
				bNeedSetSRVsPerShaderStage[Stage] = pRootSignature->MaxSRVCount(static_cast<EShaderFrequency>(Stage)) > 0;
				bNeedSetConstantBuffersPerShaderStage[Stage] = pRootSignature->MaxCBVCount(static_cast<EShaderFrequency>(Stage)) > 0;
				bNeedSetUAVsPerShaderStage[Stage] = pRootSignature->MaxUAVCount(static_cast<EShaderFrequency>(Stage)) > 0;
			}
		}
	}

	// See if we need to set our PSO:
	// In D3D11, you could Set dispatch arguments, then set Draw arguments, then call Draw/Dispatch/Draw/Dispatch without setting arguments again.
	// In D3D12, we need to understand when the app switches between Draw/Dispatch and make sure the correct PSO is set.
	if (PipelineState.Common.bNeedSetPSO || bForceState)
	{
		// Set the PSO, could be a Compute or Graphics PSO
		CommandList->SetPipelineState(PipelineState.Common.CurrentPipelineStateObject);
		PipelineState.Common.bNeedSetPSO = false;
	}

	if (!IsCompute)
	{
		// Setup non-heap bindings
		if (bNeedSetVB || bForceState)
		{
			SCOPE_CYCLE_COUNTER(STAT_D3D12ApplyStateSetVertexBufferTime);
			DescriptorCache.SetVertexBuffers(PipelineState.Graphics.VBCache);
			bNeedSetVB = false;
		}
		if (bNeedSetIB || bForceState)
		{
			if (PipelineState.Graphics.IBCache.CurrentIndexBufferLocation != nullptr)
			{
				DescriptorCache.SetIndexBuffer(PipelineState.Graphics.IBCache);
			}
			bNeedSetIB = false;
		}
		if (bNeedSetSOs || bForceState)
		{
			DescriptorCache.SetStreamOutTargets(PipelineState.Graphics.CurrentStreamOutTargets, PipelineState.Graphics.CurrentNumberOfStreamOutTargets, PipelineState.Graphics.CurrentSOOffsets);
			bNeedSetSOs = false;
		}

		if (bNeedSetViewports || bForceState)
		{
			CommandList->RSSetViewports(PipelineState.Graphics.CurrentNumberOfViewports, PipelineState.Graphics.CurrentViewport);
			bNeedSetViewports = false;
		}
		if (bNeedSetScissorRects || bForceState)
		{
			CommandList->RSSetScissorRects(PipelineState.Graphics.CurrentNumberOfScissorRects, PipelineState.Graphics.CurrentViewportScissorRects);
			bNeedSetScissorRects = false;
		}
		if (bNeedSetPrimitiveTopology || bForceState)
		{
			CommandList->IASetPrimitiveTopology(PipelineState.Graphics.CurrentPrimitiveTopology);
			bNeedSetPrimitiveTopology = false;
		}
		if (bNeedSetBlendFactor || bForceState)
		{
			CommandList->OMSetBlendFactor(PipelineState.Graphics.CurrentBlendFactor);
			bNeedSetBlendFactor = false;
		}
		if (bNeedSetStencilRef || bForceState)
		{
			CommandList->OMSetStencilRef(PipelineState.Graphics.CurrentReferenceStencil);
			bNeedSetStencilRef = false;
		}
		if (bNeedSetRTs || bForceState)
		{
			DescriptorCache.SetRenderTargets(PipelineState.Graphics.RenderTargetArray, PipelineState.Graphics.HighLevelDesc.NumRenderTargets, PipelineState.Graphics.CurrentDepthStencilTarget);
			bNeedSetRTs = false;
		}
	}

	// Reserve space in descriptor heaps
	// Since this can cause heap rollover (which causes old bindings to become invalid), the reserve must be done atomically
	const uint32 StartStage = IsCompute ? SF_Compute : 0;
	const uint32 EndStage = IsCompute ? SF_NumFrequencies : SF_Compute;
	const EShaderFrequency UAVStage = IsCompute ? SF_Compute : SF_Pixel;
	uint32 NumUAVs = 0;
	uint32 NumSRVs[SF_NumFrequencies + 1] = {};
	uint32 NumCBs[SF_NumFrequencies + 1] = {};


	uint32 ViewHeapSlot = 0;

	if (bNeedSetSamplers || bForceState)
	{
		ApplySamplers(pRootSignature, StartStage, EndStage);
	}

	for (uint32 iTries = 0; iTries < 2; ++iTries)
	{
		if (bNeedSetUAVsPerShaderStage[UAVStage] || bForceState)
		{
			if (ResourceBindingTier <= D3D12_RESOURCE_BINDING_TIER_2)
			{
				// Tier 1 and 2 HW requires the full number of UAV descriptors defined in the root signature.
				NumUAVs = pRootSignature->MaxUAVCount(UAVStage);
			}
			else
			{
				NumUAVs = PipelineState.Common.CurrentShaderUAVCounts[UAVStage];
			}
		}
		for (uint32 Stage = StartStage; Stage < EndStage; ++Stage)
		{
			if ((bNeedSetSRVs && bNeedSetSRVsPerShaderStage[Stage]) || bForceState)
			{
				if (ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_1)
				{
					// Tier 1 HW requires the full number of SRV descriptors defined in the root signature.
					NumSRVs[Stage] = pRootSignature->MaxSRVCount(Stage);
				}
				else
				{
					NumSRVs[Stage] = PipelineState.Common.CurrentShaderSRVCounts[Stage];
				}
			}
			if ((bNeedSetConstantBuffers && (bNeedSetConstantBuffersPerShaderStage[Stage])) || bForceState)
			{
				if (ResourceBindingTier <= D3D12_RESOURCE_BINDING_TIER_2)
				{
					// Tier 1 and 2 HW requires the full number of CB descriptors defined in the root signature.
					NumCBs[Stage] = pRootSignature->MaxCBVCount(Stage);
				}
				else
				{
					NumCBs[Stage] = PipelineState.Common.CurrentShaderCBCounts[Stage];
				}
			}
			NumSRVs[SF_NumFrequencies] += NumSRVs[Stage];
			NumCBs[SF_NumFrequencies] += NumCBs[Stage];
		}

		const uint32 NumViews = NumUAVs + NumSRVs[SF_NumFrequencies] + NumCBs[SF_NumFrequencies];
		if (!DescriptorCache.GetCurrentViewHeap()->CanReserveSlots(NumViews))
		{
			DescriptorCache.GetCurrentViewHeap()->RollOver();
			NumSRVs[SF_NumFrequencies] = 0;
			NumCBs[SF_NumFrequencies] = 0;
			continue;
		}
		ViewHeapSlot = DescriptorCache.GetCurrentViewHeap()->ReserveSlots(NumViews);
		break;
	}

	// Unordered access views
	if ((bNeedSetUAVsPerShaderStage[UAVStage] || bForceState) && NumUAVs > 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_D3D12ApplyStateSetUAVTime);
		check(pRootSignature->HasUAVs());
		DescriptorCache.SetUAVs(UAVStage, PipelineState.Common.CurrentUAVStartSlot[UAVStage], PipelineState.Common.UnorderedAccessViewArray[UAVStage], NumUAVs, ViewHeapSlot);
		bNeedSetUAVsPerShaderStage[UAVStage] = false;
	}

	// Shader resource views
	if (bNeedSetSRVs || bForceState)
	{
		SCOPE_CYCLE_COUNTER(STAT_D3D12ApplyStateSetSRVTime);
		for (uint32 i = StartStage; i < EndStage; i++)
		{
			if ((NumSRVs[i] > 0) || bForceState)
			{
				DescriptorCache.SetSRVs((EShaderFrequency)i, PipelineState.Common.SRVCache, NumSRVs[i], ViewHeapSlot);
				bNeedSetSRVsPerShaderStage[i] = false;
			}
		}
		bNeedSetSRVs = false;
	}

	// Constant buffers
	if (bNeedSetConstantBuffers || bForceState)
	{
		SCOPE_CYCLE_COUNTER(STAT_D3D12ApplyStateSetConstantBufferTime);
		for (uint32 i = StartStage; i < EndStage; i++)
		{
			if (bNeedSetConstantBuffersPerShaderStage[i] || bForceState)
			{
				DescriptorCache.SetConstantBuffers((EShaderFrequency)i, NumCBs[i], ViewHeapSlot);
				for (uint32 y = 0; y < NumCBs[i]; y++)
				{
					if (PipelineState.Common.CurrentConstantBuffers[(EShaderFrequency)i][y])
					{
						CommandList.UpdateResidency(PipelineState.Common.CurrentConstantBuffers[(EShaderFrequency)i][y]);
					}
				}
				bNeedSetConstantBuffersPerShaderStage[i] = false;
			}
		}
		bNeedSetConstantBuffers = false;
	}

	// Flush any needed resource barriers
	CommandList.FlushResourceBarriers();

#if ASSERT_RESOURCE_STATES
	bool bSucceeded = AssertResourceStates(IsCompute);
	check(bSucceeded);
#endif
}

void FD3D12StateCacheBase::ApplySamplers(const FD3D12RootSignature* const pRootSignature, uint32 StartStage, uint32 EndStage)
{
	const bool bForceState = false;
	bool HighLevelCacheMiss = false;

	uint32 NumSamplers[SF_NumFrequencies + 1] = {};
	uint32 SamplerHeapSlot = 0;

	auto pfnCalcSamplersNeeded = [&]()
	{
		NumSamplers[SF_NumFrequencies] = 0;

		for (uint32 Stage = StartStage; Stage < EndStage; ++Stage)
		{
			if (bNeedSetSamplersPerShaderStage[Stage] || bForceState)
			{
				if (ResourceBindingTier == D3D12_RESOURCE_BINDING_TIER_1)
				{
					// Tier 1 HW requires the full number of sampler descriptors defined in the root signature.
					NumSamplers[Stage] = pRootSignature->MaxSamplerCount(Stage);
				}
				else
				{
					NumSamplers[Stage] = PipelineState.Common.CurrentShaderSamplerCounts[Stage];
				}
			}
			NumSamplers[SF_NumFrequencies] += NumSamplers[Stage];
		}
	};

	pfnCalcSamplersNeeded();

	if (DescriptorCache.UsingGlobalSamplerHeap())
	{
		auto& GlobalSamplerSet = DescriptorCache.GetLocalSamplerSet();

		FD3D12CommandListHandle& CommandList = CmdContext->CommandListHandle;

		for (uint32 Stage = StartStage; Stage < EndStage; Stage++)
		{
			if ((bNeedSetSamplersPerShaderStage[Stage] && NumSamplers[Stage]) || bForceState)
			{
				FD3D12SamplerState** Samplers = PipelineState.Common.CurrentSamplerStates[Stage];

				FD3D12UniqueSamplerTable Table;
				Table.Key.Count = NumSamplers[Stage];

				for (uint32 i = 0; i < NumSamplers[Stage]; i++)
				{
					Table.Key.SamplerID[i] = Samplers[i] ? Samplers[i]->ID : 0;
				}

				FD3D12UniqueSamplerTable* CachedTable = GlobalSamplerSet.Find(Table);
				if (CachedTable)
				{
					check(CmdContext->DescriptorHeaps[1] == GetParentDevice()->GetGlobalSamplerHeap().GetHeap());
					check(CachedTable->GPUHandle.ptr);
					if (Stage == SF_Compute)
					{
						const uint32 RDTIndex = CmdContext->StateCache.GetComputeRootSignature()->SamplerRDTBindSlot(EShaderFrequency(Stage));
						CommandList->SetComputeRootDescriptorTable(RDTIndex, CachedTable->GPUHandle);
					}
					else
					{
						const uint32 RDTIndex = CmdContext->StateCache.GetGraphicsRootSignature()->SamplerRDTBindSlot(EShaderFrequency(Stage));
						CommandList->SetGraphicsRootDescriptorTable(RDTIndex, CachedTable->GPUHandle);
					}
				}
				else
				{
					HighLevelCacheMiss = true;
					break;
				}
			}
		}

		if (!HighLevelCacheMiss)
		{
			// Success, all the tables were found in the high level heap
			bNeedSetSamplers = false;
			return;
		}
	}

	if (HighLevelCacheMiss)
	{
		//Move to per context heap strategy
		DescriptorCache.SwitchToContextLocalSamplerHeap();

		DirtySamplerDescriptorTables();

		pfnCalcSamplersNeeded();
	}

	FD3D12OnlineHeap* SamplerHeap = DescriptorCache.GetCurrentSamplerHeap();
	check(DescriptorCache.UsingGlobalSamplerHeap() == false);
	check(SamplerHeap != &GetParentDevice()->GetGlobalSamplerHeap());
	check(SamplerHeap->GetHeap() == CmdContext->DescriptorHeaps[1]);

	if (!SamplerHeap->CanReserveSlots(NumSamplers[SF_NumFrequencies]))
	{
		SamplerHeap->RollOver();
		NumSamplers[SF_NumFrequencies] = 0;

		//Redo the calculation now that the heap has changed as we may have to reset some tables.
		pfnCalcSamplersNeeded();
	}
	SamplerHeapSlot = SamplerHeap->ReserveSlots(NumSamplers[SF_NumFrequencies]);

	for (uint32 i = StartStage; i < EndStage; i++)
	{
		if ((bNeedSetSamplersPerShaderStage[i] && NumSamplers[i]) || bForceState)
		{
			DescriptorCache.SetSamplers((EShaderFrequency)i, PipelineState.Common.CurrentSamplerStates[i], NumSamplers[i], SamplerHeapSlot);
			bNeedSetSamplersPerShaderStage[i] = false;
		}
	}
	bNeedSetSamplers = false;

	SamplerHeap->SetNextSlot(SamplerHeapSlot);
}

bool FD3D12StateCacheBase::AssertResourceStates(const bool IsCompute)
{
	// Can only verify resource states if the debug layer is used
	static const bool bWithD3DDebug = D3D12RHI_ShouldCreateWithD3DDebug();
	if (!bWithD3DDebug)
	{
		UE_LOG(LogD3D12RHI, Fatal, TEXT("*** AssertResourceStates requires the debug layer ***"));
		return false;
	}

	// Get the debug command queue
	ID3D12CommandList* pCommandList = CmdContext->CommandListHandle.CommandList();
	TRefCountPtr<ID3D12DebugCommandList> pDebugCommandList;
	VERIFYD3D12RESULT(pCommandList->QueryInterface(pDebugCommandList.GetInitReference()));

	// Note: There is nothing special to check when IsCompute = true
	if (!IsCompute)
	{
		//
		// Verify graphics pipeline state
		//

		// DSV
		{
			FD3D12DepthStencilView* pCurrentView = PipelineState.Graphics.CurrentDepthStencilTarget;

			if (pCurrentView)
			{
				// Check if the depth/stencil resource has an SRV bound
				const bool bSRVBound = PipelineState.Common.SRVCache.NumViewsIntersectWithDepthCount > 0;
				uint32 SanityCheckCount = 0;
				const uint32 StartStage = 0;
				const uint32 EndStage = SF_Compute;
				for (uint32 Stage = StartStage; Stage < EndStage; Stage++)
				{
					for (uint32 i = 0; i < MAX_SRVS; i++)
					{
						if (PipelineState.Common.SRVCache.ViewsIntersectWithDepthRT[Stage][i])
						{
							SanityCheckCount++;
						}
					}
				}
				check(SanityCheckCount == PipelineState.Common.SRVCache.NumViewsIntersectWithDepthCount);

				const D3D12_DEPTH_STENCIL_VIEW_DESC& desc = pCurrentView->GetDesc();
				const bool bDepthIsReadOnly = !!(desc.Flags & D3D12_DSV_FLAG_READ_ONLY_DEPTH);
				const bool bStencilIsReadOnly = !!(desc.Flags & D3D12_DSV_FLAG_READ_ONLY_STENCIL);

				// Decompose the view into the subresources (depth and stencil are on different planes)
				FD3D12Resource* pResource = pCurrentView->GetResource();
				const CViewSubresourceSubset subresourceSubset = pCurrentView->GetViewSubresourceSubset();
				for (CViewSubresourceSubset::CViewSubresourceIterator it = subresourceSubset.begin(); it != subresourceSubset.end(); ++it)
				{
					for (uint32 SubresourceIndex = it.StartSubresource(); SubresourceIndex < it.EndSubresource(); SubresourceIndex++)
					{
						uint16 MipSlice;
						uint16 ArraySlice;
						uint8 PlaneSlice;
						D3D12DecomposeSubresource(SubresourceIndex,
							pResource->GetMipLevels(),
							pResource->GetArraySize(),
							MipSlice, ArraySlice, PlaneSlice);

						D3D12_RESOURCE_STATES expectedState;
						if (PlaneSlice == 0)
						{
							// Depth plane
							expectedState = bDepthIsReadOnly ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;
							if (bSRVBound)
							{
								// Depth SRVs just contain the depth plane
								check(bDepthIsReadOnly);
								expectedState |=
									D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
									D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
							}
						}
						else
						{
							// Stencil plane
							expectedState = bStencilIsReadOnly ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;
						}

						bool bGoodState = !!pDebugCommandList->AssertResourceState(pResource->GetResource(), SubresourceIndex, expectedState);
						if (!bGoodState)
						{
							return false;
						}
					}
				}

			}
		}

		// RTV
		{
			const uint32 numRTVs = _countof(PipelineState.Graphics.RenderTargetArray);
			for (uint32 i = 0; i < numRTVs; i++)
			{
				FD3D12RenderTargetView* pCurrentView = PipelineState.Graphics.RenderTargetArray[i];
				if (!AssertResourceState(pCommandList, pCurrentView, D3D12_RESOURCE_STATE_RENDER_TARGET))
				{
					return false;
				}
			}
		}

		// TODO: Verify vertex buffer, index buffer, and constant buffer state.
	}

	//
	// Verify common pipeline state
	//

	const uint32 StartStage = IsCompute ? SF_Compute : 0;
	const uint32 EndStage = IsCompute ? SF_NumFrequencies : SF_Compute;
	for (uint32 Stage = StartStage; Stage < EndStage; Stage++)
	{
		// UAVs
		{
			const uint32 numUAVs = PipelineState.Common.CurrentShaderUAVCounts[Stage];
			for (uint32 i = 0; i < numUAVs; i++)
			{
				FD3D12UnorderedAccessView *pCurrentView = PipelineState.Common.UnorderedAccessViewArray[Stage][i];
				if (!AssertResourceState(pCommandList, pCurrentView, D3D12_RESOURCE_STATE_UNORDERED_ACCESS))
				{
					return false;
				}
			}
		}

		// SRVs
		{
			const uint32 numSRVs = PipelineState.Common.CurrentShaderSRVCounts[Stage];
			for (uint32 i = 0; i < numSRVs; i++)
			{
				FD3D12ShaderResourceView* pCurrentView = PipelineState.Common.SRVCache.Views[Stage][i];
				if (!AssertResourceState(pCommandList, pCurrentView, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))
				{
					return false;
				}
			}
		}
	}

	return true;
}

void FD3D12StateCacheBase::SetUAVs(EShaderFrequency ShaderStage, uint32 UAVStartSlot, uint32 NumSimultaneousUAVs, FD3D12UnorderedAccessView** UAVArray, uint32* UAVInitialCountArray)
{
	if (NumSimultaneousUAVs == 0)
	{
		return;
	}

	// When setting UAV's for Graphics, it wipes out all existing bound resources.
	const bool bIsCompute = ShaderStage == SF_Compute;
	PipelineState.Common.CurrentUAVStartSlot[ShaderStage] = bIsCompute ? FMath::Min(UAVStartSlot, PipelineState.Common.CurrentUAVStartSlot[ShaderStage]) : UAVStartSlot;

	ID3D12Resource*              CounterUploadHeap      = GetParentDevice()->GetCounterUploadHeap();
	uint32&                      CounterUploadHeapIndex = GetParentDevice()->GetCounterUploadHeapIndex();
	void*                        CounterUploadHeapData  = GetParentDevice()->GetCounterUploadHeapData();

	for (uint32 i = 0; i < NumSimultaneousUAVs; ++i)
	{
		PipelineState.Common.UnorderedAccessViewArray[ShaderStage][UAVStartSlot + i] = UAVArray[i];

		if (UAVArray[i] && UAVArray[i]->CounterResource && (!UAVArray[i]->CounterResourceInitialized || UAVInitialCountArray[i] != -1))
		{
			// Ensure we don't run off the end of the upload heap.
			CounterUploadHeapIndex = (CounterUploadHeapIndex % (COUNTER_HEAP_SIZE / 4));

			// Initialize the counter to 0 if it's not been previously initialized and the UAVInitialCount is -1, if not use the value that was passed.
			((uint32*)CounterUploadHeapData)[CounterUploadHeapIndex] = (!UAVArray[i]->CounterResourceInitialized && UAVInitialCountArray[i] == -1) ? 0 : UAVInitialCountArray[i];

			CmdContext->CommandListHandle->CopyBufferRegion(
				UAVArray[i]->CounterResource->GetResource(),
				0,
				CounterUploadHeap,
				CounterUploadHeapIndex * 4,
				4);

			CmdContext->CommandListHandle.UpdateResidency(UAVArray[i]->CounterResource);

			CounterUploadHeapIndex++;
			UAVArray[i]->CounterResourceInitialized = true;
		}
	}

	bNeedSetUAVsPerShaderStage[ShaderStage] = true;
}

void FD3D12StateCacheBase::SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
	if ((PipelineState.Graphics.CurrentPrimitiveTopology != PrimitiveTopology) || GD3D12SkipStateCaching)
	{
		PipelineState.Graphics.CurrentPrimitiveTopology = PrimitiveTopology;
		switch (PipelineState.Graphics.CurrentPrimitiveTopology)
		{
		case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
			PipelineState.Graphics.HighLevelDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
		case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
		case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
		case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
			PipelineState.Graphics.HighLevelDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
			break;
		case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
		case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
		case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
		case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
			PipelineState.Graphics.HighLevelDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			break;
		default:
			PipelineState.Graphics.HighLevelDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
			break;
		}
		bNeedSetPrimitiveTopology = true;
		PipelineState.Graphics.bNeedRebuildPSO = true;
	}
}

void FD3D12StateCacheBase::SetBlendState(D3D12_BLEND_DESC* State, const float BlendFactor[4], uint32 SampleMask)
{
	if (PipelineState.Graphics.HighLevelDesc.BlendState != State || PipelineState.Graphics.HighLevelDesc.SampleMask != SampleMask || GD3D12SkipStateCaching)
	{
		PipelineState.Graphics.HighLevelDesc.BlendState = State;
		PipelineState.Graphics.HighLevelDesc.SampleMask = SampleMask;
		PipelineState.Graphics.bNeedRebuildPSO = true;
	}

	if (FMemory::Memcmp(PipelineState.Graphics.CurrentBlendFactor, BlendFactor, sizeof(PipelineState.Graphics.CurrentBlendFactor)))
	{
		FMemory::Memcpy(PipelineState.Graphics.CurrentBlendFactor, BlendFactor, sizeof(PipelineState.Graphics.CurrentBlendFactor));
		bNeedSetBlendFactor = true;
	}
}

void FD3D12StateCacheBase::SetDepthStencilState(D3D12_DEPTH_STENCIL_DESC* State, uint32 RefStencil)
{
	if (PipelineState.Graphics.HighLevelDesc.DepthStencilState != State || GD3D12SkipStateCaching)
	{
		PipelineState.Graphics.HighLevelDesc.DepthStencilState = State;
		PipelineState.Graphics.bNeedRebuildPSO = true;
	}

	if (PipelineState.Graphics.CurrentReferenceStencil != RefStencil)
	{
		PipelineState.Graphics.CurrentReferenceStencil = RefStencil;
		bNeedSetStencilRef = true;
	}
}

void FD3D12StateCacheBase::InternalSetIndexBuffer(FD3D12ResourceLocation* IndexBufferLocation, DXGI_FORMAT Format, uint32 Offset)
{
	__declspec(align(16)) D3D12_INDEX_BUFFER_VIEW NewView;
	NewView.BufferLocation = (IndexBufferLocation) ? IndexBufferLocation->GetGPUVirtualAddress() + Offset : 0;
	NewView.SizeInBytes = (IndexBufferLocation) ? IndexBufferLocation->GetEffectiveBufferSize() - Offset : 0;
	NewView.Format = Format;

	D3D12_INDEX_BUFFER_VIEW& CurrentView = PipelineState.Graphics.IBCache.CurrentIndexBufferView;

	if (bAlwaysSetIndexBuffers ||
		NewView.BufferLocation != CurrentView.BufferLocation ||
		NewView.SizeInBytes != CurrentView.SizeInBytes ||
		NewView.Format != CurrentView.Format ||
		GD3D12SkipStateCaching)
	{
		bNeedSetIB = true;
		PipelineState.Graphics.IBCache.CurrentIndexBufferLocation = IndexBufferLocation;

		if (IndexBufferLocation != nullptr)
		{
			PipelineState.Graphics.IBCache.ResidencyHandle = IndexBufferLocation->GetResource()->GetResidencyHandle();
			FMemory::Memcpy(PipelineState.Graphics.IBCache.CurrentIndexBufferView, NewView);
		}
		else
		{
			FMemory::Memzero(&CurrentView, sizeof(CurrentView));
			PipelineState.Graphics.IBCache.CurrentIndexBufferLocation = nullptr;
			PipelineState.Graphics.IBCache.ResidencyHandle = nullptr;
		}
	}
}

void FD3D12StateCacheBase::InternalSetStreamSource(FD3D12ResourceLocation* VertexBufferLocation, uint32 StreamIndex, uint32 Stride, uint32 Offset)
{
	check(StreamIndex < ARRAYSIZE(PipelineState.Graphics.VBCache.CurrentVertexBufferResources));

	__declspec(align(16)) D3D12_VERTEX_BUFFER_VIEW NewView;
	NewView.BufferLocation = (VertexBufferLocation) ? VertexBufferLocation->GetGPUVirtualAddress() + Offset : 0;
	NewView.StrideInBytes = Stride;
	NewView.SizeInBytes = (VertexBufferLocation) ? VertexBufferLocation->GetEffectiveBufferSize() - Offset : 0; // Make sure we account for how much we offset into the VB

	D3D12_VERTEX_BUFFER_VIEW& CurrentView = PipelineState.Graphics.VBCache.CurrentVertexBufferViews[StreamIndex];

	if (NewView.BufferLocation != CurrentView.BufferLocation ||
		NewView.StrideInBytes != CurrentView.StrideInBytes ||
		NewView.SizeInBytes != CurrentView.SizeInBytes ||
		GD3D12SkipStateCaching)
	{
		bNeedSetVB = true;
		PipelineState.Graphics.VBCache.CurrentVertexBufferResources[StreamIndex] = VertexBufferLocation;

		if (VertexBufferLocation != nullptr)
		{
			PipelineState.Graphics.VBCache.ResidencyHandles[StreamIndex] = VertexBufferLocation->GetResource()->GetResidencyHandle();
			FMemory::Memcpy(CurrentView, NewView);
			PipelineState.Graphics.VBCache.BoundVBMask |= (1 << StreamIndex);
		}
		else
		{
			FMemory::Memzero(&CurrentView, sizeof(CurrentView));
			PipelineState.Graphics.VBCache.CurrentVertexBufferResources[StreamIndex] = nullptr;
			PipelineState.Graphics.VBCache.ResidencyHandles[StreamIndex] = nullptr;

			PipelineState.Graphics.VBCache.BoundVBMask &= ~(1 << StreamIndex);
		}

		if (PipelineState.Graphics.VBCache.BoundVBMask)
		{
			PipelineState.Graphics.VBCache.MaxBoundVertexBufferIndex = FMath::FloorLog2(PipelineState.Graphics.VBCache.BoundVBMask);
		}
		else
		{
			PipelineState.Graphics.VBCache.MaxBoundVertexBufferIndex = INDEX_NONE;
		}
	}
}

void FD3D12StateCacheBase::InternalSetShaderResourceView(FD3D12ShaderResourceView*& SRV, uint32 ResourceIndex, EShaderFrequency ShaderFrequency)
{
	check(ResourceIndex < MAX_SRVS);
	FD3D12ShaderResourceViewCache& Cache = PipelineState.Common.SRVCache;
	auto& CurrentShaderResourceViews = Cache.Views[ShaderFrequency];

	if ((CurrentShaderResourceViews[ResourceIndex] != SRV) || GD3D12SkipStateCaching)
	{
		if (SRV != nullptr)
		{
			// Mark the SRVs as not cleared
			bSRVSCleared = false;

			Cache.BoundMask[ShaderFrequency] |= (1 << ResourceIndex);
			Cache.ResidencyHandles[ShaderFrequency][ResourceIndex] = SRV->GetResidencyHandle();
		}
		else
		{
			Cache.BoundMask[ShaderFrequency] &= ~(1 << ResourceIndex);
			Cache.ResidencyHandles[ShaderFrequency][ResourceIndex] = nullptr;
		}

		// Find the highest set SRV
		(Cache.BoundMask[ShaderFrequency] == 0) ? Cache.MaxBoundIndex[ShaderFrequency] = INDEX_NONE :
			Cache.MaxBoundIndex[ShaderFrequency] = FMath::FloorLog2(Cache.BoundMask[ShaderFrequency]);

		CurrentShaderResourceViews[ResourceIndex] = SRV;
		bNeedSetSRVsPerShaderStage[ShaderFrequency] = true;
		bNeedSetSRVs = true;

		if (SRV && SRV->IsDepthStencilResource())
		{
			if (FD3D12DynamicRHI::ResourceViewsIntersect(PipelineState.Graphics.CurrentDepthStencilTarget, SRV))
			{
				const D3D12_DEPTH_STENCIL_VIEW_DESC &DSVDesc = PipelineState.Graphics.CurrentDepthStencilTarget->GetDesc();
				const bool bHasDepth = PipelineState.Graphics.CurrentDepthStencilTarget->HasDepth();
				const bool bHasStencil = PipelineState.Graphics.CurrentDepthStencilTarget->HasStencil();
				const bool bWritableDepth = bHasDepth && (DSVDesc.Flags & D3D12_DSV_FLAG_READ_ONLY_DEPTH) == 0;
				const bool bWritableStencil = bHasStencil && (DSVDesc.Flags & D3D12_DSV_FLAG_READ_ONLY_STENCIL) == 0;
				const bool bUnbindDepthStencil = (bWritableDepth && SRV->IsDepthPlaneResource()) || (bWritableStencil && SRV->IsStencilPlaneResource());
				if (!bUnbindDepthStencil)
				{
					// If the DSV isn't writing to the same subresource as the SRV then we can leave the depth stencil bound.
					if (!Cache.ViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex])
					{
						Cache.ViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex] = true;
						Cache.NumViewsIntersectWithDepthCount++;
					}
				}
				else
				{
					// Unbind the DSV because it's being used for depth write
					check(bWritableDepth || bWritableStencil);
					PipelineState.Graphics.CurrentDepthStencilTarget = nullptr;
					if (Cache.ViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex])
					{
						Cache.ViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex] = false;
						Cache.NumViewsIntersectWithDepthCount--;
					}
				}
			}
			else
			{
				if (Cache.ViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex])
				{
					Cache.ViewsIntersectWithDepthRT[ShaderFrequency][ResourceIndex] = false;
					Cache.NumViewsIntersectWithDepthCount--;
				}
			}
		}
	}
}

void FD3D12StateCacheBase::SetRenderTargets(uint32 NumSimultaneousRenderTargets, FD3D12RenderTargetView** RTArray, FD3D12DepthStencilView* DSTarget)
{
	// Note: We assume that the have been checks to make sure this function is only called when there really are changes being made.
	// We always update the PSO and set descriptors after calling this function.
	PipelineState.Graphics.bNeedRebuildPSO = true;
	bNeedSetRTs = true;

	// Update the depth stencil
	PipelineState.Graphics.CurrentDepthStencilTarget = DSTarget;

	// Update the render targets
	FMemory::Memzero(PipelineState.Graphics.RenderTargetArray, sizeof(PipelineState.Graphics.RenderTargetArray));
	FMemory::Memcpy(PipelineState.Graphics.RenderTargetArray, RTArray, sizeof(FD3D12RenderTargetView*)* NumSimultaneousRenderTargets);

	// In D3D11, the NumSimultaneousRenderTargets count was used even when setting RTV slots to null (to unbind them)
	// In D3D12, we don't do this. So we need change the count to match the non null views used.
	uint32 ActiveNumSimultaneousRenderTargets = 0;
	for (uint32 i = 0; i < NumSimultaneousRenderTargets; i++)
	{
		if (RTArray[i] != nullptr)
		{
			ActiveNumSimultaneousRenderTargets = i + 1;
		}
	}
	PipelineState.Graphics.HighLevelDesc.NumRenderTargets = ActiveNumSimultaneousRenderTargets;
}

void FD3D12StateCacheBase::SetStreamOutTargets(uint32 NumSimultaneousStreamOutTargets, FD3D12Resource** SOArray, const uint32* SOOffsets)
{
	PipelineState.Graphics.CurrentNumberOfStreamOutTargets = NumSimultaneousStreamOutTargets;
	if (PipelineState.Graphics.CurrentNumberOfStreamOutTargets > 0)
	{
		FMemory::Memcpy(PipelineState.Graphics.CurrentStreamOutTargets, SOArray, sizeof(FD3D12Resource*)* NumSimultaneousStreamOutTargets);
		FMemory::Memcpy(PipelineState.Graphics.CurrentSOOffsets, SOOffsets, sizeof(uint32*)* NumSimultaneousStreamOutTargets);

		bNeedSetSOs = true;
	}
}