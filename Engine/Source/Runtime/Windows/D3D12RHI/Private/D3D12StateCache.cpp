// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// Implementation of Device Context State Caching to improve draw
//	thread performance by removing redundant device context calls.

//-----------------------------------------------------------------------------
//	Include Files
//-----------------------------------------------------------------------------
#include "D3D12RHIPrivate.h"
#include "D3D12StateCache.h"
#include <emmintrin.h>

#define IL_MAX_SEMANTIC_NAME 255

static int32 GEnablePSOCache = 1;
static FAutoConsoleVariableRef CVarEnablePSOCache(
	TEXT("D3D12.EnablePSOCache"),
	GEnablePSOCache,
	TEXT("Enables a disk cache for PipelineState Objects."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
	);

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

void* FD3D12FastAllocator::Allocate(uint32 size, uint32 alignment, class FD3D12ResourceLocation* ResourceLocation)
{
	return AllocateInternal(size, alignment, ResourceLocation);
}

void FD3D12FastAllocator::Destroy()
{
	if (CurrentAllocatorPage)
	{
		PagePool->ReturnFastAllocatorPage(CurrentAllocatorPage);
		CurrentAllocatorPage = nullptr;
	}
}

void* FD3D12FastAllocator::AllocateInternal(uint32 size, uint32 alignment, class FD3D12ResourceLocation* ResourceLocation)
{
	if (size > PagePool->GetPageSize())
	{
		ResourceLocation->Clear();

		//Allocations are 64k aligned
		if(alignment)
			alignment = (D3D_BUFFER_ALIGNMENT % alignment) == 0 ? 0 : alignment;

		VERIFYD3D11RESULT(GetParentDevice()->GetResourceHelper().CreateBuffer(PagePool->GetHeapType(), size + alignment, ResourceLocation->Resource.GetInitReference()));
		SetName(ResourceLocation->Resource, L"Stand Alone Fast Allocation");

		void* Data = nullptr;
		if (PagePool->IsCPUWritable())
		{
			VERIFYD3D11RESULT(ResourceLocation->Resource->GetResource()->Map(0, nullptr, &Data));
		}

		ResourceLocation->EffectiveBufferSize = size;
		ResourceLocation->Offset = alignment;

		return Data;
	}
	else
	{
		void* Data = nullptr;

		const uint32 Offset = (CurrentAllocatorPage) ? CurrentAllocatorPage->NextFastAllocOffset : 0;
		uint32 CurrentOffset = Align(Offset, alignment);

		// See if there is room in the current pool
		if (CurrentAllocatorPage == nullptr || PagePool->GetPageSize() < CurrentOffset + size)
		{
			if (CurrentAllocatorPage)
			{
				PagePool->ReturnFastAllocatorPage(CurrentAllocatorPage);
			}
			CurrentAllocatorPage = PagePool->RequestFastAllocatorPage();

			CurrentOffset = Align(CurrentAllocatorPage->NextFastAllocOffset, alignment);
		}

		check(PagePool->GetPageSize() - size >= CurrentOffset);

		// Create a FD3D12ResourceLocation representing a sub-section of the pool resource
		ResourceLocation->SetFromD3DResource(CurrentAllocatorPage->FastAllocBuffer.GetReference(), CurrentOffset, size);
		ResourceLocation->SetAsFastAllocatedSubresource();

		CurrentAllocatorPage->NextFastAllocOffset = CurrentOffset + size;

		if (PagePool->IsCPUWritable())
		{
			Data = (void*)((uint8*)CurrentAllocatorPage->FastAllocData + CurrentOffset);
		}

		check(Data);
		return Data;
	}
}

void* FD3D12ThreadSafeFastAllocator::Allocate(uint32 size, uint32 alignment, class FD3D12ResourceLocation* ResourceLocation)
{
	FScopeLock Lock(&CS);
	return AllocateInternal(size, alignment, ResourceLocation);
}

HRESULT FD3D12ResourceHelper::CreateCommittedResource(const D3D12_RESOURCE_DESC& Desc, const D3D12_HEAP_PROPERTIES& HeapProps, const D3D12_CLEAR_VALUE* ClearValue, FD3D12Resource** ppResource)
{
	return CreateCommittedResource(Desc, HeapProps, D3D12_RESOURCE_STATE_COMMON, ClearValue, ppResource);
}

HRESULT FD3D12ResourceHelper::CreateCommittedResource(const D3D12_RESOURCE_DESC& Desc, const D3D12_HEAP_PROPERTIES& HeapProps, const D3D12_RESOURCE_STATES& InitialUsage, const D3D12_CLEAR_VALUE* ClearValue, FD3D12Resource** ppResource)
{
	return FD3D12DynamicRHI::CreateCommittedResource(Desc, HeapProps, InitialUsage, ClearValue, ppResource);
}

HRESULT FD3D12ResourceHelper::CreateDefaultResource(const D3D12_RESOURCE_DESC& Desc, const D3D12_CLEAR_VALUE* ClearValue, FD3D12Resource** ppResource)
{
	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
	return FD3D12DynamicRHI::CreateCommittedResource(Desc, heapProperties, D3D12_RESOURCE_STATE_COMMON, ClearValue, ppResource);
}

HRESULT FD3D12ResourceHelper::CreateBuffer(D3D12_HEAP_TYPE heapType, uint64 initHeapSize, FD3D12Resource** ppResource, D3D12_RESOURCE_FLAGS flags, const D3D12_HEAP_PROPERTIES *pCustomHeapProperties)
{
	TRefCountPtr<ID3D12Resource> pResource;
	ID3D12Device* pD3DDevice = GetParentDevice()->GetDevice();

	D3D12_RESOURCE_DESC BufDesc = CD3DX12_RESOURCE_DESC::Buffer(initHeapSize);
	BufDesc.Flags = flags;

	D3D12_RESOURCE_STATES InitialState = DetermineInitialResourceState(heapType, pCustomHeapProperties);

	if (!ppResource)
	{
		return E_POINTER;
	}

	check(pCustomHeapProperties != nullptr ? pCustomHeapProperties->Type == heapType: true);
	HRESULT hr =  FD3D12DynamicRHI::CreateCommittedResource(
		BufDesc, 
		(pCustomHeapProperties != nullptr) ? *pCustomHeapProperties : CD3DX12_HEAP_PROPERTIES(heapType),
		InitialState, 
		nullptr, 
		ppResource);

	return hr;
}

HRESULT FD3D12ResourceHelper::CreatePlacedBuffer(ID3D12Heap* BackingHeap, uint64 HeapOffset,D3D12_HEAP_TYPE HeapType, uint64 BufferSize, FD3D12Resource** ppOutResource, D3D12_RESOURCE_FLAGS flags)
{
	TRefCountPtr<ID3D12Resource> pResource;
	ID3D12Device* pD3DDevice = GetParentDevice()->GetDevice();

	D3D12_RESOURCE_DESC BufDesc = CD3DX12_RESOURCE_DESC::Buffer(BufferSize);
	BufDesc.Flags = flags;

	D3D12_RESOURCE_STATES InitialState = DetermineInitialResourceState(HeapType);

	if (!ppOutResource)
	{
		return E_POINTER;
	}

	HRESULT hr = FD3D12DynamicRHI::CreatePlacedResource(BufDesc, BackingHeap, HeapOffset, InitialState, nullptr, ppOutResource);

	return hr;
}


FD3D12ResourceHelper::FD3D12ResourceHelper(FD3D12Device* InParent) :
	FD3D12DeviceChild(InParent)
{}

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

	ZeroMemory(PipelineState.Common.CurrentShaderResourceViewsIntersectWithDepthRT, sizeof(PipelineState.Common.CurrentShaderResourceViewsIntersectWithDepthRT));
	PipelineState.Common.ShaderResourceViewsIntersectWithDepthCount = 0;
	for (uint32 ShaderFrequency = 0; ShaderFrequency < SF_NumFrequencies; ShaderFrequency++)
	{
		int32& MaxResourceIndex = PipelineState.Common.MaxBoundShaderResourcesIndex[ShaderFrequency];
		for (int32 resIndex = 0; resIndex <= MaxResourceIndex; ++resIndex)
		{
			PipelineState.Common.CurrentShaderResourceViews[ShaderFrequency][resIndex] = nullptr;
		}

		MaxResourceIndex = INDEX_NONE;
	}

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
		D3D12_RESOURCE_BARRIER desc = {};
		desc.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		desc.UAV.pResource = nullptr;

		FD3D12CommandListHandle& CommandList = CmdContext->CommandListHandle;

		CmdContext->numBarriers++;
		CommandList->ResourceBarrier(1, &desc);
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
		PipelineState.Graphics.CurrentVertexBuffers[i].VertexBufferLocation = nullptr;
	}
	PipelineState.Graphics.MaxBoundVertexBufferIndex = INDEX_NONE;

	FMemory::Memzero(PipelineState.Common.CurrentSamplerStates, sizeof(PipelineState.Common.CurrentSamplerStates));

	FMemory::Memzero(PipelineState.Graphics.RenderTargetArray, sizeof(PipelineState.Graphics.RenderTargetArray));

	PipelineState.Graphics.CurrentIndexBufferLocation = nullptr;
	PipelineState.Graphics.CurrentIndexFormat = DXGI_FORMAT_UNKNOWN;
	PipelineState.Graphics.CurrentIndexOffset = 0;

	PipelineState.Graphics.CurrentPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

	bAutoFlushComputeShaderCache = true;
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

void FD3D12HighLevelGraphicsPipelineStateDesc::GetLowLevelDesc(FD3D12LowLevelGraphicsPipelineStateDesc& psoDesc)
{
	ZeroMemory(&psoDesc, sizeof(psoDesc));

	psoDesc.pRootSignature = BoundShaderState->pRootSignature;
	psoDesc.Desc.pRootSignature = psoDesc.pRootSignature->GetRootSignature();
	psoDesc.Desc.SampleMask = SampleMask;
	psoDesc.Desc.PrimitiveTopologyType = PrimitiveTopologyType;
	psoDesc.Desc.NumRenderTargets = NumRenderTargets;
	FMemory::Memcpy(psoDesc.Desc.RTVFormats, RTVFormats, sizeof(RTVFormats[0]) * NumRenderTargets);
	psoDesc.Desc.DSVFormat = DSVFormat;
	psoDesc.Desc.SampleDesc = SampleDesc;

	psoDesc.Desc.InputLayout = BoundShaderState->InputLayout;

	if (BoundShaderState->GetGeometryShader())
	{
		psoDesc.Desc.StreamOutput = BoundShaderState->GetGeometryShader()->StreamOutput;
	}

#define COPY_SHADER(Initial, Name) \
	if (FD3D12##Name##Shader* Shader = BoundShaderState->Get##Name##Shader()) \
	{ \
		psoDesc.Desc.Initial##S = Shader->ShaderBytecode.GetShaderBytecode(); \
		psoDesc.Initial##SHash = Shader->ShaderBytecode.GetHash(); \
	}
	COPY_SHADER(V, Vertex);
	COPY_SHADER(P, Pixel);
	COPY_SHADER(D, Domain);
	COPY_SHADER(H, Hull);
	COPY_SHADER(G, Geometry);
#undef COPY_SHADER

	psoDesc.Desc.BlendState = BlendState ? *BlendState : CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.Desc.RasterizerState = RasterizerState ? *RasterizerState : CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.Desc.DepthStencilState = DepthStencilState ? *DepthStencilState : CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
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
			ZeroMemory(&psoDescCompute, sizeof(psoDescCompute));

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

			ZeroMemory(psoDesc.RTVFormats, sizeof(psoDesc.RTVFormats));
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
			const uint32 VBCount = PipelineState.Graphics.MaxBoundVertexBufferIndex + 1;
			DescriptorCache.SetVertexBuffers(PipelineState.Graphics.CurrentVertexBuffers, VBCount);
			bNeedSetVB = false;
		}
		if (bNeedSetIB || bForceState)
		{
			if (PipelineState.Graphics.CurrentIndexBufferLocation != nullptr)
			{
				DescriptorCache.SetIndexBuffer(PipelineState.Graphics.CurrentIndexBufferLocation, PipelineState.Graphics.CurrentIndexFormat, PipelineState.Graphics.CurrentIndexOffset);
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
			const bool bDepthIsBoundAsSRV = PipelineState.Common.ShaderResourceViewsIntersectWithDepthCount > 0;
			DescriptorCache.SetRenderTargets(PipelineState.Graphics.RenderTargetArray, PipelineState.Graphics.HighLevelDesc.NumRenderTargets, PipelineState.Graphics.CurrentDepthStencilTarget, bDepthIsBoundAsSRV);
			bNeedSetRTs = false;
		}
	}

	// Reserve space in descriptor heaps
	// Since this can cause heap rollover (which causes old bindings to become invalid), the reserve must be done atomically
	const uint32 StartStage = IsCompute ? SF_Compute : 0;
	const uint32 EndStage = IsCompute ? SF_NumFrequencies : SF_Compute;
	const EShaderFrequency UAVStage = IsCompute ? SF_Compute : SF_Pixel;
	uint32 NumUAVs = 0;
	uint32 NumSRVs[SF_NumFrequencies + 1] ={};
	uint32 NumCBs[SF_NumFrequencies + 1] ={};


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
				DescriptorCache.SetSRVs((EShaderFrequency)i, PipelineState.Common.CurrentShaderResourceViews[i], NumSRVs[i], PipelineState.Common.CurrentShaderResourceViewsIntersectWithDepthRT[i], ViewHeapSlot);
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
				bNeedSetConstantBuffersPerShaderStage[i] = false;
			}
		}
		bNeedSetConstantBuffers = false;
	}

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

inline bool FDiskCacheInterface::IsInErrorState() const
{
	return !GEnablePSOCache || mInErrorState;
}


void FDiskCacheInterface::Init(FString &filename)
{
	mFileStart = nullptr;
	hFile = 0;
	hMemoryMap = 0;
	hMapAddress = 0;
	mCurrentFileMapSize = 0;
	mCurrentOffset = 0;
	mCacheExists = false;
	mInErrorState = false;

	mFileName = filename;
	mCacheExists = true;
	if (!GEnablePSOCache)
	{
		mCacheExists = false;
	}
	else
	{
		WIN32_FIND_DATA fileData;
		FindFirstFile(mFileName.GetCharArray().GetData(), &fileData);
		if (GetLastError() == ERROR_FILE_NOT_FOUND)
		{
			mCacheExists = false;
		}
	}
	bool fileFound = mCacheExists;
	mCurrentFileMapSize = 1;
	GrowMapping(64 * 1024, true);

	if (fileFound && mFileStart)
	{
		mHeader = *(FDiskCacheHeader*)mFileStart;
		if (mHeader.mHeaderVersion != mCurrentHeaderVersion || (!mHeader.mUsesAPILibraries && FD3D12PipelineStateCache::bUseAPILibaries))
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("Disk cache is stale. Disk Cache version: %d App version: %d"), mHeader.mHeaderVersion, mCurrentHeaderVersion);
			ClearDiskCache();
		}
	}
	else
	{
		mHeader.mHeaderVersion = mCurrentHeaderVersion;
		mHeader.mNumPsos = 0;
		mHeader.mSizeInBytes = 0;
		mHeader.mUsesAPILibraries = FD3D12PipelineStateCache::bUseAPILibaries;
	}
}

void FDiskCacheInterface::GrowMapping(SIZE_T size, bool firstrun)
{
	if (!GEnablePSOCache || mInErrorState)
	{
		return;
	}

	if (mCurrentFileMapSize - mCurrentOffset < size)
	{
		while ((mCurrentFileMapSize - mCurrentOffset) < size)
		{
			mCurrentFileMapSize += mFileGrowSize;
		}
	}
	else
	{
		return;
	}

	if (hMapAddress)
	{
		FlushViewOfFile(hMapAddress, mCurrentOffset);
		UnmapViewOfFile(hMapAddress);
	}
	if (hMemoryMap)
	{
		CloseHandle(hMemoryMap);
	}
	if (hFile)
	{
		CloseHandle(hFile);
	}

	uint32 flag = (mCacheExists) ? OPEN_EXISTING : CREATE_NEW;
	// open the shader cache file
	hFile = CreateFile(mFileName.GetCharArray().GetData(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, flag, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		//error state!
		mInErrorState = true;
		return;
	}

	mCacheExists = true;

	uint32 fileSize = GetFileSize(hFile, NULL);
	if (fileSize == 0)
	{
		byte data[64];
		FMemory::Memset(data, NULL, _countof(data));
		//It's invalide to map a zero sized file so write some junk data in that case
		WriteFile(hFile, data, _countof(data), NULL, NULL);
	}
	else if (firstrun)
	{
		mCurrentFileMapSize = fileSize;
	}

	hMemoryMap = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, (uint32)mCurrentFileMapSize, NULL);
	if (hMemoryMap == (HANDLE)nullptr)
	{
		//error state!
		mInErrorState = true;
		ClearDiskCache();
		return;
	}

	hMapAddress = MapViewOfFile(hMemoryMap, FILE_MAP_ALL_ACCESS, 0, 0, mCurrentFileMapSize);
	if (hMapAddress == (HANDLE)nullptr)
	{
		//error state!
		mInErrorState = true;
		ClearDiskCache();
		return;
	}

	mFileStart = (byte*)hMapAddress;
}

bool FDiskCacheInterface::AppendData(void* pData, size_t size)
{
	GrowMapping(size, false);
	if (IsInErrorState())
	{
		return false;
	}

	memcpy((mFileStart + mCurrentOffset), pData, size);
	mCurrentOffset += size;

	return true;
}

bool FDiskCacheInterface::SetPointerAndAdvanceFilePosition(void** pDest, size_t size, bool backWithSystemMemory)
{
	GrowMapping(size, false);
	if (IsInErrorState())
	{
		return false;
	}

	// Back persistent objects in system memory to avoid
	// troubles when re-mapping the file.
	if (backWithSystemMemory)
	{
		// Optimization: most (all?) of the shader input layout semantic names are "ATTRIBUTE"...
		// instead of making 1000's of attribute strings, just set it to a static one.
		static const char attribute[] = "ATTRIBUTE";
		if (size == sizeof(attribute) && FMemory::Memcmp((mFileStart + mCurrentOffset), attribute, sizeof(attribute)) == 0)
		{
			*pDest = (void*)attribute;
		}
		else
		{
			void* newMemory = FMemory::Malloc(size);
			if (newMemory)
			{
				FMemory::Memcpy(newMemory, (mFileStart + mCurrentOffset), size);
				mBackedMemory.Add(newMemory);
				*pDest = newMemory;
			}
			else
			{
				check(false);
				return false;
			}
		}
	}
	else
	{
		*pDest = mFileStart + mCurrentOffset;
	}

	mCurrentOffset += size;

	return true;
}

void FDiskCacheInterface::Reset(RESET_TYPE type)
{
	mCurrentOffset = sizeof(FDiskCacheHeader);

	if (type == RESET_TO_AFTER_LAST_OBJECT)
	{
		mCurrentOffset += mHeader.mSizeInBytes;
	}
}

void FDiskCacheInterface::Close(uint32 numberOfPSOs)
{
	mHeader.mNumPsos = numberOfPSOs;
	mHeader.mUsesAPILibraries = FD3D12PipelineStateCache::bUseAPILibaries;

	check(mCurrentOffset >= sizeof(FDiskCacheHeader));
	mHeader.mSizeInBytes = mCurrentOffset - sizeof(FDiskCacheHeader);

	if (!IsInErrorState())
	{
		if (hMapAddress)
		{
			*(FDiskCacheHeader*)mFileStart = mHeader;
			FlushViewOfFile(hMapAddress, mCurrentOffset);
			UnmapViewOfFile(hMapAddress);
		}
		if (hMemoryMap)
		{
			CloseHandle(hMemoryMap);
		}
		if (hFile)
		{
			CloseHandle(hFile);
		}
	}
}

void FDiskCacheInterface::ClearDiskCache()
{
	// Prevent reads/writes 
	mInErrorState = true;
	mHeader.mHeaderVersion = mCurrentHeaderVersion;
	mHeader.mNumPsos = 0;
	mHeader.mUsesAPILibraries = FD3D12PipelineStateCache::bUseAPILibaries;

	if (!GEnablePSOCache)
	{
		return;
	}

	if (hMapAddress)
	{
		UnmapViewOfFile(hMapAddress);
	}
	if (hMemoryMap)
	{
		CloseHandle(hMemoryMap);
	}
	if (hFile)
	{
		CloseHandle(hFile);
	}
#ifdef UNICODE
	BOOL result = DeleteFileW(mFileName.GetCharArray().GetData());
#else
	bool result = DeleteFileA(mFileName.GetCharArray().GetData());
#endif
	UE_LOG(LogD3D12RHI, Warning, TEXT("Deleted PSO Cache with result %d"), result);
}

void FDiskCacheInterface::Flush(uint32 numberOfPSOs)
{
	mHeader.mNumPsos = numberOfPSOs;
	mHeader.mUsesAPILibraries = FD3D12PipelineStateCache::bUseAPILibaries;

	check(mCurrentOffset >= sizeof(FDiskCacheHeader));
	mHeader.mSizeInBytes = mCurrentOffset - sizeof(FDiskCacheHeader);

	if (hMapAddress && !IsInErrorState())
	{
		*(FDiskCacheHeader*)mFileStart = mHeader;
		FlushViewOfFile(hMapAddress, mCurrentOffset);
	}
}

void* FDiskCacheInterface::GetDataAt(SIZE_T Offset) const
{ 
	void* data = mFileStart + Offset;

	check(data <= (mFileStart + mCurrentFileMapSize));
	return data;
}

FD3D12PipelineStateCache& FD3D12PipelineStateCache::operator=(const FD3D12PipelineStateCache& In)
{
	if (&In != this)
	{
		HighLevelGraphicsPipelineStateCache = In.HighLevelGraphicsPipelineStateCache;
		LowLevelGraphicsPipelineStateCache = In.LowLevelGraphicsPipelineStateCache;
		ComputePipelineStateCache = In.ComputePipelineStateCache;

		for (int32 Index = 0; Index < NUM_PSO_CACHE_TYPES; ++Index)
		{
			DiskCaches[Index] = In.DiskCaches[Index];
		}
#if UE_BUILD_DEBUG
		GraphicsCacheRequestCount = In.GraphicsCacheRequestCount;
		HighLevelCacheFulfillCount = In.HighLevelCacheFulfillCount;
		HighLevelCacheStaleCount = In.HighLevelCacheStaleCount;
		HighLevelCacheMissCount = In.HighLevelCacheMissCount;
#endif
	}
	return *this;
}

void FD3D12PipelineStateCache::RebuildFromDiskCache()
{
	FScopeLock Lock(&CS);
	if (DiskCaches[PSO_CACHE_GRAPHICS].IsInErrorState() || DiskCaches[PSO_CACHE_COMPUTE].IsInErrorState() ||
		(bUseAPILibaries && DiskBinaryCache.IsInErrorState()))
	{
		return;
	}
	// The only time shader code is ever read back is on debug builds
	// when it checks for hash collisions in the PSO map. Therefore
	// there is no point backing the memory on release.
#if UE_BUILD_DEBUG
	static const bool bBackShadersWithSystemMemory = true;
#else
	static const bool bBackShadersWithSystemMemory = false;
#endif

	DiskCaches[PSO_CACHE_GRAPHICS].Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
	DiskCaches[PSO_CACHE_COMPUTE].Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
	DiskBinaryCache.Reset(FDiskCacheInterface::RESET_TO_AFTER_LAST_OBJECT); // Reset this one to the end as we always append

	uint32 NumGraphicsPSOs = DiskCaches[PSO_CACHE_GRAPHICS].GetNumPSOs();

	for (uint32 i = 0; i < NumGraphicsPSOs; i++)
	{
		FD3D12LowLevelGraphicsPipelineStateDesc* GraphicsPSODesc = nullptr;
		DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&GraphicsPSODesc, sizeof(*GraphicsPSODesc));
		D3D12_GRAPHICS_PIPELINE_STATE_DESC* PSODesc = &GraphicsPSODesc->Desc;

		GraphicsPSODesc->pRootSignature = nullptr;
		SIZE_T* RSBlobLength = nullptr;
		DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&RSBlobLength, sizeof(*RSBlobLength));
		if (RSBlobLength > 0)
		{
			void* RSBlobData = nullptr;
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&RSBlobData, *RSBlobLength);

			// Create the root signature
			const HRESULT CreateRootSignatureHR = GetParentDevice()->GetDevice()->CreateRootSignature(0, RSBlobData, *RSBlobLength, IID_PPV_ARGS(&PSODesc->pRootSignature));
			if (FAILED(CreateRootSignatureHR))
			{
				// The cache has failed, build PSOs at runtime
				return;
			}
		}
		if (PSODesc->InputLayout.NumElements)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->InputLayout.pInputElementDescs, PSODesc->InputLayout.NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC), true);
			for (uint32 i = 0; i < PSODesc->InputLayout.NumElements; i++)
			{
				// Get the Sematic name string
				uint32* stringLength = nullptr;
				DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&stringLength, sizeof(uint32));
				DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->InputLayout.pInputElementDescs[i].SemanticName, *stringLength, true);
			}
		}
		if (PSODesc->StreamOutput.NumEntries)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->StreamOutput.pSODeclaration, PSODesc->StreamOutput.NumEntries * sizeof(D3D12_SO_DECLARATION_ENTRY), true);
			for (uint32 i = 0; i < PSODesc->StreamOutput.NumEntries; i++)
			{
				//Get the Sematic name string
				uint32* stringLength = nullptr;
				DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&stringLength, sizeof(uint32));
				DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->StreamOutput.pSODeclaration[i].SemanticName, *stringLength, true);
			}
		}
		if (PSODesc->StreamOutput.NumStrides)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->StreamOutput.pBufferStrides, PSODesc->StreamOutput.NumStrides * sizeof(uint32), true);
		}
		if (PSODesc->VS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->VS.pShaderBytecode, PSODesc->VS.BytecodeLength, bBackShadersWithSystemMemory);
		}
		if (PSODesc->PS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->PS.pShaderBytecode, PSODesc->PS.BytecodeLength, bBackShadersWithSystemMemory);
		}
		if (PSODesc->DS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->DS.pShaderBytecode, PSODesc->DS.BytecodeLength, bBackShadersWithSystemMemory);
		}
		if (PSODesc->HS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->HS.pShaderBytecode, PSODesc->HS.BytecodeLength, bBackShadersWithSystemMemory);
		}
		if (PSODesc->GS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->GS.pShaderBytecode, PSODesc->GS.BytecodeLength, bBackShadersWithSystemMemory);
		}

		ReadBackShaderBlob(*PSODesc, PSO_CACHE_GRAPHICS);

		if (!DiskCaches[PSO_CACHE_GRAPHICS].IsInErrorState())
		{
			GraphicsPSODesc->CombinedHash = FD3D12PipelineStateCache::HashPSODesc(*GraphicsPSODesc);

			FD3D12PipelineState& PSOOut = LowLevelGraphicsPipelineStateCache.Add(*GraphicsPSODesc, FD3D12PipelineState(GetParentDevice()));
			PSOOut.CreateAsync(GraphicsPSODesc);
		}
		else
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("PSO Cache read error!"));
			break;
		}
	}

	uint32 NumComputePSOs = DiskCaches[PSO_CACHE_COMPUTE].GetNumPSOs();

	for (uint32 i = 0; i < NumComputePSOs; i++)
	{
		FD3D12ComputePipelineStateDesc* ComputePSODesc = nullptr;
		DiskCaches[PSO_CACHE_COMPUTE].SetPointerAndAdvanceFilePosition((void**)&ComputePSODesc, sizeof(*ComputePSODesc));
		D3D12_COMPUTE_PIPELINE_STATE_DESC* PSODesc = &ComputePSODesc->Desc;

		ComputePSODesc->pRootSignature = nullptr;
		SIZE_T* RSBlobLength = nullptr;
		DiskCaches[PSO_CACHE_COMPUTE].SetPointerAndAdvanceFilePosition((void**)&RSBlobLength, sizeof(*RSBlobLength));
		if (RSBlobLength > 0)
		{
			void* RSBlobData = nullptr;
			DiskCaches[PSO_CACHE_COMPUTE].SetPointerAndAdvanceFilePosition((void**)&RSBlobData, *RSBlobLength);

			// Create the root signature
			const HRESULT CreateRootSignatureHR = GetParentDevice()->GetDevice()->CreateRootSignature(0, RSBlobData, *RSBlobLength, IID_PPV_ARGS(&PSODesc->pRootSignature));
			if (FAILED(CreateRootSignatureHR))
			{
				// The cache has failed, build PSOs at runtime
				return;
			}
		}
		if (PSODesc->CS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_COMPUTE].SetPointerAndAdvanceFilePosition((void**)&PSODesc->CS.pShaderBytecode, PSODesc->CS.BytecodeLength, bBackShadersWithSystemMemory);
		}

		ReadBackShaderBlob(*PSODesc, PSO_CACHE_COMPUTE);

		if (!DiskCaches[PSO_CACHE_COMPUTE].IsInErrorState())
		{
			ComputePSODesc->CombinedHash = FD3D12PipelineStateCache::HashPSODesc(*ComputePSODesc);
			FD3D12PipelineState& PSOOut = ComputePipelineStateCache.Add(*ComputePSODesc, FD3D12PipelineState(GetParentDevice()));
			PSOOut.CreateAsync(ComputePSODesc);
		}
		else
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("PSO Cache read error!"));
			break;
		}
	}
}

ID3D12PipelineState* FD3D12PipelineStateCache::FindGraphics(FD3D12HighLevelGraphicsPipelineStateDesc &graphicsPSODesc)
{
	FScopeLock Lock(&CS);

#if UE_BUILD_DEBUG
	++GraphicsCacheRequestCount;
#endif

	graphicsPSODesc.CombinedHash = FD3D12PipelineStateCache::HashPSODesc(graphicsPSODesc);

	uint64 BSSUniqueID = graphicsPSODesc.BoundShaderState ? graphicsPSODesc.BoundShaderState->UniqueID : 0;
	TPair<ID3D12PipelineState*, uint64>* HighLevelCacheEntry = HighLevelGraphicsPipelineStateCache.Find(graphicsPSODesc);
	if (HighLevelCacheEntry && HighLevelCacheEntry->Value == BSSUniqueID)
	{
#if UE_BUILD_DEBUG
		++HighLevelCacheFulfillCount; // No low-level cache hit
#endif
		return HighLevelCacheEntry->Key;
	}

	FD3D12LowLevelGraphicsPipelineStateDesc lowLevelDesc;
	graphicsPSODesc.GetLowLevelDesc(lowLevelDesc);
	ID3D12PipelineState* PSO = FindGraphicsLowLevel(lowLevelDesc);

	if (HighLevelCacheEntry)
	{
#if UE_BUILD_DEBUG
		++HighLevelCacheStaleCount; // High-level cache hit, but was stale due to BSS memory re-use
#endif
		HighLevelCacheEntry->Key = PSO;
	}
	else
	{
#if UE_BUILD_DEBUG
		++HighLevelCacheMissCount; // No high-level cache hit
#endif
		HighLevelGraphicsPipelineStateCache.Add(graphicsPSODesc,
			TPairInitializer<ID3D12PipelineState*, uint64>(PSO, BSSUniqueID));
	}
	return PSO;
}

ID3D12PipelineState* FD3D12PipelineStateCache::FindGraphicsLowLevel(FD3D12LowLevelGraphicsPipelineStateDesc &graphicsPSODesc)
{
	// Lock already taken by high level find
	graphicsPSODesc.CombinedHash = FD3D12PipelineStateCache::HashPSODesc(graphicsPSODesc);

	FD3D12PipelineState* PSO = LowLevelGraphicsPipelineStateCache.Find(graphicsPSODesc);

	if (PSO)
	{
		ID3D12PipelineState* APIPso = PSO->GetPipelineState();

		if (APIPso)
		{
			return APIPso;
		}
		else
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("PSO re-creation failed. Most likely on disk descriptor corruption."));
			for (uint32 i = 0; i < NUM_PSO_CACHE_TYPES; i++)
			{
				DiskCaches[i].ClearDiskCache();
			}
		}
	}

	return Add(graphicsPSODesc);
}

ID3D12PipelineState* FD3D12PipelineStateCache::FindCompute(FD3D12ComputePipelineStateDesc &computePSODesc)
{
	FScopeLock Lock(&CS);
	computePSODesc.CombinedHash = FD3D12PipelineStateCache::HashPSODesc(computePSODesc);

	FD3D12PipelineState* PSO = ComputePipelineStateCache.Find(computePSODesc);

	if (PSO)
	{
		ID3D12PipelineState* APIPso = PSO->GetPipelineState();

		if (APIPso)
		{
			return APIPso;
		}
		else
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("PSO re-creation failed. Most likely on disk descriptor corruption."));
			for (uint32 i = 0; i < NUM_PSO_CACHE_TYPES; i++)
			{
				DiskCaches[i].ClearDiskCache();
			}
		}
	}

	return Add(computePSODesc);
}

ID3D12PipelineState* FD3D12PipelineStateCache::Add(FD3D12LowLevelGraphicsPipelineStateDesc &graphicsPSODesc)
{
	FScopeLock Lock(&CS);

	FD3D12PipelineState& PSOOut = LowLevelGraphicsPipelineStateCache.Add(graphicsPSODesc, FD3D12PipelineState(GetParentDevice()));
	PSOOut.Create(&graphicsPSODesc);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC &psoDesc = graphicsPSODesc.Desc;

	ID3D12PipelineState* APIPso = PSOOut.GetPipelineState();
	if (APIPso == nullptr)
	{
		UE_LOG(LogD3D12RHI, Warning, TEXT("Runtime PSO creation failed."));
		return nullptr;
	}

	//TODO: Optimize by only storing unique pointers
	if (!DiskCaches[PSO_CACHE_GRAPHICS].IsInErrorState())
	{
		DiskCaches[PSO_CACHE_GRAPHICS].AppendData(&graphicsPSODesc, sizeof(graphicsPSODesc));

		ID3DBlob* const pRSBlob = graphicsPSODesc.pRootSignature ? graphicsPSODesc.pRootSignature->GetRootSignatureBlob() : nullptr;
		const SIZE_T RSBlobLength = pRSBlob ? pRSBlob->GetBufferSize() : 0;
		DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)(&RSBlobLength), sizeof(RSBlobLength));
		if (RSBlobLength > 0)
		{
			// Save the root signature
			check(graphicsPSODesc.pRootSignature->GetRootSignature() == psoDesc.pRootSignature);
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData(pRSBlob->GetBufferPointer(), RSBlobLength);
		}
		if (psoDesc.InputLayout.NumElements)
		{
			//Save the layout structs
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)psoDesc.InputLayout.pInputElementDescs, psoDesc.InputLayout.NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC));
			for (uint32 i = 0; i < psoDesc.InputLayout.NumElements; i++)
			{
				//Save the Sematic name string
				uint32 stringLength = (uint32)strnlen_s(psoDesc.InputLayout.pInputElementDescs[i].SemanticName, IL_MAX_SEMANTIC_NAME);
				stringLength++; // include the NULL char
				DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)&stringLength, sizeof(stringLength));
				DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)psoDesc.InputLayout.pInputElementDescs[i].SemanticName, stringLength);
			}
		}
		if (psoDesc.StreamOutput.NumEntries)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)&psoDesc.StreamOutput.pSODeclaration, psoDesc.StreamOutput.NumEntries * sizeof(D3D12_SO_DECLARATION_ENTRY));
			for (uint32 i = 0; i < psoDesc.StreamOutput.NumEntries; i++)
			{
				//Save the Sematic name string
				uint32 stringLength = (uint32)strnlen_s(psoDesc.StreamOutput.pSODeclaration[i].SemanticName, IL_MAX_SEMANTIC_NAME);
				stringLength++; // include the NULL char
				DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)&stringLength, sizeof(stringLength));
				DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)psoDesc.StreamOutput.pSODeclaration[i].SemanticName, stringLength);
			}
		}
		if (psoDesc.StreamOutput.NumStrides)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)&psoDesc.StreamOutput.pBufferStrides, psoDesc.StreamOutput.NumStrides * sizeof(uint32));
		}
		if (psoDesc.VS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)psoDesc.VS.pShaderBytecode, psoDesc.VS.BytecodeLength);
		}
		if (psoDesc.PS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)psoDesc.PS.pShaderBytecode, psoDesc.PS.BytecodeLength);
		}
		if (psoDesc.DS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)psoDesc.DS.pShaderBytecode, psoDesc.DS.BytecodeLength);
		}
		if (psoDesc.HS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)psoDesc.HS.pShaderBytecode, psoDesc.HS.BytecodeLength);
		}
		if (psoDesc.GS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].AppendData((void*)psoDesc.GS.pShaderBytecode, psoDesc.GS.BytecodeLength);
		}

		WriteOutShaderBlob(PSO_CACHE_GRAPHICS, APIPso);

		DiskCaches[PSO_CACHE_GRAPHICS].Flush(LowLevelGraphicsPipelineStateCache.Num());
	}

	return APIPso;
}

ID3D12PipelineState* FD3D12PipelineStateCache::Add(FD3D12ComputePipelineStateDesc &computePSODesc)
{
	FScopeLock Lock(&CS);

	FD3D12PipelineState& PSOOut = ComputePipelineStateCache.Add(computePSODesc, FD3D12PipelineState(GetParentDevice()));
	PSOOut.Create(&computePSODesc);

	ID3D12PipelineState* APIPso = PSOOut.GetPipelineState();
	if (APIPso == nullptr)
	{
		UE_LOG(LogD3D12RHI, Warning, TEXT("Runtime PSO creation failed."));
		return nullptr;
	}

	D3D12_COMPUTE_PIPELINE_STATE_DESC &psoDesc = computePSODesc.Desc;

	if (!DiskCaches[PSO_CACHE_COMPUTE].IsInErrorState())
	{
		DiskCaches[PSO_CACHE_COMPUTE].AppendData(&computePSODesc, sizeof(computePSODesc));

		ID3DBlob* const pRSBlob = computePSODesc.pRootSignature ? computePSODesc.pRootSignature->GetRootSignatureBlob() : nullptr;
		const SIZE_T RSBlobLength = pRSBlob ? pRSBlob->GetBufferSize() : 0;
		DiskCaches[PSO_CACHE_COMPUTE].AppendData((void*)(&RSBlobLength), sizeof(RSBlobLength));
		if (RSBlobLength > 0)
		{
			// Save the root signature
			check(computePSODesc.pRootSignature->GetRootSignature() == psoDesc.pRootSignature);
			DiskCaches[PSO_CACHE_COMPUTE].AppendData(pRSBlob->GetBufferPointer(), RSBlobLength);
		}
		if (psoDesc.CS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_COMPUTE].AppendData((void*)psoDesc.CS.pShaderBytecode, psoDesc.CS.BytecodeLength);
		}

		WriteOutShaderBlob(PSO_CACHE_COMPUTE, APIPso);

		DiskCaches[PSO_CACHE_COMPUTE].Flush(ComputePipelineStateCache.Num());
	}

	return APIPso;
}

void FD3D12PipelineStateCache::WriteOutShaderBlob(PSO_CACHE_TYPE Cache, ID3D12PipelineState* APIPso)
{
	if (bUseAPILibaries)
	{
		TRefCountPtr<ID3DBlob> cachedBlob;
		HRESULT result = APIPso->GetCachedBlob(cachedBlob.GetInitReference());
		VERIFYD3D11RESULT(result);
		if (SUCCEEDED(result))
		{
			SIZE_T bufferSize = cachedBlob->GetBufferSize();

			SIZE_T currentOffset = DiskBinaryCache.GetCurrentOffset();
			DiskBinaryCache.AppendData(cachedBlob->GetBufferPointer(), bufferSize);

			DiskCaches[Cache].AppendData(&currentOffset, sizeof(currentOffset));
			DiskCaches[Cache].AppendData(&bufferSize, sizeof(bufferSize));

			DriverShaderBlobs++;

			DiskBinaryCache.Flush(DriverShaderBlobs);
		}
		else
		{
			check(false);
			SIZE_T bufferSize = 0;
			DiskCaches[Cache].AppendData(&bufferSize, sizeof(bufferSize));
			DiskCaches[Cache].AppendData(&bufferSize, sizeof(bufferSize));
		}
	}
	else
	{
		SIZE_T bufferSize = 0;
		DiskCaches[Cache].AppendData(&bufferSize, sizeof(bufferSize));
		DiskCaches[Cache].AppendData(&bufferSize, sizeof(bufferSize));
	}
}

void FD3D12PipelineStateCache::Close()
{
	FScopeLock Lock(&CS);

	DiskCaches[PSO_CACHE_GRAPHICS].Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
	DiskCaches[PSO_CACHE_COMPUTE].Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
	DiskBinaryCache.Reset(FDiskCacheInterface::RESET_TO_AFTER_LAST_OBJECT);


	DiskCaches[PSO_CACHE_GRAPHICS].Close(LowLevelGraphicsPipelineStateCache.Num());
	DiskCaches[PSO_CACHE_COMPUTE].Close(ComputePipelineStateCache.Num());
	DiskBinaryCache.Close(DriverShaderBlobs);

	HighLevelGraphicsPipelineStateCache.Empty();
	LowLevelGraphicsPipelineStateCache.Empty();
	ComputePipelineStateCache.Empty();
}

FORCEINLINE uint32 SSE4_CRC32(void* data, SIZE_T numBytes)
{
	check(GCPUSupportsSSE4);
	uint32 hash = 0;
#if _WIN64
	static const SIZE_T alignment = 8;//64 Bit
#elif _WIN32
	static const SIZE_T alignment = 4;//32 Bit
#else
	check(0);
	return 0;
#endif

	const SIZE_T roundingIterations = (numBytes & (alignment - 1));
	uint8* unalignedData = (uint8*)data;
	for (SIZE_T i = 0; i < roundingIterations; i++)
	{
		hash = _mm_crc32_u8(hash, unalignedData[i]);
	}
	unalignedData += roundingIterations;
	numBytes -= roundingIterations;

	SIZE_T* alignedData = (SIZE_T*)unalignedData;
	check((numBytes % alignment) == 0);
	const SIZE_T numIterations = (numBytes / alignment);
	for (SIZE_T i = 0; i < numIterations; i++)
	{
#if _WIN64
		hash = _mm_crc32_u64(hash, alignedData[i]);
#else
		hash = _mm_crc32_u32(hash, alignedData[i]);
#endif
	}

	return hash;
}

SIZE_T FD3D12PipelineStateCache::HashData(void* data, SIZE_T numBytes)
{
	if (GCPUSupportsSSE4)
	{
		return SIZE_T(SSE4_CRC32(data, numBytes));
	}
	else
	{
		return SIZE_T(FCrc::MemCrc32(data, numBytes));
	}
}

//TODO: optimize by pre-hashing these things at set time
SIZE_T FD3D12PipelineStateCache::HashPSODesc(const FD3D12LowLevelGraphicsPipelineStateDesc &graphicsPSODesc)
{
	__declspec(align(32)) FD3D12LowLevelGraphicsPipelineStateDesc hashTemp;
	FMemory::Memcpy(&hashTemp, &graphicsPSODesc, sizeof(hashTemp)); // Memcpy to avoid introducing garbage due to alignment
	hashTemp.Desc.VS.pShaderBytecode = nullptr; // null out pointers so stale ones don't ruin the hash
	hashTemp.Desc.PS.pShaderBytecode = nullptr;
	hashTemp.Desc.HS.pShaderBytecode = nullptr;
	hashTemp.Desc.DS.pShaderBytecode = nullptr;
	hashTemp.Desc.GS.pShaderBytecode = nullptr;
	hashTemp.Desc.InputLayout.pInputElementDescs = nullptr;
	hashTemp.Desc.StreamOutput.pBufferStrides = nullptr;
	hashTemp.Desc.StreamOutput.pSODeclaration = nullptr;
	hashTemp.Desc.CachedPSO.pCachedBlob = nullptr;
	hashTemp.Desc.CachedPSO.CachedBlobSizeInBytes = 0;
	hashTemp.CombinedHash = 0;
	hashTemp.Desc.pRootSignature = nullptr;
	hashTemp.pRootSignature = nullptr;

	return HashData(&hashTemp, sizeof(hashTemp));
}

SIZE_T FD3D12PipelineStateCache::HashPSODesc(const FD3D12HighLevelGraphicsPipelineStateDesc &graphicsPSODesc)
{
	__declspec(align(32)) FD3D12HighLevelGraphicsPipelineStateDesc hashTemp;
	FMemory::Memcpy(&hashTemp, &graphicsPSODesc, sizeof(hashTemp)); // Memcpy to avoid introducing garbage due to alignment
	hashTemp.CombinedHash = 0;
	return HashData(&hashTemp, sizeof(hashTemp));
}

SIZE_T FD3D12PipelineStateCache::HashPSODesc(const FD3D12ComputePipelineStateDesc &psoDesc)
{
	__declspec(align(32)) FD3D12ComputePipelineStateDesc hashTemp;
	FMemory::Memcpy(&hashTemp, &psoDesc, sizeof(hashTemp)); // Memcpy to avoid introducing garbage due to alignment
	hashTemp.Desc.CS.pShaderBytecode = nullptr;  // null out pointers so stale ones don't ruin the hash
	hashTemp.Desc.CachedPSO.pCachedBlob = nullptr;
	hashTemp.Desc.CachedPSO.CachedBlobSizeInBytes = 0;
	hashTemp.CombinedHash = 0;
	hashTemp.Desc.pRootSignature = nullptr;
	hashTemp.pRootSignature = nullptr;

	return HashData(&hashTemp, sizeof(hashTemp));
}


#define SSE4_2     0x100000 
#define SSE4_CPUID_ARRAY_INDEX 2
void FD3D12PipelineStateCache::Init(FString &GraphicsCacheFilename, FString &ComputeCacheFilename, FString &DriverBlobFilename)
{
	FScopeLock Lock(&CS);

	DiskCaches[PSO_CACHE_GRAPHICS].Init(GraphicsCacheFilename);
	DiskCaches[PSO_CACHE_COMPUTE].Init(ComputeCacheFilename);
	DiskBinaryCache.Init(DriverBlobFilename);

	DiskCaches[PSO_CACHE_GRAPHICS].Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
	DiskCaches[PSO_CACHE_COMPUTE].Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
	DiskBinaryCache.Reset(FDiskCacheInterface::RESET_TO_AFTER_LAST_OBJECT);

	DriverShaderBlobs = DiskBinaryCache.GetNumPSOs();

	// Check for SSE4 support see: https://msdn.microsoft.com/en-us/library/vstudio/hskdteyh(v=vs.100).aspx
	{
		int32 cpui[4];
		__cpuidex(cpui, 1, 0);
		GCPUSupportsSSE4 = !!(cpui[SSE4_CPUID_ARRAY_INDEX] & SSE4_2);
	}

}

FD3D12PipelineStateCache::FD3D12PipelineStateCache(FD3D12Device* InParent) :
	DriverShaderBlobs(0)
	, FD3D12DeviceChild(InParent)
{

}

FD3D12PipelineStateCache::~FD3D12PipelineStateCache()
{

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
	VERIFYD3D11RESULT(pCommandList->QueryInterface(pDebugCommandList.GetInitReference()));

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
				const bool bSRVBound = PipelineState.Common.ShaderResourceViewsIntersectWithDepthCount > 0;
				uint32 SanityCheckCount = 0;
				const uint32 StartStage = 0;
				const uint32 EndStage = SF_Compute;
				for (uint32 Stage = StartStage; Stage < EndStage; Stage++)
				{
					for (uint32 i = 0; i < MAX_SRVS; i++)
					{
						if (PipelineState.Common.CurrentShaderResourceViewsIntersectWithDepthRT[Stage][i])
						{
							SanityCheckCount++;
						}
					}
				}
				check(SanityCheckCount == PipelineState.Common.ShaderResourceViewsIntersectWithDepthCount);

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
				FD3D12ShaderResourceView* pCurrentView = PipelineState.Common.CurrentShaderResourceViews[Stage][i];
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

#if SUPPORTS_MEMORY_RESIDENCY
			UAVArray[i]->CounterResource->UpdateResidency();
#endif

			CmdContext->CommandListHandle->CopyBufferRegion(
				UAVArray[i]->CounterResource->GetResource(),
				0,
				CounterUploadHeap,
				CounterUploadHeapIndex * 4,
				4);

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
	if (bAlwaysSetIndexBuffers || (PipelineState.Graphics.CurrentIndexBufferLocation != IndexBufferLocation || PipelineState.Graphics.CurrentIndexFormat != Format || PipelineState.Graphics.CurrentIndexOffset != Offset) || GD3D12SkipStateCaching)
	{
		PipelineState.Graphics.CurrentIndexBufferLocation = IndexBufferLocation;
		PipelineState.Graphics.CurrentIndexFormat = Format;
		PipelineState.Graphics.CurrentIndexOffset = Offset;
		bNeedSetIB = true;
	}
}

void FD3D12StateCacheBase::InternalSetStreamSource(FD3D12ResourceLocation* VertexBufferLocation, uint32 StreamIndex, uint32 Stride, uint32 Offset, TSetStreamSourceAlternate AlternatePathFunction)
{
	check(StreamIndex < ARRAYSIZE(PipelineState.Graphics.CurrentVertexBuffers));
	FD3D12VertexBufferState& Slot = PipelineState.Graphics.CurrentVertexBuffers[StreamIndex];
	if ((Slot.VertexBufferLocation != VertexBufferLocation || Slot.Offset != Offset || Slot.Stride != Stride) || GD3D12SkipStateCaching)
	{
		Slot.VertexBufferLocation = VertexBufferLocation;
		Slot.Offset = Offset;
		Slot.Stride = Stride;
		if (AlternatePathFunction != nullptr)
		{
			(*AlternatePathFunction)(this, VertexBufferLocation->GetResource(), StreamIndex, Stride, Offset + VertexBufferLocation->GetOffset());
		}
		else
		{
			bNeedSetVB = true;
		}

		// Keep track of the highest bound vertex buffer
		int32& MaxResourceIndex = PipelineState.Graphics.MaxBoundVertexBufferIndex;
		if (VertexBufferLocation)
		{
			// Update the max resource index to the highest bound resource index.
			MaxResourceIndex = FMath::Max(MaxResourceIndex, static_cast<int32>(StreamIndex));
		}
		else
		{
			// If this was the highest bound resource...
			if (MaxResourceIndex == StreamIndex)
			{
				// Adjust the max resource index downwards until we
				// hit the next non-null slot, or we've run out of slots.
				do
				{
					MaxResourceIndex--;
				} while (MaxResourceIndex >= 0 && !PipelineState.Graphics.CurrentVertexBuffers[MaxResourceIndex].VertexBufferLocation);
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

void FD3D12DescriptorCache::HeapRolledOver(D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
	// When using sub-allocation a heap roll over doesn't require resetting the heap, unless
	// the sampler heap has rolled over.
	if (CurrentViewHeap != &SubAllocatedViewHeap || Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
	{
		TArray<ID3D12DescriptorHeap*> Heaps;
		Heaps.Add(CurrentViewHeap->GetHeap());
		Heaps.Add(CurrentSamplerHeap->GetHeap());
		CmdContext->SetDescriptorHeaps(Heaps);
	}

	if (Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
	{
		CmdContext->StateCache.DirtyViewDescriptorTables();
		ViewHeapSequenceNumber++;

		SRVMap.Reset();
	}
	else
	{
		check(Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		CmdContext->StateCache.DirtySamplerDescriptorTables();

		SamplerMap.Reset();
	}
}

void FD3D12DescriptorCache::HeapLoopedAround(D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
	if (Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
	{
		ViewHeapSequenceNumber++;

		SRVMap.Reset();
	}
	else
	{
		check(Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

		SamplerMap.Reset();
	}
}

void FD3D12DescriptorCache::Init(FD3D12Device* InParent, FD3D12CommandContext* InCmdContext, uint32 InNumViewDescriptors, uint32 InNumSamplerDescriptors, FD3D12SubAllocatedOnlineHeap::SubAllocationDesc& SubHeapDesc)
{
	Parent = InParent;
	CmdContext = InCmdContext;
	SubAllocatedViewHeap.SetParent(this);
	LocalSamplerHeap.SetParent(this);

	SubAllocatedViewHeap.SetParentDevice(InParent);
	LocalSamplerHeap.SetParentDevice(InParent);

	SubAllocatedViewHeap.Init(SubHeapDesc);

	// Always Init a local sampler heap as the high level cache will always miss initialy
	// so we need something to fall back on (The view heap never rolls over so we init that one
	// lazily as a backup to save memory)
	LocalSamplerHeap.Init(InNumSamplerDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	NumLocalViewDescriptors = InNumViewDescriptors;

	CurrentViewHeap = &SubAllocatedViewHeap; //Begin with the global heap
	CurrentSamplerHeap = &LocalSamplerHeap;
	bUsingGlobalSamplerHeap = false;

	TArray<ID3D12DescriptorHeap*> Heaps;
	Heaps.Add(CurrentViewHeap->GetHeap());
	Heaps.Add(CurrentSamplerHeap->GetHeap());
	InCmdContext->SetDescriptorHeaps(Heaps);

	// Create default views
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	SRVDesc.Texture2D.MipLevels = 1;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	pNullSRV = new FD3D12ShaderResourceView(GetParentDevice(), &SRVDesc, nullptr);

	D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
	RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	RTVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	RTVDesc.Texture2D.MipSlice = 0;
	pNullRTV = new FD3D12RenderTargetView(GetParentDevice(), &RTVDesc, nullptr);

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	UAVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	UAVDesc.Texture2D.MipSlice = 0;
	pNullUAV = new FD3D12UnorderedAccessView(GetParentDevice(), &UAVDesc, nullptr);

	// Init offline descriptor allocators
	CBVAllocator.Init(GetParentDevice()->GetDevice());
	FDescriptorHeapManager::HeapIndex heapIndex;
	pNullCBV = CBVAllocator.AllocateHeapSlot(heapIndex);
	check(pNullCBV.ptr != 0);

	D3D12_CONSTANT_BUFFER_VIEW_DESC CBView = {};
	CBView.BufferLocation = 0;
	CBView.SizeInBytes = 0;
	GetParentDevice()->GetDevice()->CreateConstantBufferView(&CBView, pNullCBV);

	FSamplerStateInitializerRHI SamplerDesc(
		SF_Trilinear,
		AM_Clamp,
		AM_Clamp,
		AM_Clamp,
		0,
		0,
		0,
		FLT_MAX
		);

	FSamplerStateRHIRef Sampler = GetParentDevice()->CreateSamplerState(SamplerDesc);

	pDefaultSampler = static_cast<FD3D12SamplerState*>(Sampler.GetReference());

	// The default sampler must have ID=0
	// DescriptorCache::SetSamplers relies on this
	check(pDefaultSampler->ID == 0);

	// Create offline SRV heaps
	for (uint32 ShaderFrequency = 0; ShaderFrequency < _countof(OfflineHeap); ShaderFrequency++)
	{
		OfflineHeap[ShaderFrequency].CurrentSRVSequenceNumber.AddZeroed(D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);

		OfflineHeap[ShaderFrequency].CurrentUniformBufferSequenceNumber.AddZeroed(D3D12_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);

		D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
		HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		HeapDesc.NumDescriptors = D3D12_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT + D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
		HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		VERIFYD3D11RESULT(GetParentDevice()->GetDevice()->CreateDescriptorHeap(
			&HeapDesc,
			IID_PPV_ARGS(OfflineHeap[ShaderFrequency].Heap.GetInitReference())
			));
		SetName(OfflineHeap[ShaderFrequency].Heap, L"Offline View Heap");

		D3D12_CPU_DESCRIPTOR_HANDLE HeapBase = OfflineHeap[ShaderFrequency].Heap->GetCPUDescriptorHandleForHeapStart();

		// The first 14 descriptors are for CBVs
		// The next 128 are for SRVs
		OfflineHeap[ShaderFrequency].CBVBaseHandle = HeapBase;

		OfflineHeap[ShaderFrequency].SRVBaseHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(HeapBase, D3D12_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, CurrentViewHeap->GetDescriptorSize());

		// Fill the offline heap with null descriptors
		for (uint32 i = 0; i < D3D12_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE OfflineDescriptor(OfflineHeap[ShaderFrequency].CBVBaseHandle, i, CurrentViewHeap->GetDescriptorSize());

			CD3DX12_CPU_DESCRIPTOR_HANDLE SrcDescriptor = pNullCBV;

			GetParentDevice()->GetDevice()->CopyDescriptorsSimple(1, OfflineDescriptor, SrcDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}

		for (uint32 i = 0; i < D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE OfflineDescriptor(OfflineHeap[ShaderFrequency].SRVBaseHandle, i, CurrentViewHeap->GetDescriptorSize());
			CD3DX12_CPU_DESCRIPTOR_HANDLE SrcDescriptor = pNullSRV->GetView();

			GetParentDevice()->GetDevice()->CopyDescriptorsSimple(1, OfflineDescriptor, SrcDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
	}
}

void FD3D12DescriptorCache::Clear()
{
	pNullSRV = nullptr;
	pNullUAV = nullptr;
	pNullRTV = nullptr;
}

void FD3D12DescriptorCache::BeginFrame()
{
	FD3D12GlobalOnlineHeap& DeviceSamplerHeap = GetParentDevice()->GetGlobalSamplerHeap();

	{
		FScopeLock Lock(&DeviceSamplerHeap.GetCriticalSection());
		if (DeviceSamplerHeap.DescriptorTablesDirty())
		{
			LocalSamplerSet = DeviceSamplerHeap.GetUniqueDescriptorTables();
		}
	}

	SwitchToGlobalSamplerHeap();
}

void FD3D12DescriptorCache::EndFrame()
{
	SRVMap.Reset();

	if (UniqueTables.Num())
	{
		GatherUniqueSamplerTables();
	}
}

void FD3D12DescriptorCache::GatherUniqueSamplerTables()
{
	FD3D12GlobalOnlineHeap& deviceSamplerHeap = GetParentDevice()->GetGlobalSamplerHeap();

	FScopeLock Lock(&deviceSamplerHeap.GetCriticalSection());

	auto& TableSet = deviceSamplerHeap.GetUniqueDescriptorTables();

	for (auto& Table : UniqueTables)
	{
		if (TableSet.Contains(Table) == false)
		{
			uint32 HeapSlot = deviceSamplerHeap.ReserveSlots(Table.Key.Count);

			if (HeapSlot != FD3D12GlobalOnlineHeap::HeapExhaustedValue)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor = deviceSamplerHeap.GetCPUSlotHandle(HeapSlot);

				GetParentDevice()->GetDevice()->CopyDescriptors(
					1, &DestDescriptor, &Table.Key.Count,
					Table.Key.Count, Table.CPUTable, nullptr /* sizes */,
					D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

				Table.GPUHandle = deviceSamplerHeap.GetGPUSlotHandle(HeapSlot);
				TableSet.Add(Table);

				deviceSamplerHeap.ToggleDescriptorTablesDirtyFlag(true);
			}
		}
	}

	// Reset the tables as the next frame should inherit them from the global heap
	UniqueTables.Empty();
}

void FD3D12DescriptorCache::NotifyCurrentCommandList(const FD3D12CommandListHandle& CommandListHandle)
{
	CurrentViewHeap->NotifyCurrentCommandList(CommandListHandle);

	// The global sampler heap doesn't care about the current command list
	LocalSamplerHeap.NotifyCurrentCommandList(CommandListHandle);
}

void FD3D12DescriptorCache::SetIndexBuffer(FD3D12ResourceLocation* IndexBufferLocation, DXGI_FORMAT Format, uint32 Offset)
{
	if (IndexBufferLocation != NULL)
	{
		FD3D12Resource* IndexBuffer = IndexBufferLocation->GetResource();

		D3D12_INDEX_BUFFER_VIEW IBView;
		IBView.BufferLocation = IndexBuffer->GetResource()->GetGPUVirtualAddress() + IndexBufferLocation->GetOffset() + Offset;
		IBView.Format = Format;
		IBView.SizeInBytes = IndexBufferLocation->GetEffectiveBufferSize();

		CmdContext->CommandListHandle->IASetIndexBuffer(&IBView);
	}
}

void FD3D12DescriptorCache::SetVertexBuffers(FD3D12VertexBufferState* VertexBuffers, uint32 Count)
{
	const uint32 SlotsNeeded = Count;
	if (0 == SlotsNeeded)
	{
		return; // No-op
	}

	D3D12_VERTEX_BUFFER_VIEW VBViews[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

	// Fill heap slots
	for (uint32 i = 0; i < SlotsNeeded; i++)
	{
		ID3D12Resource* VertexBuffer = VertexBuffers[i].VertexBufferLocation ? VertexBuffers[i].VertexBufferLocation->GetResource()->GetResource() : nullptr;
		if (VertexBuffer != nullptr)
		{
			D3D12_VERTEX_BUFFER_VIEW &currentView = VBViews[i];
			currentView.BufferLocation = VertexBuffer->GetGPUVirtualAddress() + VertexBuffers[i].VertexBufferLocation->GetOffset() + VertexBuffers[i].Offset;
			currentView.StrideInBytes = VertexBuffers[i].Stride;
			currentView.SizeInBytes = VertexBuffers[i].VertexBufferLocation->GetEffectiveBufferSize() - VertexBuffers[i].Offset;	// Make sure we account for how much we offset into the VB
		}
		else
		{
			FMemory::Memzero(&VBViews[i], sizeof(VBViews[i]));
		}
	}

	CmdContext->CommandListHandle->IASetVertexBuffers(0, SlotsNeeded, VBViews);
}

void FD3D12DescriptorCache::SetUAVs(EShaderFrequency ShaderStage, uint32 UAVStartSlot, TRefCountPtr<FD3D12UnorderedAccessView>* UnorderedAccessViewArray, uint32 SlotsNeeded, uint32 &HeapSlot)
{
	if (0 == SlotsNeeded)
	{
		return; // No-op
	}

	// Reserve heap slots
	uint32 FirstSlotIndex = HeapSlot;
	HeapSlot += SlotsNeeded;
	CD3DX12_CPU_DESCRIPTOR_HANDLE DestDescriptor(CurrentViewHeap->GetCPUSlotHandle(FirstSlotIndex));
	CD3DX12_GPU_DESCRIPTOR_HANDLE BindDescriptor(CurrentViewHeap->GetGPUSlotHandle(FirstSlotIndex));

	CD3DX12_CPU_DESCRIPTOR_HANDLE SrcDescriptors[D3D12_PS_CS_UAV_REGISTER_COUNT];

	FD3D12CommandListHandle& CommandList = CmdContext->CommandListHandle;

	// Fill heap slots
	check(UAVStartSlot != -1);	// This should never happen or we'll write past the end of the descriptor heap.
	for (uint32 i = 0; i < UAVStartSlot; i++)
	{
		SrcDescriptors[i] = pNullUAV->GetView();
	}
	for (uint32 i = 0; i < SlotsNeeded; i++)
	{
		if (UnorderedAccessViewArray[i] != NULL)
		{
			FD3D12DynamicRHI::TransitionResource(CommandList, UnorderedAccessViewArray[i], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			SrcDescriptors[i + UAVStartSlot] = UnorderedAccessViewArray[i]->GetView();
		}
		else
		{
			SrcDescriptors[i + UAVStartSlot] = pNullUAV->GetView();
		}
	}

	// Gather the descriptors from the offline heaps to the online heap
	uint32 NumCopySlots = SlotsNeeded + UAVStartSlot;
	GetParentDevice()->GetDevice()->CopyDescriptors(
		1, &DestDescriptor, &NumCopySlots,
		NumCopySlots, SrcDescriptors, nullptr /* sizes */,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	if (ShaderStage == SF_Pixel)
	{
		const uint32 RDTIndex = CmdContext->StateCache.GetGraphicsRootSignature()->UAVRDTBindSlot(ShaderStage);
		CommandList->SetGraphicsRootDescriptorTable(RDTIndex, BindDescriptor);
	}
	else if (ShaderStage == SF_Compute)
	{
		const uint32 RDTIndex = CmdContext->StateCache.GetComputeRootSignature()->UAVRDTBindSlot(ShaderStage);
		CommandList->SetComputeRootDescriptorTable(RDTIndex, BindDescriptor);
	}
	else
	{
		check(false);
	}

#if SUPPORTS_MEMORY_RESIDENCY
	for (uint32 i = 0; i < SlotsNeeded; i++)
	{
		if (UnorderedAccessViewArray[i] != NULL)
		{
			UnorderedAccessViewArray[i]->GetResource()->UpdateResidency();
		}
	}
#endif

#ifdef VERBOSE_DESCRIPTOR_HEAP_DEBUG
	FMsg::Logf(__FILE__, __LINE__, TEXT("DescriptorCache"), ELogVerbosity::Log, TEXT("SetUnorderedAccessViewTable [STAGE %d] to slots %d - %d"), (int32)ShaderStage, FirstSlotIndex, FirstSlotIndex + SlotsNeeded - 1);
#endif

}

void FD3D12DescriptorCache::SetRenderTargets(FD3D12RenderTargetView** RenderTargetViewArray, uint32 Count, FD3D12DepthStencilView* DepthStencilTarget, bool bDepthIsBoundAsSRV)
{
	// NOTE: For this function, setting zero render targets might not be a no-op, since this is also used
	//       sometimes for only setting a depth stencil.

	D3D12_CPU_DESCRIPTOR_HANDLE RTVDescriptors[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];

	FD3D12CommandListHandle& CommandList = CmdContext->CommandListHandle;

	// Fill heap slots
	for (uint32 i = 0; i < Count; i++)
	{
		if (RenderTargetViewArray[i] != NULL)
		{
			// RTV should already be in the correct state. It is transitioned in RHISetRenderTargets.
			FD3D12DynamicRHI::TransitionResource(CommandList, RenderTargetViewArray[i], D3D12_RESOURCE_STATE_RENDER_TARGET);
			RTVDescriptors[i] = RenderTargetViewArray[i]->GetView();
		}
		else
		{
			RTVDescriptors[i] = pNullRTV->GetView();
		}
	}

	if (DepthStencilTarget != NULL)
	{
		if (bDepthIsBoundAsSRV)
		{
			// Depth is also bound as an SRV. Need to add some SRV state to the depth plane, so transition the depth and stencil separately.
			const D3D12_DEPTH_STENCIL_VIEW_DESC &desc = DepthStencilTarget->GetDesc();
			check(desc.Flags & D3D12_DSV_FLAG_READ_ONLY_DEPTH);
			const D3D12_RESOURCE_STATES depthState = D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
			FD3D12DynamicRHI::TransitionResource(CommandList, DepthStencilTarget->GetResource(), depthState, DepthStencilTarget->GetDepthOnlyViewSubresourceSubset());

			if (DepthStencilTarget->HasStencil())
			{
				const D3D12_RESOURCE_STATES stencilState = (desc.Flags & D3D12_DSV_FLAG_READ_ONLY_STENCIL) ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;
				FD3D12DynamicRHI::TransitionResource(CommandList, DepthStencilTarget->GetResource(), stencilState, DepthStencilTarget->GetStencilOnlyViewSubresourceSubset());
			}
		}
		else
		{
			check(!bDepthIsBoundAsSRV);
			FD3D12DynamicRHI::TransitionResource(CommandList, DepthStencilTarget);
		}

		D3D12_CPU_DESCRIPTOR_HANDLE DSVDescriptor = DepthStencilTarget->GetView();
		CommandList->OMSetRenderTargets(Count, RTVDescriptors, 0, &DSVDescriptor);
	}
	else
	{
		CommandList->OMSetRenderTargets(Count, RTVDescriptors, 0, nullptr);
	}
}

void FD3D12DescriptorCache::SetStreamOutTargets(FD3D12Resource** Buffers, uint32 Count, const uint32* Offsets)
{
	// Determine how many slots are really needed, since the Count passed in is a pre-defined maximum
	uint32 SlotsNeeded = 0;
	for (int32 i = Count - 1; i >= 0; i--)
	{
		if (Buffers[i] != NULL)
		{
			SlotsNeeded = i + 1;
			break;
		}
	}

	if (0 == SlotsNeeded)
	{
		return; // No-op
	}

	D3D12_STREAM_OUTPUT_BUFFER_VIEW SOViews[D3D12_SO_BUFFER_SLOT_COUNT] = { 0 };

	FD3D12CommandListHandle& CommandList = CmdContext->CommandListHandle;

	// Fill heap slots
	for (uint32 i = 0; i < SlotsNeeded; i++)
	{
		ID3D12Resource* StreamOutBuffer = Buffers[i] ? Buffers[i]->GetResource() : nullptr;

		D3D12_STREAM_OUTPUT_BUFFER_VIEW &currentView = SOViews[i];
		currentView.BufferLocation = (StreamOutBuffer != nullptr) ? StreamOutBuffer->GetGPUVirtualAddress() : 0;

		// MS - The following view members are not correct
		check(0);
		currentView.BufferFilledSizeLocation = 0;
		currentView.SizeInBytes = -1;

		if (Buffers[i] != nullptr)
		{
			FD3D12DynamicRHI::TransitionResource(CommandList, Buffers[i], D3D12_RESOURCE_STATE_STREAM_OUT, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		}
	}

	CommandList->SOSetTargets(0, SlotsNeeded, SOViews);
}

void FD3D12DescriptorCache::SetSamplers(EShaderFrequency ShaderStage, FD3D12SamplerState** Samplers, uint32 SlotsNeeded, uint32 &HeapSlot)
{
	check(CurrentSamplerHeap != &GetParentDevice()->GetGlobalSamplerHeap());
	check(bUsingGlobalSamplerHeap == false);

	if (0 == SlotsNeeded)
	{
		return; // No-op
	}

	D3D12_GPU_DESCRIPTOR_HANDLE BindDescriptor = { 0 };
	bool CacheHit = false;

	// Check to see if the sampler configuration is already in the sampler heap
	FD3D12SamplerArrayDesc Desc = {};
	if (SlotsNeeded <= _countof(Desc.SamplerID))
	{
		Desc.Count = SlotsNeeded;

		for (uint32 i = 0; i < SlotsNeeded; i++)
		{
			Desc.SamplerID[i] = Samplers[i] ? Samplers[i]->ID : 0;
		}

		// The hash uses all of the bits
		for (uint32 i = SlotsNeeded; i < _countof(Desc.SamplerID); i++)
		{
			Desc.SamplerID[i] = 0;
		}

		D3D12_GPU_DESCRIPTOR_HANDLE* FoundDescriptor = SamplerMap.Find(Desc);

		if (FoundDescriptor)
		{
			BindDescriptor = *FoundDescriptor;
			CacheHit = true;
		}
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE SrcDescriptors[D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT];

	if (!CacheHit)
	{
		// Reserve heap slots
		uint32 FirstSlotIndex = HeapSlot;
		HeapSlot += SlotsNeeded;
		D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor = CurrentSamplerHeap->GetCPUSlotHandle(FirstSlotIndex);
		BindDescriptor = CurrentSamplerHeap->GetGPUSlotHandle(FirstSlotIndex);

		// Fill heap slots
		for (uint32 i = 0; i < SlotsNeeded; i++)
		{
			if (Samplers[i] != NULL)
			{
				SrcDescriptors[i] = Samplers[i]->Descriptor;
			}
			else
			{
				SrcDescriptors[i] = pDefaultSampler->Descriptor;
			}
		}

		GetParentDevice()->GetDevice()->CopyDescriptors(
			1, &DestDescriptor, &SlotsNeeded,
			SlotsNeeded, SrcDescriptors, nullptr /* sizes */,
			D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

		// Remember the locations of the samplers in the sampler map
		if (SlotsNeeded <= _countof(Desc.SamplerID))
		{
			UniqueTables.Add(FD3D12UniqueSamplerTable(Desc, SrcDescriptors));

			SamplerMap.Add(Desc, BindDescriptor);
		}
	}

	FD3D12CommandListHandle& CommandList = CmdContext->CommandListHandle;

	if (ShaderStage == SF_Compute)
	{
		const uint32 RDTIndex = CmdContext->StateCache.GetComputeRootSignature()->SamplerRDTBindSlot(ShaderStage);
		CommandList->SetComputeRootDescriptorTable(RDTIndex, BindDescriptor);
	}
	else
	{
		const uint32 RDTIndex = CmdContext->StateCache.GetGraphicsRootSignature()->SamplerRDTBindSlot(ShaderStage);
		CommandList->SetGraphicsRootDescriptorTable(RDTIndex, BindDescriptor);
	}

#ifdef VERBOSE_DESCRIPTOR_HEAP_DEBUG
	FMsg::Logf(__FILE__, __LINE__, TEXT("DescriptorCache"), ELogVerbosity::Log, TEXT("SetSamplerTable [STAGE %d] to slots %d - %d"), (int32)ShaderStage, FirstSlotIndex, FirstSlotIndex + SlotsNeeded - 1);
#endif
}

void FD3D12DescriptorCache::SetSRVs(EShaderFrequency ShaderStage, TRefCountPtr<FD3D12ShaderResourceView>* SRVs, uint32 SlotsNeeded, bool* CurrentShaderResourceViewsIntersectWithDepthRT, uint32 &HeapSlot)
{
	if (0 == SlotsNeeded)
	{
		return; // No-op
	}

	// Check hash table for hit

	D3D12_GPU_DESCRIPTOR_HANDLE BindDescriptor = { 0 };

	FD3D12CommandListHandle& CommandList = CmdContext->CommandListHandle;

	D3D12_RESOURCE_STATES ResourceStateFlag = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	// Check to see if the srv configuration is already in the srv heap
	FD3D12SRVArrayDesc Desc = {0};
	check(SlotsNeeded <= _countof(Desc.SRVSequenceNumber));
	Desc.Count = SlotsNeeded;

	for (uint32 i = 0; i < SlotsNeeded; i++)
	{
		if (SRVs[i] != NULL)
		{
			Desc.SRVSequenceNumber[i] = SRVs[i]->GetSequenceNumber();

			if (CurrentShaderResourceViewsIntersectWithDepthRT[i] == true)
			{
				FD3D12DynamicRHI::TransitionResource(CommandList, SRVs[i], ResourceStateFlag | D3D12_RESOURCE_STATE_DEPTH_READ);
			}
			else
			{
				FD3D12DynamicRHI::TransitionResource(CommandList, SRVs[i], ResourceStateFlag);
			}
		}
	}

	D3D12_GPU_DESCRIPTOR_HANDLE* FoundDescriptor = SRVMap.Find(Desc);

	if (FoundDescriptor)
	{
		BindDescriptor = *FoundDescriptor;
	}
	else
	{
		// Reserve heap slots
		uint32 FirstSlotIndex = HeapSlot;
		HeapSlot += SlotsNeeded;

		TArray<uint64>& CurrentSequenceNumber = OfflineHeap[ShaderStage].CurrentSRVSequenceNumber;
		CD3DX12_CPU_DESCRIPTOR_HANDLE OfflineBase = OfflineHeap[ShaderStage].SRVBaseHandle;

		uint32 DescriptorSize = CurrentViewHeap->GetDescriptorSize();
		ID3D12Device *pDevice = GetParentDevice()->GetDevice();

		// Copy to offline heap (only those descriptors which have changed)
		CD3DX12_CPU_DESCRIPTOR_HANDLE OfflineDescriptor = OfflineBase;

		for (uint32 i = 0; i < SlotsNeeded; i++)
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE SrcDescriptor;
			uint64 SequenceNumber = 0;

			if (SRVs[i] != NULL)
			{
				SequenceNumber = SRVs[i]->GetSequenceNumber();
				SrcDescriptor = SRVs[i]->GetView();
			}
			else
			{
				SrcDescriptor = pNullSRV->GetView();
			}
			check(SrcDescriptor.ptr != 0);

			if (SequenceNumber != CurrentSequenceNumber[i])
			{
				pDevice->CopyDescriptorsSimple(1, OfflineDescriptor, SrcDescriptor, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				CurrentSequenceNumber[i] = SequenceNumber;
			}

			OfflineDescriptor.Offset(DescriptorSize);
		}

		// Copy from offline heap to online heap
		D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor = CurrentViewHeap->GetCPUSlotHandle(FirstSlotIndex);

		// Copy the descriptors from the offline heaps to the online heap
		pDevice->CopyDescriptorsSimple(SlotsNeeded, DestDescriptor, OfflineBase, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		BindDescriptor = CurrentViewHeap->GetGPUSlotHandle(FirstSlotIndex);

		// Remember the locations of the srvs in the map
		SRVMap.Add(Desc, BindDescriptor);
	}

	if (ShaderStage == SF_Compute)
	{
		const uint32 RDTIndex = CmdContext->StateCache.GetComputeRootSignature()->SRVRDTBindSlot(ShaderStage);
		CommandList->SetComputeRootDescriptorTable(RDTIndex, BindDescriptor);
	}
	else
	{
		const uint32 RDTIndex = CmdContext->StateCache.GetGraphicsRootSignature()->SRVRDTBindSlot(ShaderStage);
		CommandList->SetGraphicsRootDescriptorTable(RDTIndex, BindDescriptor);
	}

#if SUPPORTS_MEMORY_RESIDENCY
	for (uint32 i = 0; i < SlotsNeeded; i++)
	{
		if (SRVs[i] != NULL)
		{
			SRVs[i]->GetResource()->UpdateResidency();
		}
	}
#endif

#ifdef VERBOSE_DESCRIPTOR_HEAP_DEBUG
	FMsg::Logf(__FILE__, __LINE__, TEXT("DescriptorCache"), ELogVerbosity::Log, TEXT("SetShaderResourceViewTable [STAGE %d] to slots %d - %d"), (int32)ShaderStage, FirstSlotIndex, FirstSlotIndex + SlotsNeeded - 1);
#endif
}

// Updates the offline heap
void FD3D12DescriptorCache::SetConstantBuffer(
	EShaderFrequency ShaderStage,
	uint32 SlotIndex,
	FD3D12UniformBuffer* UniformBuffer
	)
{
	check(SlotIndex < D3D12_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);
	check(UniformBuffer);
	check(UniformBuffer->OfflineDescriptorHandle.ptr != 0);

	CD3DX12_CPU_DESCRIPTOR_HANDLE DestCPUHandle(OfflineHeap[ShaderStage].CBVBaseHandle, SlotIndex, CurrentViewHeap->GetDescriptorSize());

	TArray<uint64>& CurrentSequenceNumber = OfflineHeap[ShaderStage].CurrentUniformBufferSequenceNumber;

	// Only call CopyDescriptors if this is a new uniform buffer for this slot
	if (CurrentSequenceNumber[SlotIndex] != UniformBuffer->SequenceNumber)
	{
		GetParentDevice()->GetDevice()->CopyDescriptorsSimple(1, DestCPUHandle, UniformBuffer->OfflineDescriptorHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		CurrentSequenceNumber[SlotIndex] = UniformBuffer->SequenceNumber;
	}
}

void FD3D12DescriptorCache::SetConstantBuffer(
	EShaderFrequency ShaderStage,
	uint32 SlotIndex,
	ID3D12Resource* Resource,
	uint32 OffsetInBytes,
	uint32 SizeInBytes
	)
{
	check(SlotIndex < D3D12_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);
	check(Resource);

	CD3DX12_CPU_DESCRIPTOR_HANDLE DestCPUHandle(OfflineHeap[ShaderStage].CBVBaseHandle, SlotIndex, CurrentViewHeap->GetDescriptorSize());

	TArray<uint64>& CurrentSequenceNumber = OfflineHeap[ShaderStage].CurrentUniformBufferSequenceNumber;

	D3D12_CONSTANT_BUFFER_VIEW_DESC CBView;
	CBView.BufferLocation = Resource->GetGPUVirtualAddress() + OffsetInBytes;
	CBView.SizeInBytes = SizeInBytes;
	check(OffsetInBytes % 256 == 0);
	check(CBView.SizeInBytes <= 4096 * 16);
	check(CBView.SizeInBytes % 256 == 0);

	GetParentDevice()->GetDevice()->CreateConstantBufferView(&CBView, DestCPUHandle);

	CurrentSequenceNumber[SlotIndex] = 0;
}

void FD3D12DescriptorCache::ClearConstantBuffer(
	EShaderFrequency ShaderStage,
	uint32 SlotIndex
	)
{
	check(SlotIndex < D3D12_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);

	CD3DX12_CPU_DESCRIPTOR_HANDLE DestCPUHandle(OfflineHeap[ShaderStage].CBVBaseHandle, SlotIndex, CurrentViewHeap->GetDescriptorSize());

	TArray<uint64>& CurrentSequenceNumber = OfflineHeap[ShaderStage].CurrentUniformBufferSequenceNumber;

	// Only call CopyDescriptors if this is a new uniform buffer for this slot
	if (CurrentSequenceNumber[SlotIndex] != 0)
	{
		GetParentDevice()->GetDevice()->CopyDescriptorsSimple(1, DestCPUHandle, pNullCBV, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		CurrentSequenceNumber[SlotIndex] = 0;
	}
}

void FD3D12DescriptorCache::SetConstantBuffers(EShaderFrequency ShaderStage, uint32 SlotsNeeded, uint32 &HeapSlot)
{
	if (0 == SlotsNeeded)
	{
		return; // No-op
	}

	// Reserve heap slots
	uint32 FirstSlotIndex = HeapSlot;
	HeapSlot += SlotsNeeded;

	// Copy from offline heap to online heap

	D3D12_CPU_DESCRIPTOR_HANDLE OnlineHeapHandle = CurrentViewHeap->GetCPUSlotHandle(FirstSlotIndex);

	D3D12_CPU_DESCRIPTOR_HANDLE OfflineHeapHandle = OfflineHeap[ShaderStage].CBVBaseHandle;

	GetParentDevice()->GetDevice()->CopyDescriptorsSimple(SlotsNeeded, OnlineHeapHandle, OfflineHeapHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_GPU_DESCRIPTOR_HANDLE BindDescriptor = CurrentViewHeap->GetGPUSlotHandle(FirstSlotIndex);

	FD3D12CommandListHandle& CommandList = CmdContext->CommandListHandle;

	if (ShaderStage == SF_Compute)
	{
		const uint32 RDTIndex = CmdContext->StateCache.GetComputeRootSignature()->CBVRDTBindSlot(ShaderStage);
		CommandList->SetComputeRootDescriptorTable(RDTIndex, BindDescriptor);
	}
	else
	{
		const uint32 RDTIndex = CmdContext->StateCache.GetGraphicsRootSignature()->CBVRDTBindSlot(ShaderStage);
		CommandList->SetGraphicsRootDescriptorTable(RDTIndex, BindDescriptor);
	}

#ifdef VERBOSE_DESCRIPTOR_HEAP_DEBUG
	FMsg::Logf(__FILE__, __LINE__, TEXT("DescriptorCache"), ELogVerbosity::Log, TEXT("ConstantBufferViewTable set to slots %d - %d"), FirstSlotIndex, FirstSlotIndex + SlotsNeeded - 1);
#endif
}

void FD3D12ThreadLocalOnlineHeap::RollOver()
{
	// Enqueue the current entry
	ReclaimPool.Enqueue(Entry);

	if (ReclaimPool.Peek(Entry) && Entry.SyncPoint.IsComplete())
	{
		ReclaimPool.Dequeue(Entry);

		Heap = Entry.Heap;
	}
	else
	{
		UE_LOG(LogD3D12RHI, Warning, TEXT("OnlineHeap RollOver Detected. Increase the heap size to prevent creation of additional heaps"));
		VERIFYD3D11RESULT(GetParentDevice()->GetDevice()->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(Heap.GetInitReference())));
		SetName(Heap, Desc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ? L"Thread Local - Online View Heap" : L"Thread Local - Online Sampler Heap");

		Entry.Heap = Heap;
	}

	Entry.SyncPoint = CurrentCommandList;

	NextSlotIndex = 0;
	FirstUsedSlot = 0;

	// Notify other layers of heap change
	CPUBase = Heap->GetCPUDescriptorHandleForHeapStart();
	GPUBase = Heap->GetGPUDescriptorHandleForHeapStart();
	Parent->HeapRolledOver(Desc.Type);
}

void FD3D12ThreadLocalOnlineHeap::NotifyCurrentCommandList(const FD3D12CommandListHandle& CommandListHandle)
{
	if (CurrentCommandList != nullptr && NextSlotIndex > 0)
	{
		// Track the previous command list
		SyncPointEntry SyncPoint;
		SyncPoint.SyncPoint = CurrentCommandList;
		SyncPoint.LastSlotInUse = NextSlotIndex - 1;
		SyncPoints.Enqueue(SyncPoint);

		Entry.SyncPoint = CurrentCommandList;

		// Free up slots for finished command lists
		while (SyncPoints.Peek(SyncPoint) && SyncPoint.SyncPoint.IsComplete())
		{
			SyncPoints.Dequeue(SyncPoint);
			FirstUsedSlot = SyncPoint.LastSlotInUse + 1;
		}
	}

	// Update the current command list
	CurrentCommandList = CommandListHandle;
}

uint32 GetTypeHash(const FD3D12SamplerArrayDesc& Key)
{
	return uint32(FD3D12PipelineStateCache::HashData((void*)Key.SamplerID, Key.Count * sizeof(Key.SamplerID[0])));
}

uint32 GetTypeHash(const FD3D12SRVArrayDesc& Key)
{
	return uint32(FD3D12PipelineStateCache::HashData((void*)Key.SRVSequenceNumber, Key.Count * sizeof(Key.SRVSequenceNumber[0])));
}

uint32 GetTypeHash(const FD3D12QuantizedBoundShaderState& Key)
{
	return uint32(FD3D12PipelineStateCache::HashData((void*)&Key, sizeof(Key)));
}

void FD3D12GlobalOnlineHeap::Init(uint32 TotalSize, D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
	D3D12_DESCRIPTOR_HEAP_FLAGS HeapFlags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	Desc = {};
	Desc.Flags = HeapFlags;
	Desc.Type = Type;
	Desc.NumDescriptors = TotalSize;

	VERIFYD3D11RESULT(GetParentDevice()->GetDevice()->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(Heap.GetInitReference())));
	SetName(Heap, Desc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ? L"Device Global - Online View Heap" : L"Device Global - Online Sampler Heap");

	CPUBase = Heap->GetCPUDescriptorHandleForHeapStart();
	GPUBase = Heap->GetGPUDescriptorHandleForHeapStart();
	DescriptorSize = GetParentDevice()->GetDevice()->GetDescriptorHandleIncrementSize(Desc.Type);

	if (Desc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
	{
		// Reserve the whole heap for sub allocation
		ReserveSlots(TotalSize);
	}
}

bool FD3D12OnlineHeap::CanReserveSlots(uint32 NumSlots)
{
	const uint32 HeapSize = GetTotalSize();

	// Sanity checks
	if (0 == NumSlots)
	{
		return true;
	}
	if (NumSlots > HeapSize)
	{
		throw E_OUTOFMEMORY;
	}
	uint32 FirstRequestedSlot = NextSlotIndex;
	uint32 SlotAfterReservation = NextSlotIndex + NumSlots;

	// TEMP: Disable wrap around by not allowing it to reserve slots if the heap is full.
	if (SlotAfterReservation > HeapSize)
	{
		return false;
	}

	return true;

	// TEMP: Uncomment this code once the heap wrap around is fixed.
	//if (SlotAfterReservation <= HeapSize)
	//{
	//	return true;
	//}

	//// Try looping around to prevent rollovers
	//SlotAfterReservation = NumSlots;

	//if (SlotAfterReservation <= FirstUsedSlot)
	//{
	//	return true;
	//}

	//return false;
}

uint32 FD3D12OnlineHeap::ReserveSlots(uint32 NumSlotsRequested)
{
#ifdef VERBOSE_DESCRIPTOR_HEAP_DEBUG
	FMsg::Logf(__FILE__, __LINE__, TEXT("DescriptorCache"), ELogVerbosity::Log, TEXT("Requesting reservation [TYPE %d] with %d slots, required fence is %d"),
		(int32)Desc.Type, NumSlotsRequested, RequiredFenceForCurrentCL);
#endif

	const uint32 HeapSize = GetTotalSize();

	// Sanity checks
	if (NumSlotsRequested > HeapSize)
	{
		throw E_OUTOFMEMORY;
		return HeapExhaustedValue;
	}

	// CanReserveSlots should have been called first
	check(CanReserveSlots(NumSlotsRequested));

	// Decide which slots will be reserved and what needs to be cleaned up
	uint32 FirstRequestedSlot = NextSlotIndex;
	uint32 SlotAfterReservation = NextSlotIndex + NumSlotsRequested;

	// Loop around if the end of the heap has been reached
	if (bCanLoopAround && SlotAfterReservation > HeapSize)
	{
		FirstRequestedSlot = 0;
		SlotAfterReservation = NumSlotsRequested;

		FirstUsedSlot = SlotAfterReservation;

		Parent->HeapLoopedAround(Desc.Type);
	}

	// Note where to start looking next time
	NextSlotIndex = SlotAfterReservation;

	return FirstRequestedSlot;
}

void FD3D12GlobalOnlineHeap::RollOver()
{
	check(false);
	UE_LOG(LogD3D12RHI, Warning, TEXT("Global Descriptor heaps can't roll over!"));
}

void FD3D12OnlineHeap::SetNextSlot(uint32 NextSlot)
{
	// For samplers, ReserveSlots will be called with a conservative estimate
	// This is used to correct for the actual number of heap slots used
	check(NextSlot <= NextSlotIndex);

	NextSlotIndex = NextSlot;
}

void FD3D12ThreadLocalOnlineHeap::Init(uint32 NumDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
	Desc = {};
	Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	Desc.Type = Type;
	Desc.NumDescriptors = NumDescriptors;

	VERIFYD3D11RESULT(GetParentDevice()->GetDevice()->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(Heap.GetInitReference())));
	SetName(Heap, Desc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ? L"Thread Local - Online View Heap" : L"Thread Local - Online Sampler Heap");

	Entry.Heap = Heap;

	CPUBase = Heap->GetCPUDescriptorHandleForHeapStart();
	GPUBase = Heap->GetGPUDescriptorHandleForHeapStart();
	DescriptorSize = GetParentDevice()->GetDevice()->GetDescriptorHandleIncrementSize(Type);
}

void FD3D12OnlineHeap::NotifyCurrentCommandList(const FD3D12CommandListHandle& CommandListHandle)
{
	//Specialization should be called
	check(false);
}

uint32 FD3D12OnlineHeap::GetTotalSize()
{
	return Desc.NumDescriptors;
}

uint32 FD3D12SubAllocatedOnlineHeap::GetTotalSize()
{
	return CurrentSubAllocation.Size;
}

void FD3D12SubAllocatedOnlineHeap::Init(SubAllocationDesc _Desc)
{
	SubDesc = _Desc;

	const uint32 Blocks = SubDesc.Size / DESCRIPTOR_HEAP_BLOCK_SIZE;
	check(Blocks > 0);
	check(SubDesc.Size >= DESCRIPTOR_HEAP_BLOCK_SIZE);

	uint32 BaseSlot = SubDesc.BaseSlot;
	for (uint32 i = 0; i < Blocks; i++)
	{
		DescriptorBlockPool.Enqueue(FD3D12OnlineHeapBlock(BaseSlot, DESCRIPTOR_HEAP_BLOCK_SIZE));
		check(BaseSlot + DESCRIPTOR_HEAP_BLOCK_SIZE <= SubDesc.ParentHeap->GetTotalSize());
		BaseSlot += DESCRIPTOR_HEAP_BLOCK_SIZE;
	}

	Heap = SubDesc.ParentHeap->GetHeap();

	DescriptorSize = SubDesc.ParentHeap->GetDescriptorSize();
	Desc = SubDesc.ParentHeap->GetDesc();

	DescriptorBlockPool.Dequeue(CurrentSubAllocation);

	CPUBase = SubDesc.ParentHeap->GetCPUSlotHandle(CurrentSubAllocation.BaseSlot);
	GPUBase = SubDesc.ParentHeap->GetGPUSlotHandle(CurrentSubAllocation.BaseSlot);
}

void FD3D12SubAllocatedOnlineHeap::RollOver()
{
	// Enqueue the current entry
	CurrentSubAllocation.SyncPoint = CurrentCommandList;
	CurrentSubAllocation.bFresh = false;
	DescriptorBlockPool.Enqueue(CurrentSubAllocation);

	if (DescriptorBlockPool.Peek(CurrentSubAllocation) && 
		(CurrentSubAllocation.bFresh|| CurrentSubAllocation.SyncPoint.IsComplete()))
	{
		DescriptorBlockPool.Dequeue(CurrentSubAllocation);
	}
	else
	{
		//Notify parent that we have run out of sub allocations
		//This should *never* happen but we will handle it and revert to local heaps to be safe
		UE_LOG(LogD3D12RHI, Warning, TEXT("Descriptor cache ran out of sub allocated descriptor blocks! Moving to Context local View heap strategy"));
		Parent->SwitchToContextLocalViewHeap();
		return;
	}

	NextSlotIndex = 0;
	FirstUsedSlot = 0;

	// Notify other layers of heap change
	CPUBase = SubDesc.ParentHeap->GetCPUSlotHandle(CurrentSubAllocation.BaseSlot);
	GPUBase = SubDesc.ParentHeap->GetGPUSlotHandle(CurrentSubAllocation.BaseSlot);
	Parent->HeapRolledOver(Desc.Type);
}

void FD3D12SubAllocatedOnlineHeap::NotifyCurrentCommandList(const FD3D12CommandListHandle& CommandListHandle)
{
	// Update the current command list
	CurrentCommandList = CommandListHandle;
}

void FD3D12DescriptorCache::SwitchToContextLocalViewHeap()
{
	if (LocalViewHeap == nullptr)
	{
		UE_LOG(LogD3D12RHI, Warning, TEXT("This should only happen in the Editor where it doesn't matter as much. If it happens in game you should increase the device global heap size!"));
		//Allocate the heap lazily
		LocalViewHeap = new FD3D12ThreadLocalOnlineHeap(GetParentDevice(), this);
		if (LocalViewHeap)
		{
			check(NumLocalViewDescriptors);
			LocalViewHeap->Init(NumLocalViewDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
		else
		{
			check(false);
			return;
		}
	}

	CurrentViewHeap = LocalViewHeap;

	// Reset State as appropriate
	HeapRolledOver(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void FD3D12DescriptorCache::SwitchToContextLocalSamplerHeap()
{
	DisableGlobalSamplerHeap();

	CurrentSamplerHeap = &LocalSamplerHeap;

	TArray<ID3D12DescriptorHeap*> Heaps;
	Heaps.Add(GetViewDescriptorHeap());
	Heaps.Add(GetSamplerDescriptorHeap());
	CmdContext->SetDescriptorHeaps(Heaps);
}

void FD3D12DescriptorCache::SwitchToGlobalSamplerHeap()
{
	bUsingGlobalSamplerHeap = true;

	CurrentSamplerHeap = &GetParentDevice()->GetGlobalSamplerHeap();

	TArray<ID3D12DescriptorHeap*> Heaps;
	Heaps.Add(GetViewDescriptorHeap());
	Heaps.Add(GetSamplerDescriptorHeap());
	CmdContext->SetDescriptorHeaps(Heaps);
}

FD3D12PipelineState::~FD3D12PipelineState()
{
	if (Worker)
	{
		Worker->EnsureCompletion(true);
		delete(Worker);
		Worker = nullptr;
	}
}

void FD3D12PipelineState::Create(FD3D12ComputePipelineStateDesc* Desc)
{
	VERIFYD3D11RESULT(GetParentDevice()->GetDevice()->CreateComputePipelineState(&Desc->Desc, IID_PPV_ARGS(PipelineState.GetInitReference())));
}

void FD3D12PipelineState::CreateAsync(FD3D12ComputePipelineStateDesc* Desc)
{
	Worker = new FAsyncTask<FD3D12PipelineStateWorker>(GetParentDevice(), &Desc->Desc);
	if (Worker)
	{
		Worker->StartBackgroundTask();
	}
}

void FD3D12PipelineState::Create(FD3D12LowLevelGraphicsPipelineStateDesc* Desc)
{
	VERIFYD3D11RESULT(GetParentDevice()->GetDevice()->CreateGraphicsPipelineState(&Desc->Desc, IID_PPV_ARGS(PipelineState.GetInitReference())));
}

void FD3D12PipelineState::CreateAsync(FD3D12LowLevelGraphicsPipelineStateDesc* Desc)
{
	Worker = new FAsyncTask<FD3D12PipelineStateWorker>(GetParentDevice(), &Desc->Desc);
	if (Worker)
	{
		Worker->StartBackgroundTask();
	}
}

ID3D12PipelineState* FD3D12PipelineState::GetPipelineState()
{
	if (Worker)
	{
		Worker->EnsureCompletion(true);

		PipelineState = Worker->GetTask().PSO;

		delete(Worker);
		Worker = nullptr;
	}

	return PipelineState.GetReference();
}

void FD3D12PipelineStateWorker::DoWork()
{
	if (bIsGraphics)
	{
		VERIFYD3D11RESULT(GetParentDevice()->GetDevice()->CreateGraphicsPipelineState(&Desc.GraphicsDesc, IID_PPV_ARGS(PSO.GetInitReference())));
	}
	else
	{
		VERIFYD3D11RESULT(GetParentDevice()->GetDevice()->CreateComputePipelineState(&Desc.ComputeDesc, IID_PPV_ARGS(PSO.GetInitReference())));
	}
}

void FD3D12ResourceBlockInfo::Destroy()
{
	ResourceHeap->Release();
	ResourceHeap = nullptr;
}

FD3D12FastAllocatorPage* FD3D12FastAllocatorPagePool::RequestFastAllocatorPage()
{
	return RequestFastAllocatorPageInternal();
}

void FD3D12FastAllocatorPagePool::ReturnFastAllocatorPage(FD3D12FastAllocatorPage* Page)
{
	return ReturnFastAllocatorPageInternal(Page);
}

void FD3D12FastAllocatorPagePool::CleanUpPages(uint64 frameLag, bool force)
{
	return CleanUpPagesInternal(frameLag, force);
}

FD3D12FastAllocatorPage* FD3D12ThreadSafeFastAllocatorPagePool::RequestFastAllocatorPage()
{
	FScopeLock Lock(&CS);
	return RequestFastAllocatorPageInternal();
}

void FD3D12ThreadSafeFastAllocatorPagePool::ReturnFastAllocatorPage(FD3D12FastAllocatorPage* Page)
{
	FScopeLock Lock(&CS);
	return ReturnFastAllocatorPageInternal(Page);
}

void FD3D12ThreadSafeFastAllocatorPagePool::CleanUpPages(uint64 frameLag, bool force)
{
	FScopeLock Lock(&CS);
	return CleanUpPagesInternal(frameLag, force);
}

void FD3D12FastAllocatorPagePool::Destroy()
{
	for (int32 i = 0; i <  Pool.Num(); i++)
	{
		check(Pool[i]->FastAllocBuffer->GetRefCount() == 1);
		{
			FD3D12FastAllocatorPage *Page = Pool[i];
			delete(Page);
		}
	}

	Pool.Empty();
}

FD3D12FastAllocatorPage* FD3D12FastAllocatorPagePool::RequestFastAllocatorPageInternal()
{
	FD3D12FastAllocatorPage* Page = nullptr;

	uint64 currentFence = GetParentDevice()->GetCommandListManager().GetFence(EFenceType::FT_Frame).GetLastCompletedFence();

	for (int32 Index = 0; Index < Pool.Num(); Index++)
	{
		//If the GPU is done with it and no-one has a lock on it
		if (Pool[Index]->FastAllocBuffer->GetRefCount() == 1 &&
			Pool[Index]->FrameFence <= currentFence)
		{
			Page = Pool[Index];
			Page->Reset();
			Pool.RemoveAt(Index);
			return Page;
		}
	}

	check(Page == nullptr);
	Page = new FD3D12FastAllocatorPage(PageSize);

	VERIFYD3D11RESULT(GetParentDevice()->GetResourceHelper().CreateBuffer(HeapType, PageSize, Page->FastAllocBuffer.GetInitReference(), D3D12_RESOURCE_FLAG_NONE, &HeapProperties));
	SetName(Page->FastAllocBuffer, L"Fast Allocator Page");

	VERIFYD3D11RESULT(Page->FastAllocBuffer->GetResource()->Map(0, nullptr, &Page->FastAllocData));
	return Page;
}

void FD3D12FastAllocatorPagePool::ReturnFastAllocatorPageInternal(FD3D12FastAllocatorPage* Page)
{
	Page->FrameFence = GetParentDevice()->GetCommandListManager().GetFence(EFenceType::FT_Frame).GetCurrentFence();
	Pool.Add(Page);
}

void FD3D12FastAllocatorPagePool::CleanUpPagesInternal(uint64 frameLag, bool force)
{
	uint64 currentFence = GetParentDevice()->GetCommandListManager().GetFence(EFenceType::FT_Frame).GetLastCompletedFence();
	
	FD3D12FastAllocatorPage* Page = nullptr;
	
	int32 Index = Pool.Num() - 1;
	while (Index >= 0)
	{
		//If the GPU is done with it and no-one has a lock on it
		if (Pool[Index]->FastAllocBuffer->GetRefCount() == 1 &&
			Pool[Index]->FrameFence + frameLag <= currentFence)
		{
			Page = Pool[Index];
			Pool.RemoveAt(Index);
			delete(Page);
		}
	
		Index--;
	}
}