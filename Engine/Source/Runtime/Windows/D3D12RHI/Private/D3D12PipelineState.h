// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// Implementation of D3D12 Pipelinestate related functions

#pragma once

static bool GCPUSupportsSSE4;

#define PSO_IF_NOT_EQUAL_RETURN_FALSE( value ) if(lhs.##value != rhs.##value){ return false; }

#define PSO_IF_MEMCMP_FAILS_RETURN_FALSE( value ) if(FMemory::Memcmp(&lhs.##value, &rhs.##value, sizeof(rhs.##value)) != 0){ return false; }

#define PSO_IF_STRING_COMPARE_FAILS_RETURN_FALSE( value ) \
	const char* const lhString = lhs.##value##; \
	const char* const rhString = rhs.##value##; \
	if (lhString != rhString) \
	{ \
		if (strcmp(lhString, rhString) != 0) \
		{ \
			return false; \
		} \
	}

template <typename TDesc> struct equality_pipeline_state_desc;
template <> struct equality_pipeline_state_desc<FD3D12HighLevelGraphicsPipelineStateDesc>
{
	bool operator()(const FD3D12HighLevelGraphicsPipelineStateDesc& lhs, const FD3D12HighLevelGraphicsPipelineStateDesc& rhs)
	{
		PSO_IF_NOT_EQUAL_RETURN_FALSE(BoundShaderState);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(BlendState);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(DepthStencilState);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(RasterizerState);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(SampleMask);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(PrimitiveTopologyType);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(NumRenderTargets);
		for (SIZE_T i = 0; i < lhs.NumRenderTargets; i++)
		{
			PSO_IF_NOT_EQUAL_RETURN_FALSE(RTVFormats[i]);
		}
		PSO_IF_NOT_EQUAL_RETURN_FALSE(DSVFormat);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(SampleDesc.Count);
		PSO_IF_NOT_EQUAL_RETURN_FALSE(SampleDesc.Quality);
		return true;
	}
};

template <> struct equality_pipeline_state_desc<FD3D12LowLevelGraphicsPipelineStateDesc>
{
	bool operator()(const FD3D12LowLevelGraphicsPipelineStateDesc& lhs, const FD3D12LowLevelGraphicsPipelineStateDesc& rhs)
	{
		// Order from most likely to change to least
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.PS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.VS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.GS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.DS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.HS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.NumElements)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.NumRenderTargets)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.DSVFormat)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.PrimitiveTopologyType)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.Flags)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.pRootSignature)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.SampleMask)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.IBStripCutValue)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.NodeMask)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.RasterizedStream)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.NumEntries)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.NumStrides)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.SampleDesc.Count)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.SampleDesc.Quality)

		PSO_IF_MEMCMP_FAILS_RETURN_FALSE(Desc.BlendState)
		PSO_IF_MEMCMP_FAILS_RETURN_FALSE(Desc.RasterizerState)
		PSO_IF_MEMCMP_FAILS_RETURN_FALSE(Desc.DepthStencilState)
		for (SIZE_T i = 0; i < lhs.Desc.NumRenderTargets; i++)
		{
			PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.RTVFormats[i]);
		}

		// Shader byte code is hashed with SHA1 (160 bit) so the chances of collision
		// should be tiny i.e if there were 1 quadrillion shaders the chance of a 
		// collision is ~ 1 in 10^18. so only do a full check on debug builds
		PSO_IF_NOT_EQUAL_RETURN_FALSE(VSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(PSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(GSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(HSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(DSHash)

#if UE_BUILD_DEBUG
			const D3D12_SHADER_BYTECODE* lhByteCode = &lhs.Desc.VS;
		const D3D12_SHADER_BYTECODE* rhByteCode = &rhs.Desc.VS;
		for (SIZE_T i = 0; i < 5; i++)
		{
			if (lhByteCode[i].pShaderBytecode != rhByteCode[i].pShaderBytecode &&
				lhByteCode[i].pShaderBytecode != nullptr &&
				lhByteCode[i].BytecodeLength)
			{
				if (FMemory::Memcmp(lhByteCode[i].pShaderBytecode, rhByteCode[i].pShaderBytecode, rhByteCode[i].BytecodeLength) != 0)
				{
					UE_LOG(LogD3D12RHI, Error, TEXT("Error! there is a collision with the SHA1. This should never happen but checking for completeness."));
					return false;
				}
			}
		}
#endif

		if (lhs.Desc.StreamOutput.pSODeclaration != rhs.Desc.StreamOutput.pSODeclaration &&
			lhs.Desc.StreamOutput.NumEntries)
		{
			for (SIZE_T i = 0; i < lhs.Desc.StreamOutput.NumEntries; i++)
			{
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].Stream)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].SemanticIndex)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].StartComponent)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].ComponentCount)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].OutputSlot)
				PSO_IF_STRING_COMPARE_FAILS_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].SemanticName)
			}
		}

		if (lhs.Desc.StreamOutput.pBufferStrides != rhs.Desc.StreamOutput.pBufferStrides &&
			lhs.Desc.StreamOutput.NumStrides)
		{
			for (SIZE_T i = 0; i < lhs.Desc.StreamOutput.NumStrides; i++)
			{
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pBufferStrides[i])
			}
		}

		if (lhs.Desc.InputLayout.pInputElementDescs != rhs.Desc.InputLayout.pInputElementDescs &&
			lhs.Desc.InputLayout.NumElements)
		{
			for (SIZE_T i = 0; i < lhs.Desc.InputLayout.NumElements; i++)
			{
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].SemanticIndex)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].Format)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].InputSlot)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].AlignedByteOffset)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].InputSlotClass)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].InstanceDataStepRate)
				PSO_IF_STRING_COMPARE_FAILS_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].SemanticName)
			}
		}
		return true;
	}
};
template <> struct equality_pipeline_state_desc<FD3D12ComputePipelineStateDesc>
{
	bool operator()(const FD3D12ComputePipelineStateDesc& lhs, const FD3D12ComputePipelineStateDesc& rhs)
	{
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.CS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.Flags)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.pRootSignature)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.NodeMask)

		// Shader byte code is hashed with SHA1 (160 bit) so the chances of collision
		// should be tiny i.e if there were 1 quadrillion shaders the chance of a 
		// collision is ~ 1 in 10^18. so only do a full check on debug builds
		PSO_IF_NOT_EQUAL_RETURN_FALSE(CSHash)

#if UE_BUILD_DEBUG
			if (lhs.Desc.CS.pShaderBytecode != rhs.Desc.CS.pShaderBytecode &&
				lhs.Desc.CS.pShaderBytecode != nullptr &&
				lhs.Desc.CS.BytecodeLength)
			{
				if (FMemory::Memcmp(lhs.Desc.CS.pShaderBytecode, rhs.Desc.CS.pShaderBytecode, lhs.Desc.CS.BytecodeLength) != 0)
				{
					return false;
				}
			}
#endif

		return true;
	}
};

template <typename TDesc> struct TPSOFunctionMap;
template<> struct TPSOFunctionMap < D3D12_GRAPHICS_PIPELINE_STATE_DESC >
{
	static decltype(&ID3D12Device::CreateGraphicsPipelineState) GetCreatePipelineState() { return &ID3D12Device::CreateGraphicsPipelineState; }
	static decltype(&ID3D12PipelineLibrary::LoadGraphicsPipeline) GetLoadPipeline() { return &ID3D12PipelineLibrary::LoadGraphicsPipeline; }
};
template<> struct TPSOFunctionMap < D3D12_COMPUTE_PIPELINE_STATE_DESC >
{
	static decltype(&ID3D12Device::CreateComputePipelineState) GetCreatePipelineState() { return &ID3D12Device::CreateComputePipelineState; }
	static decltype(&ID3D12PipelineLibrary::LoadComputePipeline) GetLoadPipeline() { return &ID3D12PipelineLibrary::LoadComputePipeline; }
};

struct FD3D12PipelineStateWorker : public FD3D12DeviceChild, public FNonAbandonableTask
{
	FD3D12PipelineStateWorker(FD3D12Device* Device, D3D12_COMPUTE_PIPELINE_STATE_DESC* InDesc, ID3D12PipelineLibrary* InLibrary, const FString& InName)
		: bIsGraphics(false), Library(InLibrary), Name(InName), FD3D12DeviceChild(Device) {
		Desc.ComputeDesc = *InDesc;
	};

	FD3D12PipelineStateWorker(FD3D12Device* Device, D3D12_GRAPHICS_PIPELINE_STATE_DESC* InDesc, ID3D12PipelineLibrary* InLibrary, const FString& InName)
		: bIsGraphics(true), Library(InLibrary), Name(InName), FD3D12DeviceChild(Device) {
		Desc.GraphicsDesc = *InDesc;
	};

	void DoWork();

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FD3D12PipelineStateWorker, STATGROUP_ThreadPoolAsyncTasks); }

	union
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC ComputeDesc;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC GraphicsDesc;
	} Desc;

	bool bIsGraphics;
	ID3D12PipelineLibrary* Library;
	FString Name;
	TRefCountPtr<ID3D12PipelineState> PSO;
};

struct FD3D12PipelineState : public FD3D12DeviceChild
{
public:
	FD3D12PipelineState() : Worker(nullptr), FD3D12DeviceChild(nullptr) {};
	FD3D12PipelineState(FD3D12Device* Parent) : Worker(nullptr), FD3D12DeviceChild(Parent) {};

	~FD3D12PipelineState();

	void Create(FD3D12ComputePipelineStateDesc* Desc, ID3D12PipelineLibrary* Library = nullptr);
	void CreateAsync(FD3D12ComputePipelineStateDesc* Desc, ID3D12PipelineLibrary* Library = nullptr);

	void Create(FD3D12LowLevelGraphicsPipelineStateDesc* Desc, ID3D12PipelineLibrary* Library = nullptr);
	void CreateAsync(FD3D12LowLevelGraphicsPipelineStateDesc* Desc, ID3D12PipelineLibrary* Library = nullptr);

	ID3D12PipelineState* GetPipelineState();

private:
	TRefCountPtr<ID3D12PipelineState> PipelineState;
	FAsyncTask<FD3D12PipelineStateWorker>* Worker;
};

class FD3D12PipelineStateCache : public FD3D12DeviceChild
{
private:
	enum PSO_CACHE_TYPE
	{
		PSO_CACHE_GRAPHICS,
		PSO_CACHE_COMPUTE,
		NUM_PSO_CACHE_TYPES
	};

	template <typename TDesc, typename TValue>
	struct TStateCacheKeyFuncs : BaseKeyFuncs<TPair<TDesc, TValue>, TDesc, false>
	{
		typedef typename TTypeTraits<TDesc>::ConstPointerType KeyInitType;
		typedef const typename TPairInitializer<typename TTypeTraits<TDesc>::ConstInitType, typename TTypeTraits<TValue>::ConstInitType>& ElementInitType;

		static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
		{
			return Element.Key;
		}
		static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
		{
			equality_pipeline_state_desc<TDesc> equal;
			return equal(A, B);
		}
		static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
		{
			return Key.CombinedHash;
		}
	};

	template <typename TDesc, typename TValue = FD3D12PipelineState>
	using TPipelineCache = TMap<TDesc, TValue, FDefaultSetAllocator, TStateCacheKeyFuncs<TDesc, TValue>>;

	TPipelineCache<FD3D12HighLevelGraphicsPipelineStateDesc, TPair<ID3D12PipelineState*, uint64>> HighLevelGraphicsPipelineStateCache;
	TPipelineCache<FD3D12LowLevelGraphicsPipelineStateDesc> LowLevelGraphicsPipelineStateCache;
	TPipelineCache<FD3D12ComputePipelineStateDesc> ComputePipelineStateCache;

	FCriticalSection CS;
	FDiskCacheInterface DiskCaches[NUM_PSO_CACHE_TYPES];
	FDiskCacheInterface DiskBinaryCache;
	TRefCountPtr<ID3D12PipelineLibrary> PipelineLibrary;

	ID3D12PipelineState* Add(FD3D12LowLevelGraphicsPipelineStateDesc &graphicsPSODesc);
	ID3D12PipelineState* Add(FD3D12ComputePipelineStateDesc &computePSODesc);

	ID3D12PipelineState* FindGraphicsLowLevel(FD3D12LowLevelGraphicsPipelineStateDesc &graphicsPSODesc);

#if UE_BUILD_DEBUG
	uint64 GraphicsCacheRequestCount = 0;
	uint64 HighLevelCacheFulfillCount = 0;
	uint64 HighLevelCacheStaleCount = 0;
	uint64 HighLevelCacheMissCount = 0;
#endif

	void WriteOutShaderBlob(PSO_CACHE_TYPE Cache, ID3D12PipelineState* APIPso);

	template<typename PipelineStateDescType>
	void ReadBackShaderBlob(PipelineStateDescType& Desc, PSO_CACHE_TYPE Cache)
	{
		SIZE_T* cachedBlobOffset = nullptr;
		DiskCaches[Cache].SetPointerAndAdvanceFilePosition((void**)&cachedBlobOffset, sizeof(SIZE_T));

		SIZE_T* cachedBlobSize = nullptr;
		DiskCaches[Cache].SetPointerAndAdvanceFilePosition((void**)&cachedBlobSize, sizeof(SIZE_T));

		check(cachedBlobOffset);
		check(cachedBlobSize);

		if (UseCachedBlobs())
		{
			check(*cachedBlobSize);
			Desc.CachedPSO.CachedBlobSizeInBytes = *cachedBlobSize;
			Desc.CachedPSO.pCachedBlob = DiskBinaryCache.GetDataAt(*cachedBlobOffset);
		}
		else
		{
			Desc.CachedPSO.CachedBlobSizeInBytes = 0;
			Desc.CachedPSO.pCachedBlob = nullptr;
		}
	}

	bool UsePipelineLibrary() const
	{
		return bUseAPILibaries && PipelineLibrary != nullptr;
	}

	bool UseCachedBlobs() const
	{
		return bUseAPILibaries && bUseCachedBlobs && !UsePipelineLibrary();
	}

public:
	void RebuildFromDiskCache();

	ID3D12PipelineState* FindGraphics(FD3D12HighLevelGraphicsPipelineStateDesc &graphicsPSODesc);
	ID3D12PipelineState* FindCompute(FD3D12ComputePipelineStateDesc &computePSODesc);

	void Close();

	void Init(FString &GraphicsCacheFilename, FString &ComputeCacheFilename, FString &DriverBlobFilename);
	bool IsInErrorState() const;

	static SIZE_T HashPSODesc(const FD3D12HighLevelGraphicsPipelineStateDesc &psoDesc);
	static SIZE_T HashPSODesc(const FD3D12LowLevelGraphicsPipelineStateDesc &psoDesc);
	static SIZE_T HashPSODesc(const FD3D12ComputePipelineStateDesc &psoDesc);

	static SIZE_T HashData(void* pData, SIZE_T NumBytes);

	FD3D12PipelineStateCache(FD3D12Device* InParent);
	~FD3D12PipelineStateCache();

	FD3D12PipelineStateCache& operator=(const FD3D12PipelineStateCache&);

	static const bool bUseAPILibaries = true;
	static const bool bUseCachedBlobs = false;
	uint32 DriverShaderBlobs;
};