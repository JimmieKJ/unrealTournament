// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Device.h: D3D12 Device Interfaces
=============================================================================*/

#pragma once

class FD3D12DynamicRHI;
class FD3D12OcclusionQuery;

struct FD3D12Adapter
{
	/** -1 if not supported or FindAdpater() wasn't called. Ideally we would store a pointer to IDXGIAdapter but it's unlikely the adpaters change during engine init. */
	int32 AdapterIndex;
	/** The maximum D3D12 feature level supported. 0 if not supported or FindAdpater() wasn't called */
	D3D_FEATURE_LEVEL MaxSupportedFeatureLevel;

	// constructor
	FD3D12Adapter(int32 InAdapterIndex = -1, D3D_FEATURE_LEVEL InMaxSupportedFeatureLevel = (D3D_FEATURE_LEVEL)0)
		: AdapterIndex(InAdapterIndex)
		, MaxSupportedFeatureLevel(InMaxSupportedFeatureLevel)
	{
	}

	bool IsValid() const
	{
		return MaxSupportedFeatureLevel != (D3D_FEATURE_LEVEL)0 && AdapterIndex >= 0;
	}
};

class FD3D12Device
{

private:
	FD3D12Adapter               DeviceAdapter;
	DXGI_ADAPTER_DESC           AdapterDesc;
	TRefCountPtr<IDXGIAdapter3> DxgiAdapter3;
	TRefCountPtr<IDXGIFactory4> DXGIFactory;
	TRefCountPtr<ID3D12Device>  Direct3DDevice;
	TRefCountPtr<ID3D12Device1> Direct3DDevice1;
	FD3D12DynamicRHI*           OwningRHI;
	D3D12_RESOURCE_HEAP_TIER    ResourceHeapTier;
	D3D12_RESOURCE_BINDING_TIER ResourceBindingTier;

	/** True if the device being used has been removed. */
	bool bDeviceRemoved;

public:
	FD3D12Device(FD3D12DynamicRHI* InOwningRHI, IDXGIFactory4* InDXGIFactory, FD3D12Adapter& InAdapter);

	/** If it hasn't been initialized yet, initializes the D3D device. */
	virtual void InitD3DDevice();

	void CreateCommandContexts();

	/**
	* Cleanup the D3D device.
	* This function must be called from the main game thread.
	*/
	virtual void CleanupD3DDevice();

	/**
	* Populates a D3D query's data buffer.
	* @param Query - The occlusion query to read data from.
	* @param bWait - If true, it will wait for the query to finish.
	* @return true if the query finished.
	*/
	bool GetQueryData(FD3D12OcclusionQuery& Query, bool bWait);

	FSamplerStateRHIRef CreateSamplerState(const FSamplerStateInitializerRHI& Initializer);

	inline ID3D12Device*                GetDevice() { return Direct3DDevice; }
	inline ID3D12Device1*               GetDevice1() { return Direct3DDevice1; }
	inline uint32                       GetAdapterIndex() { return DeviceAdapter.AdapterIndex; }
	inline D3D_FEATURE_LEVEL            GetFeatureLevel() { return DeviceAdapter.MaxSupportedFeatureLevel; }
	inline DXGI_ADAPTER_DESC*           GetD3DAdapterDesc() { return &AdapterDesc; }
	inline void                         SetDeviceRemoved(bool value) { bDeviceRemoved = value; }
	inline bool                         IsDeviceRemoved() { return bDeviceRemoved; }
	inline FD3D12DynamicRHI*            GetOwningRHI() { return OwningRHI; }
	inline void                         SetAdapter3(IDXGIAdapter3* Adapter3) { DxgiAdapter3 = Adapter3; }
	inline IDXGIAdapter3*               GetAdapter3() { return DxgiAdapter3; }

	inline TArray<FD3D12Viewport*>&     GetViewports() { return Viewports; }
	inline FD3D12Viewport*              GetDrawingViewport() { return DrawingViewport; }
	inline void                         SetDrawingViewport(FD3D12Viewport* InViewport) { DrawingViewport = InViewport; }

	inline FD3D12PipelineStateCache&    GetPSOCache() { return PipelineStateCache; }
	inline ID3D12CommandSignature*      GetDrawIndirectCommandSignature() { return DrawIndirectCommandSignature; }
	inline ID3D12CommandSignature*      GetDrawIndexedIndirectCommandSignature() { return DrawIndexedIndirectCommandSignature; }
	inline ID3D12CommandSignature*      GetDispatchIndirectCommandSignature() { return DispatchIndirectCommandSignature; }
	inline FD3D12QueryHeap*             GetQueryHeap() { return &OcclusionQueryHeap; }

	ID3D12Resource*						GetCounterUploadHeap() { return CounterUploadHeap.GetReference(); }
	uint32&								GetCounterUploadHeapIndex() { return CounterUploadHeapIndex; }
	void*								GetCounterUploadHeapData() { return CounterUploadHeapData; }

	template <typename TViewDesc> FDescriptorHeapManager& GetViewDescriptorAllocator();
	template<> FDescriptorHeapManager& GetViewDescriptorAllocator<D3D12_SHADER_RESOURCE_VIEW_DESC>() { return SRVAllocator; }
	template<> FDescriptorHeapManager& GetViewDescriptorAllocator<D3D12_RENDER_TARGET_VIEW_DESC>() { return RTVAllocator; }
	template<> FDescriptorHeapManager& GetViewDescriptorAllocator<D3D12_DEPTH_STENCIL_VIEW_DESC>() { return DSVAllocator; }
	template<> FDescriptorHeapManager& GetViewDescriptorAllocator<D3D12_UNORDERED_ACCESS_VIEW_DESC>() { return UAVAllocator; }
	template<> FDescriptorHeapManager& GetViewDescriptorAllocator<D3D12_CONSTANT_BUFFER_VIEW_DESC>() { return CBVAllocator; }

	inline FDescriptorHeapManager&          GetSamplerDescriptorAllocator() { return SamplerAllocator; }
	inline FD3D12CommandListManager&        GetCommandListManager() { return CommandListManager; }
	inline FD3D12CommandListManager&        GetCopyCommandListManager() { return CopyCommandListManager; }
	inline FD3D12CommandListManager&        GetAsyncCommandListManager() { return AsyncCommandListManager; }
	inline FD3D12CommandAllocatorManager&   GetTextureStreamingCommandAllocatorManager() { return TextureStreamingCommandAllocatorManager; }
	inline FD3D12ResourceHelper&            GetResourceHelper() { return ResourceHelper; }
	inline FD3D12DeferredDeletionQueue&     GetDeferredDeletionQueue() { return DeferredDeletionQueue; }
	inline FD3D12DefaultBufferAllocator&    GetDefaultBufferAllocator() { return DefaultBufferAllocator; }
	inline FD3D12GlobalOnlineHeap&          GetGlobalSamplerHeap() { return GlobalSamplerHeap; }
	inline FD3D12GlobalOnlineHeap&          GetGlobalViewHeap() { return GlobalViewHeap; }

	bool IsGPUIdle();

	inline const D3D12_HEAP_PROPERTIES &GetConstantBufferPageProperties() { return ConstantBufferPageProperties; }

	inline uint32 GetNumContexts() { return CommandContextArray.Num(); }
	inline FD3D12CommandContext& GetCommandContext(uint32 i = 0) const { return *CommandContextArray[i]; }

	inline uint32 GetNumAsyncComputeContexts() { return AsyncComputeContextArray.Num(); }
	inline FD3D12CommandContext& GetAsyncComputeContext(uint32 i = 0) const { return *AsyncComputeContextArray[i]; }

	inline FD3D12CommandContext* ObtainCommandContext() {
		FScopeLock Lock(&FreeContextsLock);
		return FreeCommandContexts.Pop();
	}
	inline void ReleaseCommandContext(FD3D12CommandContext* CmdContext) {
		FScopeLock Lock(&FreeContextsLock);
		FreeCommandContexts.Add(CmdContext);
	}

	inline FD3D12RootSignature* GetRootSignature(const FD3D12QuantizedBoundShaderState& QBSS) {
		return RootSignatureManager.GetRootSignature(QBSS);
	}

	inline FD3D12CommandContext& GetDefaultCommandContext() const { return GetCommandContext(0); }
	inline FD3D12CommandContext& GetDefaultAsyncComputeContext() const { return GetAsyncComputeContext(0); }
	inline FD3D12DynamicHeapAllocator& GetDefaultUploadHeapAllocator() { return DefaultUploadHeapAllocator; }
	inline FD3D12FastAllocator& GetDefaultFastAllocator() { return DefaultFastAllocator; }
	inline FD3D12ThreadSafeFastAllocator& GetBufferInitFastAllocator() { return BufferInitializerFastAllocator; }
	inline FD3D12FastAllocatorPagePool& GetDefaultFastAllocatorPool() { return DefaultFastAllocatorPagePool; }
	inline FD3D12FastAllocatorPagePool& GetBufferInitFastAllocatorPool() { return BufferInitializerFastAllocatorPagePool; }
	inline FD3D12TextureAllocatorPool& GetTextureAllocator() { return TextureAllocator; }

	inline D3D12_RESOURCE_HEAP_TIER    GetResourceHeapTier() { return ResourceHeapTier; }
	inline D3D12_RESOURCE_BINDING_TIER GetResourceBindingTier() { return ResourceBindingTier; }

	inline FD3D12FenceCorePool& GetFenceCorePool() { return FenceCorePool; }

	inline FD3D12ResidencyManager& GetResidencyManager() { return ResidencyManager; }

	TArray<FD3D12CommandListHandle> PendingCommandLists;
	uint32 PendingCommandListsTotalWorkCommands;

	bool FirstFrameSeen;

protected:


	/** A pool of command lists we can cycle through for the global D3D device */
	FD3D12CommandListManager CommandListManager;
	FD3D12CommandListManager CopyCommandListManager;
	FD3D12CommandListManager AsyncCommandListManager;

	/** A pool of command allocators that texture streaming threads share */
	FD3D12CommandAllocatorManager TextureStreamingCommandAllocatorManager;

	FD3D12RootSignatureManager RootSignatureManager;

	TRefCountPtr<ID3D12CommandSignature> DrawIndirectCommandSignature;
	TRefCountPtr<ID3D12CommandSignature> DrawIndexedIndirectCommandSignature;
	TRefCountPtr<ID3D12CommandSignature> DispatchIndirectCommandSignature;

	TRefCountPtr<ID3D12Resource> CounterUploadHeap;
	uint32                       CounterUploadHeapIndex;
	void*                        CounterUploadHeapData;

	FD3D12PipelineStateCache PipelineStateCache;

	// Must be before the StateCache so that destructor ordering is valid
	FDescriptorHeapManager RTVAllocator;
	FDescriptorHeapManager DSVAllocator;
	FDescriptorHeapManager SRVAllocator;
	FDescriptorHeapManager UAVAllocator;
	FDescriptorHeapManager SamplerAllocator;
	FDescriptorHeapManager CBVAllocator;

	/** The resource manager allocates resources and tracks when to release them */
	FD3D12ResourceHelper ResourceHelper;

	FD3D12GlobalOnlineHeap GlobalSamplerHeap;
	FD3D12GlobalOnlineHeap GlobalViewHeap;

	FD3D12DeferredDeletionQueue DeferredDeletionQueue;

	FD3D12QueryHeap OcclusionQueryHeap;

	FD3D12DefaultBufferAllocator DefaultBufferAllocator;

	TArray<FD3D12CommandContext*> CommandContextArray;
	TArray<FD3D12CommandContext*> FreeCommandContexts;
	FCriticalSection FreeContextsLock;

	TArray<FD3D12CommandContext*> AsyncComputeContextArray;

	/** A list of all viewport RHIs that have been created. */
	TArray<FD3D12Viewport*> Viewports;

	/** The viewport which is currently being drawn. */
	TRefCountPtr<FD3D12Viewport> DrawingViewport;

	TMap< D3D12_SAMPLER_DESC, TRefCountPtr<FD3D12SamplerState> > SamplerMap;
	uint32 SamplerID;

	// set by UpdateMSAASettings(), get by GetMSAAQuality()
	// [SampleCount] = Quality, 0xffffffff if not supported
	uint32 AvailableMSAAQualities[DX_MAX_MSAA_COUNT + 1];

	// set by UpdateConstantBufferPageProperties, get by GetConstantBufferPageProperties
	D3D12_HEAP_PROPERTIES ConstantBufferPageProperties;

	// Creates default root and execute indirect signatures
	void CreateSignatures();

	// shared code for different D3D12  devices (e.g. PC DirectX12 and XboxOne) called
	// after device creation and GRHISupportsAsyncTextureCreation was set and before resource init
	void SetupAfterDeviceCreation();

	// called by SetupAfterDeviceCreation() when the device gets initialized

	void UpdateMSAASettings();

	void UpdateConstantBufferPageProperties();

	void ReleasePooledUniformBuffers();

	// Used for Locking and unlocking dynamic VBs and IBs. These functions only occur on the default
	// context so no thread sync is required
	FD3D12FastAllocatorPagePool DefaultFastAllocatorPagePool;
	FD3D12FastAllocator DefaultFastAllocator;

	// Buffers Get Initialized on multiple threads so access to the allocator
	// must be guarded by a lock
	FD3D12FastAllocatorPagePool  BufferInitializerFastAllocatorPagePool;
	FD3D12ThreadSafeFastAllocator BufferInitializerFastAllocator;

	FD3D12DynamicHeapAllocator	DefaultUploadHeapAllocator;

	FD3D12TextureAllocatorPool TextureAllocator;

	FD3D12FenceCorePool FenceCorePool;

	FD3D12ResidencyManager ResidencyManager;
};

// Inline functions for FD3D12View which are dependent on the full FD3DDevice declaration

template <typename TDesc>
void FD3D12View<TDesc>::AllocateHeapSlot()
{
	FDescriptorHeapManager& DescriptorAllocator = GetParentDevice()->GetViewDescriptorAllocator<TDesc>();
	Descriptor = DescriptorAllocator.AllocateHeapSlot(DescriptorHeapIndex);
	check(Descriptor.ptr != 0);
}

template <typename TDesc>
void FD3D12View<TDesc>::FreeHeapSlot()
{
	if (Descriptor.ptr)
	{
		FDescriptorHeapManager& DescriptorAllocator = GetParentDevice()->GetViewDescriptorAllocator<TDesc>();
		DescriptorAllocator.FreeHeapSlot(Descriptor, DescriptorHeapIndex);
		Descriptor.ptr = 0;
	}
}


template <typename TDesc>
void FD3D12View<TDesc>::CreateView(FD3D12Resource* InResource, FD3D12Resource* InCounterResource)
{
	if (!InResource)
	{
		InResource = GetResource();
	}
	else
	{
		// Only need to update the view's subresource subset if a new resource is used
		UpdateViewSubresourceSubset(InResource);
	}

	check(Descriptor.ptr != 0);
	(GetParentDevice()->GetDevice()->*TCreateViewMap<TDesc>::GetCreate()) (
		InResource ? InResource->GetResource() : nullptr, &Desc, Descriptor);
}

template <typename TDesc>
void FD3D12View<TDesc>::CreateViewWithCounter(FD3D12Resource* InResource, FD3D12Resource* InCounterResource)
{
	if (!InResource)
	{
		InResource = GetResource();
	}
	else
	{
		// Only need to update the view's subresource subset if a new resource is used
		UpdateViewSubresourceSubset(InResource);
	}

	check(Descriptor.ptr != 0);
	(GetParentDevice()->GetDevice()->*TCreateViewMap<TDesc>::GetCreate()) (
		InResource ? InResource->GetResource() : nullptr,
		InCounterResource ? InCounterResource->GetResource() : nullptr, &Desc, Descriptor);
}
