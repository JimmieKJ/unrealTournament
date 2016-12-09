// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ChunkDependencyInfo.generated.h"

USTRUCT()
struct FChunkDependency
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = ChunkInfo)
	int32 ChunkID;

	UPROPERTY(EditAnywhere, Category = ChunkInfo)
	int32 ParentChunkID;

	bool operator == (const FChunkDependency& RHS) 
	{
		return ChunkID == RHS.ChunkID;
	}
};

struct FChunkDependencyTreeNode
{
	FChunkDependencyTreeNode(int32 InChunkID=0)
		: ChunkID(InChunkID)
	{}

	int32 ChunkID;

	TArray<FChunkDependencyTreeNode> ChildNodes;
};

UCLASS(config=Engine, defaultconfig)
class UNREALED_API UChunkDependencyInfo : public UObject
{
	GENERATED_UCLASS_BODY()

	const FChunkDependencyTreeNode* GetChunkDependencyGraph(uint32 TotalChunks);

public:

	void AddChildrenRecursive(FChunkDependencyTreeNode& Node, TArray<FChunkDependency>& DepInfo);

	UPROPERTY(config)
	TArray<FChunkDependency> DependencyArray;

	FChunkDependencyTreeNode RootTreeNode;
};
