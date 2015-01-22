// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EdGraph/EdGraph.h"
#include "AnimationStateMachineGraph.generated.h"

UCLASS(MinimalAPI)
class UAnimationStateMachineGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

	// Entry node within the state machine
	UPROPERTY()
	class UAnimStateEntryNode* EntryNode;

	// Parent instance node
	UPROPERTY()
	class UAnimGraphNode_StateMachineBase* OwnerAnimGraphNode;
};

