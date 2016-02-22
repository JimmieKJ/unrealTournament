// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Resources.cpp: D3D RHI utility implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "EngineModule.h"

FD3D12CommandAllocator::FD3D12CommandAllocator(ID3D12Device* InDevice, const D3D12_COMMAND_LIST_TYPE& InType)
{
	Init(InDevice, InType);
}

void FD3D12CommandAllocator::Init(ID3D12Device* InDevice, const D3D12_COMMAND_LIST_TYPE& InType)
{
	check(CommandAllocator.GetReference() == nullptr);
	VERIFYD3D11RESULT(InDevice->CreateCommandAllocator(InType, IID_PPV_ARGS(CommandAllocator.GetInitReference())));
}

FD3D12DeferredDeletionQueue::FD3D12DeferredDeletionQueue(FD3D12Device* InParent) :
	FD3D12DeviceChild(InParent) {}

FD3D12DeferredDeletionQueue::~FD3D12DeferredDeletionQueue()
{
	FAsyncTask<FD3D12AsyncDeletionWorker>* DeleteTask = nullptr;
	while (DeleteTasks.Peek(DeleteTask))
	{
		DeleteTasks.Dequeue(DeleteTask);
		DeleteTask->EnsureCompletion(true);
		delete(DeleteTask);
	}
}

bool FD3D12DeferredDeletionQueue::ReleaseResources(bool DeleteImmediately)
{
	if (DeleteImmediately)
	{
		FAsyncTask<FD3D12AsyncDeletionWorker>* DeleteTask = nullptr;
		// Call back all threads
		while (DeleteTasks.Peek(DeleteTask))
		{
			DeleteTasks.Dequeue(DeleteTask);
			DeleteTask->EnsureCompletion(true);
			delete(DeleteTask);
		}

		struct FDequeueFenceObject
		{
			FDequeueFenceObject(FD3D12CommandListManager* InCommandListManager)
				: CommandListManager(InCommandListManager)
			{
			}

			bool operator() (FencedObjectType FenceObject)
			{
				return CommandListManager->GetFence(FT_Frame).IsFenceComplete(FenceObject.Value);
			}

			FD3D12CommandListManager* CommandListManager;
		};

		FD3D12CommandListManager& CommandListManager = GetParentDevice()->GetCommandListManager();

		FencedObjectType FenceObject;
		FDequeueFenceObject DequeueFenceObject(&CommandListManager);

		const uint64 LastCompletedFrameFence = CommandListManager.GetFence(EFenceType::FT_Frame).GetLastCompletedFence();
		while (DeferredReleaseQueue.Dequeue(FenceObject, DequeueFenceObject))
		{
			FenceObject.Key->Release();
		}

		return DeferredReleaseQueue.IsEmpty();
	}
	else
	{
		FAsyncTask<FD3D12AsyncDeletionWorker>* DeleteTask = nullptr;
		while (DeleteTasks.Peek(DeleteTask) && DeleteTask->IsDone())
		{
			DeleteTasks.Dequeue(DeleteTask);
			delete(DeleteTask);
		}

		DeleteTask = new FAsyncTask<FD3D12AsyncDeletionWorker>(GetParentDevice(), &DeferredReleaseQueue);

		DeleteTask->StartBackgroundTask();
		DeleteTasks.Enqueue(DeleteTask);

		return false;
	}
}

FD3D12DeferredDeletionQueue::FD3D12AsyncDeletionWorker::FD3D12AsyncDeletionWorker(FD3D12Device* Device, FThreadsafeQueue<FencedObjectType>* DeletionQueue) :
	FD3D12DeviceChild(Device)
{
	struct FDequeueFenceObject
	{
		FDequeueFenceObject(FD3D12CommandListManager* InCommandListManager)
			: CommandListManager(InCommandListManager)
		{
		}

		bool operator() (FencedObjectType FenceObject)
		{
			return CommandListManager->GetFence(FT_Frame).IsFenceComplete(FenceObject.Value);
		}

		FD3D12CommandListManager* CommandListManager;
	};

	FD3D12CommandListManager& CommandListManager = GetParentDevice()->GetCommandListManager();

	FencedObjectType FenceObject;
	FDequeueFenceObject DequeueFenceObject(&CommandListManager);

	DeletionQueue->BatchDequeue(&Queue, DequeueFenceObject, 4096);
}

void FD3D12DeferredDeletionQueue::FD3D12AsyncDeletionWorker::DoWork()
{
	FencedObjectType ResourceToDelete;

	while (Queue.Dequeue(ResourceToDelete))
	{
		// TEMP: Disable check until memory cleanup issues are resolved. This should be a final release.
		//check(ResourceToDelete.Key->GetRefCount() == 1);
		ResourceToDelete.Key->Release();
	}
}

#if UE_BUILD_DEBUG
int64 FD3D12Resource::TotalResourceCount = 0;
int64 FD3D12Resource::NoStateTrackingResourceCount = 0;
#endif

FD3D12Resource::~FD3D12Resource()
{
#if SUPPORTS_MEMORY_RESIDENCY
	if (HeapType == D3D12_HEAP_TYPE_DEFAULT)
	{
		FD3D12DynamicRHI::GetD3DRHI()->GetResourceResidencyManager().ResourceFreed(this);
	}
#endif

	if (pResourceState)
	{
		delete pResourceState;
		pResourceState = nullptr;
	}
}

#if SUPPORTS_MEMORY_RESIDENCY
void FD3D12Resource::UpdateResourceSize()
{
	if (HeapType == D3D12_HEAP_TYPE_DEFAULT)
	{
		ResourceSize = (uint32)GetRequiredIntermediateSize(GetResource(), 0, Desc.MipLevels);
	}
}

void FD3D12Resource::UpdateResidency()
{
	if (HeapType == D3D12_HEAP_TYPE_DEFAULT)
	{
		FD3D12DynamicRHI::GetD3DRHI()->GetResourceResidencyManager().UpdateResidency(this);
	}
}
#endif

void inline FD3D12CommandListHandle::FD3D12CommandListData::FCommandListResourceState::ConditionalInitalize(FD3D12Resource* pResource, CResourceState& ResourceState)
{
	// If there is no entry, all subresources should be in the resource's TBD state.
	// This means we need to have pending resource barrier(s).
	if (!ResourceState.CheckResourceStateInitalized())
	{
		ResourceState.Initialize(pResource->GetSubresourceCount());
		check(ResourceState.CheckResourceState(D3D12_RESOURCE_STATE_TBD));
	}

	check(ResourceState.CheckResourceStateInitalized());
}

CResourceState& FD3D12CommandListHandle::FD3D12CommandListData::FCommandListResourceState::GetResourceState(FD3D12Resource* pResource)
{
	// Only certain resources should use this
	check(pResource->RequiresResourceStateTracking());

	CResourceState& ResourceState = ResourceStates.FindOrAdd(pResource);
	ConditionalInitalize(pResource, ResourceState);
	return ResourceState;
}

void FD3D12CommandListHandle::FD3D12CommandListData::FCommandListResourceState::Empty()
{
	ResourceStates.Empty();
}

void FD3D12CommandListHandle::Execute(bool WaitForCompletion)
{
	check(CommandListData);
	CommandListData->CommandListManager->ExecuteCommandList(*this, WaitForCompletion);
}
void FD3D12RootSignature::Init(const FD3D12QuantizedBoundShaderState& InQBSS)
{
	// Create a root signature desc from the quantized bound shader state.
	FD3D12RootSignatureDesc Desc(InQBSS);
	Init(Desc.GetDesc());
}

void FD3D12RootSignature::Init(const D3D12_ROOT_SIGNATURE_DESC& InDesc)
{
	// Serialize the desc.
	TRefCountPtr<ID3DBlob> Error;
	const HRESULT SerializeHR = D3D12SerializeRootSignature(&InDesc, D3D_ROOT_SIGNATURE_VERSION_1, RootSignatureBlob.GetInitReference(), Error.GetInitReference());
	if (Error.GetReference())
	{
		UE_LOG(LogD3D12RHI, Fatal, TEXT("D3D12SerializeRootSignature failed with error %s"), ANSI_TO_TCHAR(Error->GetBufferPointer()));
	}
	VERIFYD3D11RESULT(SerializeHR);

	// Create and analyze the root signature.
	VERIFYD3D11RESULT(GetParentDevice()->GetDevice()->CreateRootSignature(0, RootSignatureBlob->GetBufferPointer(), RootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(RootSignature.GetInitReference())));
	AnalyzeSignature(InDesc);
}

void FD3D12RootSignature::Init(ID3DBlob* const InBlob)
{
	// Save the blob
	RootSignatureBlob = InBlob;

	// Deserialize to get the desc.
	TRefCountPtr<ID3D12RootSignatureDeserializer> Deserializer;
	VERIFYD3D11RESULT(D3D12CreateRootSignatureDeserializer(RootSignatureBlob->GetBufferPointer(), RootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(Deserializer.GetInitReference())));

	// Create and analyze the root signature.
	VERIFYD3D11RESULT(GetParentDevice()->GetDevice()->CreateRootSignature(0, RootSignatureBlob->GetBufferPointer(), RootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(RootSignature.GetInitReference())));
	AnalyzeSignature(*Deserializer->GetRootSignatureDesc());
}

void FD3D12RootSignature::AnalyzeSignature(const D3D12_ROOT_SIGNATURE_DESC& Desc)
{
	// Reset members to default values.
	{
		FMemory::Memset(BindSlotMap, 0xFF, sizeof(BindSlotMap));
		bHasUAVs = false;
		bHasSRVs = false;
		bHasCBVs = false;
		bHasSamplers = false;
	}

	const bool bDenyVS = (Desc.Flags & D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS) != 0;
	const bool bDenyHS = (Desc.Flags & D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS) != 0;
	const bool bDenyDS = (Desc.Flags & D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS) != 0;
	const bool bDenyGS = (Desc.Flags & D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS) != 0;
	const bool bDenyPS = (Desc.Flags & D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS) != 0;

	// Go through each root parameter.
	for (uint32 i = 0; i < Desc.NumParameters; i++)
	{
		const D3D12_ROOT_PARAMETER& CurrentParameter = Desc.pParameters[i];

		EShaderFrequency CurrentVisibleSF = SF_NumFrequencies;
		switch (CurrentParameter.ShaderVisibility)
		{
		case D3D12_SHADER_VISIBILITY_ALL:
			CurrentVisibleSF = SF_NumFrequencies;
			break;

		case D3D12_SHADER_VISIBILITY_VERTEX:
			CurrentVisibleSF = SF_Vertex;
			break;
		case D3D12_SHADER_VISIBILITY_HULL:
			CurrentVisibleSF = SF_Hull;
			break;
		case D3D12_SHADER_VISIBILITY_DOMAIN:
			CurrentVisibleSF = SF_Domain;
			break;
		case D3D12_SHADER_VISIBILITY_GEOMETRY:
			CurrentVisibleSF = SF_Geometry;
			break;
		case D3D12_SHADER_VISIBILITY_PIXEL:
			CurrentVisibleSF = SF_Pixel;
			break;

		default:
			check(false);
			break;
		}

		// Determine shader stage visibility.
		{
			Stage[SF_Vertex].bVisible = Stage[SF_Vertex].bVisible || (!bDenyVS && HasVisibility(CurrentParameter.ShaderVisibility, D3D12_SHADER_VISIBILITY_VERTEX));
			Stage[SF_Hull].bVisible = Stage[SF_Hull].bVisible || (!bDenyHS && HasVisibility(CurrentParameter.ShaderVisibility, D3D12_SHADER_VISIBILITY_HULL));
			Stage[SF_Domain].bVisible = Stage[SF_Domain].bVisible || (!bDenyDS && HasVisibility(CurrentParameter.ShaderVisibility, D3D12_SHADER_VISIBILITY_DOMAIN));
			Stage[SF_Geometry].bVisible = Stage[SF_Geometry].bVisible || (!bDenyGS && HasVisibility(CurrentParameter.ShaderVisibility, D3D12_SHADER_VISIBILITY_GEOMETRY));
			Stage[SF_Pixel].bVisible = Stage[SF_Pixel].bVisible || (!bDenyPS && HasVisibility(CurrentParameter.ShaderVisibility, D3D12_SHADER_VISIBILITY_PIXEL));

			// Compute is a special case, it must have visibility all.
			Stage[SF_Compute].bVisible = Stage[SF_Compute].bVisible || (CurrentParameter.ShaderVisibility == D3D12_SHADER_VISIBILITY_ALL);
		}

		// Determine shader resource counts.
		{
			switch (CurrentParameter.ParameterType)
			{
			case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
				check(CurrentParameter.DescriptorTable.NumDescriptorRanges == 1);	// Code currently assumes a single descriptor range.
				{
					const D3D12_DESCRIPTOR_RANGE& CurrentRange = CurrentParameter.DescriptorTable.pDescriptorRanges[0];
					check(CurrentRange.BaseShaderRegister == 0);	// Code currently assumes always starting at register 0.
					switch (CurrentRange.RangeType)
					{
					case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
						SetMaxSRVCount(CurrentVisibleSF, CurrentRange.NumDescriptors);
						SetSRVRDTBindSlot(CurrentVisibleSF, i);
						break;
					case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
						SetMaxUAVCount(CurrentVisibleSF, CurrentRange.NumDescriptors);
						SetUAVRDTBindSlot(CurrentVisibleSF, i);
						break;
					case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
						SetMaxCBVCount(CurrentVisibleSF, CurrentRange.NumDescriptors);
						SetCBVRDTBindSlot(CurrentVisibleSF, i);
						break;
					case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
						SetMaxSamplerCount(CurrentVisibleSF, CurrentRange.NumDescriptors);
						SetSamplersRDTBindSlot(CurrentVisibleSF, i);
						break;

					default: check(false); break;
					}
				}
				break;

			default:
				// Need to update this for the other types. Currently we only use descriptor tables in the root signature.
				check(false);
				break;
			}
		}
	}
}

FD3D12RootSignatureManager::~FD3D12RootSignatureManager()
{
	for (auto Iter = RootSignatureMap.CreateIterator(); Iter; ++Iter)
	{
		FD3D12RootSignature* pRootSignature = Iter.Value();
		delete pRootSignature;
	}
}

FD3D12RootSignature* FD3D12RootSignatureManager::GetRootSignature(const FD3D12QuantizedBoundShaderState& QBSS)
{
	// Creating bound shader states happens in parallel, so this must be thread safe.
	FScopeLock Lock(&CS);

	FD3D12RootSignature** ppRootSignature = RootSignatureMap.Find(QBSS);
	if (ppRootSignature == nullptr)
	{
		// Create a new root signature and return it.
		return CreateRootSignature(QBSS);
	}

	check(*ppRootSignature);
	return *ppRootSignature;
}

FD3D12RootSignature* FD3D12RootSignatureManager::CreateRootSignature(const FD3D12QuantizedBoundShaderState& QBSS)
{
	// Create a desc and the root signature.
	FD3D12RootSignature* pNewRootSignature = new FD3D12RootSignature(GetParentDevice(), QBSS);
	check(pNewRootSignature);

	// Add the index to the map.
	RootSignatureMap.Add(QBSS, pNewRootSignature);

	return pNewRootSignature;
}
