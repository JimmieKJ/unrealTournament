// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanResources.h: Vulkan resource RHI definitions.
=============================================================================*/

#pragma once

#include "BoundShaderStateCache.h"
#include "CrossCompilerCommon.h"

class FVulkanShaderVarying
{
public:
	FVulkanShaderVarying():
		Location(0),
		Components(0)
	{
	}

	uint16 Location;
	TArray<ANSICHAR> Varying;
	uint16 Components; // Scalars/Integers

	friend bool operator==(const FVulkanShaderVarying& A, const FVulkanShaderVarying& B)
	{
		if (&A != &B)
		{
			return (A.Location == B.Location)
				&& (A.Varying.Num() == B.Varying.Num())
				&& (FMemory::Memcmp(A.Varying.GetData(), B.Varying.GetData(), A.Varying.Num() * sizeof(ANSICHAR)) == 0)
				&& (A.Components == B.Components);
		}
		return true;
	}

	friend uint32 GetTypeHash(const FVulkanShaderVarying& Var)
	{
		uint32 Hash = GetTypeHash(Var.Location);
		Hash ^= FCrc::MemCrc32(Var.Varying.GetData(), Var.Varying.Num() * sizeof(ANSICHAR));
		return Hash;
	}
};

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderVarying& Var)
{
	check(!Ar.IsSaving() || Var.Components > 0);

	Ar << Var.Varying;
	Ar << Var.Location;
	Ar << Var.Components;
	return Ar;
}

static void ClearBindings(CrossCompiler::FShaderBindings& Bindings)
{
	Bindings.InOutMask = 0;
	Bindings.NumSamplers = 0;
	Bindings.NumUniformBuffers = 0;
	Bindings.NumUAVs = 0;
	Bindings.bHasRegularUniformBuffers = 0;
}

class FVulkanShaderSerializedBindings : public CrossCompiler::FShaderBindings
{
public:
	FVulkanShaderSerializedBindings():
		bFlattenUB(false)
	{
		ClearBindings(*this);
		FMemory::Memzero(PackedUBTypeIndex);
	}

	enum EBindingType : uint16
	{
		TYPE_COMBINED_IMAGE_SAMPLER,
		TYPE_SAMPLER_BUFFER,
		TYPE_UNIFORM_BUFFER,
		TYPE_PACKED_UNIFORM_BUFFER,
		//TYPE_SAMPLER,
		//TYPE_IMAGE,

		TYPE_MAX,
	};

	struct FBindMap
	{
		FBindMap() :
			VulkanBindingIndex(-1),
			EngineBindingIndex(-1)
		{
		}

		int16 VulkanBindingIndex;
		int16 EngineBindingIndex;	// Used to remap EngineBindingIndex -> VulkanBindingIndex
	};

	TArray<FBindMap> Bindings[TYPE_MAX];
	// For the packed UB bindings, what is the packed type index to use
	uint8 PackedUBTypeIndex[CrossCompiler::PACKED_TYPEINDEX_MAX];

	bool bFlattenUB;
};

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderSerializedBindings::FBindMap& Binding)
{
	Ar << Binding.VulkanBindingIndex;
	Ar << Binding.EngineBindingIndex;

	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderSerializedBindings& Bindings)
{
	Ar << Bindings.PackedUniformBuffers;
	Ar << Bindings.PackedGlobalArrays;
	for (int32 Index = 0; Index < FVulkanShaderSerializedBindings::TYPE_MAX; ++Index)
	{
		Ar << Bindings.Bindings[Index];
	}
	for (int32 Index = 0; Index < CrossCompiler::PACKED_TYPEINDEX_MAX; ++Index)
	{
		Ar << Bindings.PackedUBTypeIndex[Index];
	}

	Ar << Bindings.ShaderResourceTable.ResourceTableBits;
	Ar << Bindings.ShaderResourceTable.MaxBoundResourceTable;
	Ar << Bindings.ShaderResourceTable.TextureMap;
	Ar << Bindings.ShaderResourceTable.ShaderResourceViewMap;
	Ar << Bindings.ShaderResourceTable.SamplerMap;
	Ar << Bindings.ShaderResourceTable.UnorderedAccessViewMap;
	Ar << Bindings.ShaderResourceTable.ResourceTableLayoutHashes;

	Ar << Bindings.InOutMask;
	Ar << Bindings.NumSamplers;
	Ar << Bindings.NumUniformBuffers;
	Ar << Bindings.NumUAVs;
	Ar << Bindings.bFlattenUB;

	return Ar;
}

struct FVulkanCodeHeader
{
	FVulkanShaderSerializedBindings SerializedBindings;
	// List of memory copies from RHIUniformBuffer to packed uniforms
	TArray<CrossCompiler::FUniformBufferCopyInfo> UniformBuffersCopyInfo;
	FString ShaderName;
	FSHAHash SourceHash;

	FBaseShaderResourceTable ShaderResourceTable;
};

inline FArchive& operator<<(FArchive& Ar, FVulkanCodeHeader& Header)
{
	Ar << Header.SerializedBindings;
	int32 NumInfos = Header.UniformBuffersCopyInfo.Num();
	Ar << NumInfos;
	if (Ar.IsSaving())
	{
		for (int32 Index = 0; Index < NumInfos; ++Index)
		{
			Ar << Header.UniformBuffersCopyInfo[Index];
		}
	}
	else if (Ar.IsLoading())
	{
		Header.UniformBuffersCopyInfo.Empty(NumInfos);
		for (int32 Index = 0; Index < NumInfos; ++Index)
		{
			CrossCompiler::FUniformBufferCopyInfo Info;
			Ar << Info;
			Header.UniformBuffersCopyInfo.Add(Info);
		}
	}
	Ar << Header.ShaderName;
	Ar << Header.SourceHash;
	return Ar;
}


class FVulkanShaderBindingTable
{
public:
	TArray<uint32> CombinedSamplerBindingIndices;
	//#todo-rco: FIX! SamplerBuffers share numbering with Samplers
	TArray<uint32> SamplerBufferBindingIndices;
	TArray<uint32> UniformBufferBindingIndices;

	int32 PackedGlobalUBsIndices[CrossCompiler::PACKED_TYPEINDEX_MAX];

	uint16 NumDescriptors;
	uint16 NumDescriptorsWithoutPackedUniformBuffers;

	FVulkanShaderBindingTable() :
		NumDescriptors(0),
		NumDescriptorsWithoutPackedUniformBuffers(0)
	{
	}
};

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderBindingTable& BindingTable)
{
	Ar << BindingTable.CombinedSamplerBindingIndices;
	Ar << BindingTable.SamplerBufferBindingIndices;
	Ar << BindingTable.UniformBufferBindingIndices;
	for (int32 Index = 0; Index < CrossCompiler::PACKED_TYPEINDEX_MAX; ++Index)
	{
		Ar << BindingTable.PackedGlobalUBsIndices[Index];
	}

	Ar << BindingTable.NumDescriptors;
	Ar << BindingTable.NumDescriptorsWithoutPackedUniformBuffers;

	return Ar;
}
