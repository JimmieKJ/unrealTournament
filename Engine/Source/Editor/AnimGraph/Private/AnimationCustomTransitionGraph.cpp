// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimationCustomTransitionGraph.h"

#define LOCTEXT_NAMESPACE "AnimationCustomTransitionGraph"

/////////////////////////////////////////////////////
// UAnimationStateGraph

UAnimationCustomTransitionGraph::UAnimationCustomTransitionGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAnimGraphNode_CustomTransitionResult* UAnimationCustomTransitionGraph::GetResultNode()
{
	return MyResultNode;
}

#undef LOCTEXT_NAMESPACE
