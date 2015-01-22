// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AnimationGraph.h"
#include "AnimationStateGraph.generated.h"

UCLASS(MinimalAPI)
class UAnimationStateGraph : public UAnimationGraph
{
	GENERATED_UCLASS_BODY()

	// Result node within the state's animation graph
	UPROPERTY()
	class UAnimGraphNode_StateResult* MyResultNode;

	ANIMGRAPH_API class UAnimGraphNode_StateResult* GetResultNode();
};

