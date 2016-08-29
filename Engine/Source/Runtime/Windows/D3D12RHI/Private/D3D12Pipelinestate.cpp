// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// Implementation of D3D12 Pipelinestate related functions

//-----------------------------------------------------------------------------
//	Include Files
//-----------------------------------------------------------------------------
#include "D3D12RHIPrivate.h"
#include "D3D12PipelineState.h"

void FD3D12HighLevelGraphicsPipelineStateDesc::GetLowLevelDesc(FD3D12LowLevelGraphicsPipelineStateDesc& psoDesc)
{
	FMemory::Memzero(&psoDesc, sizeof(psoDesc));

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
	if (IsInErrorState())
	{
		// TODO: Make sure we clear the disk caches that are in error.
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

	const uint32 NumGraphicsPSOs = DiskCaches[PSO_CACHE_GRAPHICS].GetNumPSOs();
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
			for (uint32 LayoutIndex = 0; LayoutIndex < PSODesc->InputLayout.NumElements; LayoutIndex++)
			{
				// Get the Sematic name string
				uint32* stringLength = nullptr;
				DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&stringLength, sizeof(uint32));
				DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->InputLayout.pInputElementDescs[LayoutIndex].SemanticName, *stringLength, true);
			}
		}
		if (PSODesc->StreamOutput.NumEntries)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->StreamOutput.pSODeclaration, PSODesc->StreamOutput.NumEntries * sizeof(D3D12_SO_DECLARATION_ENTRY), true);
			for (uint32 OutputIndex = 0; OutputIndex < PSODesc->StreamOutput.NumEntries; OutputIndex++)
			{
				//Get the Sematic name string
				uint32* stringLength = nullptr;
				DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&stringLength, sizeof(uint32));
				DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->StreamOutput.pSODeclaration[OutputIndex].SemanticName, *stringLength, true);
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
			PSOOut.CreateAsync(GraphicsPSODesc, PipelineLibrary);
		}
		else
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("PSO Cache read error!"));
			break;
		}
	}

	const uint32 NumComputePSOs = DiskCaches[PSO_CACHE_COMPUTE].GetNumPSOs();
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
			PSOOut.CreateAsync(ComputePSODesc, PipelineLibrary);
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
	PSOOut.Create(&graphicsPSODesc, PipelineLibrary);

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
	PSOOut.Create(&computePSODesc, PipelineLibrary);

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
			CA_SUPPRESS(6011);
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
	if (UseCachedBlobs())
	{
		TRefCountPtr<ID3DBlob> cachedBlob;
		HRESULT result = APIPso->GetCachedBlob(cachedBlob.GetInitReference());
		VERIFYD3D12RESULT(result);
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

	TArray<BYTE> LibraryData;
	const bool bOverwriteExistingPipelineLibrary = true;
	if (UsePipelineLibrary() && bOverwriteExistingPipelineLibrary)
	{
		// Serialize the Library.
		const SIZE_T LibrarySize = PipelineLibrary->GetSerializedSize();
		if (LibrarySize)
		{
			LibraryData.AddUninitialized(LibrarySize);
			check(LibraryData.Num() == LibrarySize);

			UE_LOG(LogD3D12RHI, Log, TEXT("Serializing Pipeline Library to disk (%llu KiB containing %u PSOs)"), LibrarySize / 1024ll, DriverShaderBlobs);
			VERIFYD3D12RESULT(PipelineLibrary->Serialize(LibraryData.GetData(), LibrarySize));

			// Write the Library to disk (overwrite existing data).
			DiskBinaryCache.Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
			const bool bSuccess = DiskBinaryCache.AppendData(LibraryData.GetData(), LibrarySize);
			UE_CLOG(!bSuccess, LogD3D12RHI, Warning, TEXT("Failed to write Pipeline Library to disk."));
		}
	}

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

	if (bUseAPILibaries)
	{
		// Create a pipeline library if the system supports it.
		ID3D12Device1* pDevice1 = GetParentDevice()->GetDevice1();
		if (pDevice1)
		{
			const SIZE_T LibrarySize = DiskBinaryCache.GetSizeInBytes();
			void* pLibraryBlob = LibrarySize ? DiskBinaryCache.GetDataAtStart() : nullptr;

			if (pLibraryBlob)
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("Creating Pipeline Library from existing disk cache (%llu KiB containing %u PSOs)."), LibrarySize / 1024ll, DriverShaderBlobs);
			}
			else
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("Creating new Pipeline Library."));
			}

			const HRESULT HResult = pDevice1->CreatePipelineLibrary(pLibraryBlob, LibrarySize, IID_PPV_ARGS(PipelineLibrary.GetInitReference()));

			// E_INVALIDARG if the blob is corrupted or unrecognized. D3D12_ERROR_DRIVER_VERSION_MISMATCH if the provided data came from 
			// an old driver or runtime. D3D12_ERROR_ADAPTER_NOT_FOUND if the data came from different hardware.
			if (DXGI_ERROR_UNSUPPORTED == HResult)
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("The driver doesn't support Pipeline Libraries."));
			}
			else if (FAILED(HResult))
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("Create Pipeline Library failed. Perhaps the Library has stale PSOs for the current HW or driver. Clearing the disk cache and trying again..."));

				// TODO: In the case of D3D12_ERROR_ADAPTER_NOT_FOUND, we don't really need to clear the cache, we just need to try another one. We should really have a cache per adapter.
				DiskBinaryCache.ClearDiskCache();
				DiskBinaryCache.Init(DriverBlobFilename);
				check(DiskBinaryCache.GetSizeInBytes() == 0);

				VERIFYD3D12RESULT(pDevice1->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(PipelineLibrary.GetInitReference())));
			}

			SetName(PipelineLibrary, L"Pipeline Library");
		}
	}

	// Check for SSE4 support see: https://msdn.microsoft.com/en-us/library/vstudio/hskdteyh(v=vs.100).aspx
	{
		int32 cpui[4];
		__cpuidex(cpui, 1, 0);
		GCPUSupportsSSE4 = !!(cpui[SSE4_CPUID_ARRAY_INDEX] & SSE4_2);
	}
}

bool FD3D12PipelineStateCache::IsInErrorState() const
{
	return (DiskCaches[PSO_CACHE_GRAPHICS].IsInErrorState() ||
		DiskCaches[PSO_CACHE_COMPUTE].IsInErrorState() ||
		(bUseAPILibaries && DiskBinaryCache.IsInErrorState()));
}

FD3D12PipelineStateCache::FD3D12PipelineStateCache(FD3D12Device* InParent) :
	DriverShaderBlobs(0)
	, FD3D12DeviceChild(InParent)
{

}

FD3D12PipelineStateCache::~FD3D12PipelineStateCache()
{

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

// Thread-safe create graphics/compute pipeline state. Conditionally load/store the PSO using a Pipeline Library.
template <typename TDesc>
ID3D12PipelineState* CreatePipelineState(ID3D12Device* Device, TDesc* Desc, ID3D12PipelineLibrary* Library, const TCHAR* const Name)
{
	ID3D12PipelineState* PSO = nullptr;
	if (Library)
	{
		// Try to load the PSO from the library.
		check(Name);
		HRESULT HResult = (Library->*TPSOFunctionMap<TDesc>::GetLoadPipeline())(Name, Desc, IID_PPV_ARGS(&PSO));
		if (E_INVALIDARG == HResult)
		{
			// The name doesn't exist or the input desc doesn't match the data in the library, just create the PSO.
			VERIFYD3D12RESULT((Device->*TPSOFunctionMap<TDesc>::GetCreatePipelineState())(Desc, IID_PPV_ARGS(&PSO)));

			// Try to save the PSO to the library for another time.
			check(PSO);
			HResult = Library->StorePipeline(Name, PSO);
			if (E_INVALIDARG != HResult)
			{
				// E_INVALIDARG means the name already exists in the library. Since the name is based on the hash, this is a hash collision.
				// We ignore E_INVALIDARG because we just create PSO's if they don't exist in the library.
				VERIFYD3D12RESULT(HResult);
			}
		}
		else
		{
			VERIFYD3D12RESULT(HResult);
		}
	}
	else
	{
		VERIFYD3D12RESULT((Device->*TPSOFunctionMap<TDesc>::GetCreatePipelineState())(Desc, IID_PPV_ARGS(&PSO)));
	}

	check(PSO);
	return PSO;
}

void FD3D12PipelineState::Create(FD3D12ComputePipelineStateDesc* Desc, ID3D12PipelineLibrary* Library)
{
	const FString Name = Library ? Desc->GetName() : L"";
	PipelineState = CreatePipelineState(GetParentDevice()->GetDevice(), &Desc->Desc, Library, Name.GetCharArray().GetData());
}

void FD3D12PipelineState::CreateAsync(FD3D12ComputePipelineStateDesc* Desc, ID3D12PipelineLibrary* Library)
{
	const FString Name = Library ? Desc->GetName() : L"";
	Worker = new FAsyncTask<FD3D12PipelineStateWorker>(GetParentDevice(), &Desc->Desc, Library, Name);
	if (Worker)
	{
		Worker->StartBackgroundTask();
	}
}

void FD3D12PipelineState::Create(FD3D12LowLevelGraphicsPipelineStateDesc* Desc, ID3D12PipelineLibrary* Library)
{
	const FString Name = Library ? Desc->GetName() : L"";
	PipelineState = CreatePipelineState(GetParentDevice()->GetDevice(), &Desc->Desc, Library, Name.GetCharArray().GetData());
}

void FD3D12PipelineState::CreateAsync(FD3D12LowLevelGraphicsPipelineStateDesc* Desc, ID3D12PipelineLibrary* Library)
{
	const FString Name = Library ? Desc->GetName() : L"";
	Worker = new FAsyncTask<FD3D12PipelineStateWorker>(GetParentDevice(), &Desc->Desc, Library, Name);
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
		PSO = CreatePipelineState(GetParentDevice()->GetDevice(), &Desc.GraphicsDesc, Library, Name.GetCharArray().GetData());
	}
	else
	{
		PSO = CreatePipelineState(GetParentDevice()->GetDevice(), &Desc.ComputeDesc, Library, Name.GetCharArray().GetData());
	}
}