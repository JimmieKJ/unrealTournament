// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "ChunkDependencyInfo.h"

UChunkDependencyInfo::UChunkDependencyInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const FChunkDependencyTreeNode* UChunkDependencyInfo::GetChunkDependencyGraph(uint32 TotalChunks)
{
	// Reset any current tree
	RootTreeNode.ChunkID = 0;
	RootTreeNode.ChildNodes.Reset(0);

	// Ensure the DependencyArray is OK to work with.
	// Remove cycles
	DependencyArray.RemoveAllSwap([](const FChunkDependency& A){ return A.ChunkID == A.ParentChunkID; });
	// Add missing links (assumes they parent to chunk zero)
	for (uint32 i = 1; i < TotalChunks; ++i)
	{
		if (!DependencyArray.FindByPredicate([=](const FChunkDependency& RHS){ return i == RHS.ChunkID; }))
		{
			FChunkDependency Dep;
			Dep.ChunkID = i;
			Dep.ParentChunkID = 0;
			DependencyArray.Add(Dep);
		}
	}
	// Remove duplicates
	DependencyArray.StableSort([](const FChunkDependency& LHS, const FChunkDependency& RHS) { return LHS.ChunkID < RHS.ChunkID; });
	for (int32 i = 0; i < DependencyArray.Num() - 1;)
	{
		if (DependencyArray[i] == DependencyArray[i + 1])
		{
			DependencyArray.RemoveAt(i + 1);
		}
		else
		{
			++i;
		}
	}

	//
	AddChildrenRecursive(RootTreeNode, DependencyArray);
	return &RootTreeNode;
}

void UChunkDependencyInfo::AddChildrenRecursive(FChunkDependencyTreeNode& Node, TArray<FChunkDependency>& DepInfo)
{
	auto ChildNodeIndices = DepInfo.FilterByPredicate(
		[&](const FChunkDependency& RHS) 
		{
			return Node.ChunkID == RHS.ParentChunkID;
		});
	for (const auto& ChildIndex : ChildNodeIndices)
	{
		Node.ChildNodes.Add(FChunkDependencyTreeNode(ChildIndex.ChunkID));
	}
	for (auto& Child : Node.ChildNodes) 
	{
		AddChildrenRecursive(Child, DepInfo);
	}
}
