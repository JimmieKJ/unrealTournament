// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanVertexDeclaration.cpp: Vulkan vertex declaration RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"

struct FVulkanVertexDeclarationKey
{
    FVertexDeclarationElementList VertexElements;

    uint32 Hash;

    explicit FVulkanVertexDeclarationKey(const FVertexDeclarationElementList& InElements)
        : VertexElements(InElements)
    {
        Hash = FCrc::MemCrc_DEPRECATED(VertexElements.GetData(), VertexElements.Num()*sizeof(FVertexElement));
    }
};

inline uint32 GetTypeHash(const FVulkanVertexDeclarationKey& Key)
{
    return Key.Hash;
}

bool operator==(const FVulkanVertexDeclarationKey& A, const FVulkanVertexDeclarationKey& B)
{
	return (A.VertexElements.Num() == B.VertexElements.Num()
			&& !memcmp(A.VertexElements.GetData(), B.VertexElements.GetData(), A.VertexElements.Num() * sizeof(FVertexElement)));
}

FVulkanVertexDeclaration::FVulkanVertexDeclaration(const FVertexDeclarationElementList& InElements) :
	Elements(InElements)
{
}

TMap<FVulkanVertexDeclarationKey, FVertexDeclarationRHIRef> GVertexDeclarationCache;

void FVulkanVertexDeclaration::EmptyCache()
{
	GVertexDeclarationCache.Empty(0);
}

FVertexDeclarationRHIRef FVulkanDynamicRHI::RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
    FVulkanVertexDeclarationKey Key(Elements);

    FVertexDeclarationRHIRef* VertexDeclarationRefPtr = GVertexDeclarationCache.Find(Key);
    if (VertexDeclarationRefPtr == nullptr)
    {
        VertexDeclarationRefPtr = &GVertexDeclarationCache.Add(Key, new FVulkanVertexDeclaration(Elements));
    }

    check(VertexDeclarationRefPtr);
    check(IsValidRef(*VertexDeclarationRefPtr));

    return *VertexDeclarationRefPtr;
}

FVulkanVertexInputStateInfo::FVulkanVertexInputStateInfo() :
	Hash(0)
{
	FMemory::Memzero(Info);
}

void FVulkanVertexInputStateInfo::Create(uint32 BindingsNum, VkVertexInputBindingDescription* VertexInputBindings, uint32 AttributesNum, VkVertexInputAttributeDescription* Attributes)
{
	// GenerateVertexInputState is expected to be called only once!
	check(Info.sType == 0);

	// Vertex Input
	Info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	// Its possible to have no vertex buffers
	if (BindingsNum == 0)
	{
		check(Hash == 0);
		return;
	}

	Info.vertexBindingDescriptionCount = BindingsNum;
	Info.pVertexBindingDescriptions = VertexInputBindings;

	check(AttributesNum > 0);
	Info.vertexAttributeDescriptionCount = AttributesNum;
	Info.pVertexAttributeDescriptions = Attributes;

	Hash = FCrc::MemCrc32(VertexInputBindings, BindingsNum * sizeof(VertexInputBindings[0]));
	check(AttributesNum > 0);
	Hash = FCrc::MemCrc32(Attributes, AttributesNum * sizeof(Attributes[0]), Hash);
}
