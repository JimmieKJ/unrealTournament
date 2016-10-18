// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//-----------------------------------------------------------------------------
//	Include Files
//-----------------------------------------------------------------------------
#include "D3D12RHIPrivate.h"

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

		VERIFYD3D12RESULT(GetParentDevice()->GetDevice()->CreateDescriptorHeap(
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

void FD3D12DescriptorCache::SetIndexBuffer(FD3D12IndexBufferCache& Cache)
{
	CmdContext->CommandListHandle.UpdateResidency(Cache.ResidencyHandle);
	CmdContext->CommandListHandle->IASetIndexBuffer(&Cache.CurrentIndexBufferView);
}

void FD3D12DescriptorCache::SetVertexBuffers(FD3D12VertexBufferCache& Cache)
{
	const uint32 Count = Cache.MaxBoundVertexBufferIndex + 1;
	if (Count == 0)
	{
		return; // No-op
	}

	CmdContext->CommandListHandle.UpdateResidency(Cache.ResidencyHandles, Count);
	CmdContext->CommandListHandle->IASetVertexBuffers(0, Count, Cache.CurrentVertexBufferViews);
}

void FD3D12DescriptorCache::SetUAVs(EShaderFrequency ShaderStage, uint32 UAVStartSlot, TRefCountPtr<FD3D12UnorderedAccessView>* UnorderedAccessViewArray, uint32 SlotsNeeded, uint32 &HeapSlot)
{
	if (0 == SlotsNeeded)
	{
		return; // No-op
	}

	// Reserve heap slots
	// Note: SlotsNeeded already accounts for the UAVStartSlot. For example, if a shader has 4 UAVs starting at slot 2 then SlotsNeeded will be 6 (because the root descriptor table currently starts at slot 0).
	uint32 FirstSlotIndex = HeapSlot;
	HeapSlot += SlotsNeeded;
	CD3DX12_CPU_DESCRIPTOR_HANDLE DestDescriptor(CurrentViewHeap->GetCPUSlotHandle(FirstSlotIndex));
	CD3DX12_GPU_DESCRIPTOR_HANDLE BindDescriptor(CurrentViewHeap->GetGPUSlotHandle(FirstSlotIndex));

	CD3DX12_CPU_DESCRIPTOR_HANDLE SrcDescriptors[D3D12_PS_CS_UAV_REGISTER_COUNT];

	FD3D12CommandListHandle& CommandList = CmdContext->CommandListHandle;

	// Fill heap slots
	check(UAVStartSlot != -1);	// This should never happen or we'll write past the end of the descriptor heap.
	check(UAVStartSlot < D3D12_PS_CS_UAV_REGISTER_COUNT);
	for (uint32 i = 0; i < SlotsNeeded; i++)
	{
		if ((i < UAVStartSlot) || (UnorderedAccessViewArray[i] == nullptr))
		{
			SrcDescriptors[i] = pNullUAV->GetView();
		}
		else
		{
			FD3D12DynamicRHI::TransitionResource(CommandList, UnorderedAccessViewArray[i], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			SrcDescriptors[i] = UnorderedAccessViewArray[i]->GetView();
			CommandList.UpdateResidency(UnorderedAccessViewArray[i]->GetResource());
		}
	}

	// Gather the descriptors from the offline heaps to the online heap
	GetParentDevice()->GetDevice()->CopyDescriptors(
		1, &DestDescriptor, &SlotsNeeded,
		SlotsNeeded, SrcDescriptors, nullptr /* sizes */,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	if (ShaderStage == SF_Pixel)
	{
		const uint32 RDTIndex = CmdContext->StateCache.GetGraphicsRootSignature()->UAVRDTBindSlot(ShaderStage);
		CommandList->SetGraphicsRootDescriptorTable(RDTIndex, BindDescriptor);
	}
	else
	{
		check(ShaderStage == SF_Compute);
		const uint32 RDTIndex = CmdContext->StateCache.GetComputeRootSignature()->UAVRDTBindSlot(ShaderStage);
		CommandList->SetComputeRootDescriptorTable(RDTIndex, BindDescriptor);
	}

#ifdef VERBOSE_DESCRIPTOR_HEAP_DEBUG
	FMsg::Logf(__FILE__, __LINE__, TEXT("DescriptorCache"), ELogVerbosity::Log, TEXT("SetUnorderedAccessViewTable [STAGE %d] to slots %d - %d"), (int32)ShaderStage, FirstSlotIndex, FirstSlotIndex + SlotsNeeded - 1);
#endif

}

void FD3D12DescriptorCache::SetRenderTargets(FD3D12RenderTargetView** RenderTargetViewArray, uint32 Count, FD3D12DepthStencilView* DepthStencilTarget)
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

			CommandList.UpdateResidency(RenderTargetViewArray[i]->GetResource());
		}
		else
		{
			RTVDescriptors[i] = pNullRTV->GetView();
		}
	}

	if (DepthStencilTarget != nullptr)
	{
		FD3D12DynamicRHI::TransitionResource(CommandList, DepthStencilTarget);

		const D3D12_CPU_DESCRIPTOR_HANDLE DSVDescriptor = DepthStencilTarget->GetView();
		CommandList->OMSetRenderTargets(Count, RTVDescriptors, 0, &DSVDescriptor);
		CommandList.UpdateResidency(DepthStencilTarget->GetResource());
	}
	else
	{
		CA_SUPPRESS(6001);
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
		if (Buffers[i])
		{
			CommandList.UpdateResidency(Buffers[i]);
		}

		D3D12_STREAM_OUTPUT_BUFFER_VIEW &currentView = SOViews[i];
		currentView.BufferLocation = (Buffers[i] != nullptr) ? Buffers[i]->GetGPUVirtualAddress() : 0;

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

	if (!CacheHit)
	{
		// Reserve heap slots
		uint32 FirstSlotIndex = HeapSlot;
		HeapSlot += SlotsNeeded;
		D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor = CurrentSamplerHeap->GetCPUSlotHandle(FirstSlotIndex);
		BindDescriptor = CurrentSamplerHeap->GetGPUSlotHandle(FirstSlotIndex);

		checkSlow(SlotsNeeded < D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT);

		CD3DX12_CPU_DESCRIPTOR_HANDLE SrcDescriptors[D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT];
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

void FD3D12DescriptorCache::SetSRVs(EShaderFrequency ShaderStage, FD3D12ShaderResourceViewCache& Cache, uint32 SlotsNeeded, uint32 &HeapSlot)
{
	if (0 == SlotsNeeded)
	{
		return; // No-op
	}

	FD3D12CommandListHandle& CommandList = CmdContext->CommandListHandle;

	auto& SRVs = Cache.Views[ShaderStage];

	// Check to see if the srv configuration is already in the srv heap
	FD3D12SRVArrayDesc Desc = { 0 };
	check(SlotsNeeded <= _countof(Desc.SRVSequenceNumber));
	Desc.Count = SlotsNeeded;

	for (uint32 i = 0; i < SlotsNeeded; i++)
	{
		if (SRVs[i] != NULL)
		{
			Desc.SRVSequenceNumber[i] = SRVs[i]->GetSequenceNumber();
			CommandList.UpdateResidency(Cache.ResidencyHandles[ShaderStage][i]);

			if (SRVs[i]->IsDepthStencilResource())
			{
				FD3D12DynamicRHI::TransitionResource(CommandList, SRVs[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ);
			}
			else
			{
				FD3D12DynamicRHI::TransitionResource(CommandList, SRVs[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			}
		}
	}

	// Check hash table for hit
	D3D12_GPU_DESCRIPTOR_HANDLE BindDescriptor = { 0 };
	const D3D12_GPU_DESCRIPTOR_HANDLE* FoundDescriptor = SRVMap.Find(Desc);

	if (FoundDescriptor)
	{
		BindDescriptor = *FoundDescriptor;
	}
	else
	{
		TArray<uint64>& CurrentSequenceNumber = OfflineHeap[ShaderStage].CurrentSRVSequenceNumber;
		const CD3DX12_CPU_DESCRIPTOR_HANDLE OfflineBase = OfflineHeap[ShaderStage].SRVBaseHandle;

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

			OfflineDescriptor.Offset(CurrentViewHeap->GetDescriptorSize());
		}

		// Copy from offline heap to online heap
		{
			// Reserve heap slots
			uint32 FirstSlotIndex = HeapSlot;
			HeapSlot += SlotsNeeded;

			D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor = CurrentViewHeap->GetCPUSlotHandle(FirstSlotIndex);

			// Copy the descriptors from the offline heaps to the online heap
			pDevice->CopyDescriptorsSimple(SlotsNeeded, DestDescriptor, OfflineBase, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			BindDescriptor = CurrentViewHeap->GetGPUSlotHandle(FirstSlotIndex);

			// Remember the locations of the srvs in the map
			SRVMap.Add(Desc, BindDescriptor);
		}
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

#ifdef VERBOSE_DESCRIPTOR_HEAP_DEBUG
	FMsg::Logf(__FILE__, __LINE__, TEXT("DescriptorCache"), ELogVerbosity::Log, TEXT("SetShaderResourceViewTable [STAGE %d] to slots %d - %d"), (int32)ShaderStage, FirstSlotIndex, FirstSlotIndex + SlotsNeeded - 1);
#endif
}

// Updates the offline heap
void FD3D12DescriptorCache::SetConstantBuffer(EShaderFrequency ShaderStage, uint32 SlotIndex, FD3D12UniformBuffer* UniformBuffer)
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

void FD3D12DescriptorCache::SetConstantBuffer(EShaderFrequency ShaderStage, uint32 SlotIndex, FD3D12ResourceLocation* ResourceLocation, uint32 SizeInBytes)
{
	check(SlotIndex < D3D12_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);
	check(ResourceLocation);

	CD3DX12_CPU_DESCRIPTOR_HANDLE DestCPUHandle(OfflineHeap[ShaderStage].CBVBaseHandle, SlotIndex, CurrentViewHeap->GetDescriptorSize());

	TArray<uint64>& CurrentSequenceNumber = OfflineHeap[ShaderStage].CurrentUniformBufferSequenceNumber;

	D3D12_CONSTANT_BUFFER_VIEW_DESC CBView;
	CBView.BufferLocation = ResourceLocation->GetGPUVirtualAddress();
	CBView.SizeInBytes = SizeInBytes;
	check(ResourceLocation->GetOffset() % 256 == 0);
	check(CBView.SizeInBytes <= 4096 * 16);
	check(CBView.SizeInBytes % 256 == 0);

	GetParentDevice()->GetDevice()->CreateConstantBufferView(&CBView, DestCPUHandle);

	CurrentSequenceNumber[SlotIndex] = 0;
}

void FD3D12DescriptorCache::ClearConstantBuffer(EShaderFrequency ShaderStage, uint32 SlotIndex)
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Descriptor Heaps
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
		VERIFYD3D12RESULT(GetParentDevice()->GetDevice()->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(Heap.GetInitReference())));
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

void FD3D12GlobalOnlineHeap::Init(uint32 TotalSize, D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
	D3D12_DESCRIPTOR_HEAP_FLAGS HeapFlags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	Desc = {};
	Desc.Flags = HeapFlags;
	Desc.Type = Type;
	Desc.NumDescriptors = TotalSize;

	VERIFYD3D12RESULT(GetParentDevice()->GetDevice()->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(Heap.GetInitReference())));
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

	VERIFYD3D12RESULT(GetParentDevice()->GetDevice()->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(Heap.GetInitReference())));
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
		(CurrentSubAllocation.bFresh || CurrentSubAllocation.SyncPoint.IsComplete()))
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Util
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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