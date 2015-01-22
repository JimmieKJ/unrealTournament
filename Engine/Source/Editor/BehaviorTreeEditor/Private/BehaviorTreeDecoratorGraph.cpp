// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "BehaviorTreeEditorPrivatePCH.h"
#include "BehaviorTreeEditorModule.h"

//////////////////////////////////////////////////////////////////////////
// BehaviorTreeGraph

UBehaviorTreeDecoratorGraph::UBehaviorTreeDecoratorGraph(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Schema = UEdGraphSchema_BehaviorTreeDecorator::StaticClass();
}

void UBehaviorTreeDecoratorGraph::CollectDecoratorData(TArray<UBTDecorator*>& DecoratorInstances, TArray<FBTDecoratorLogic>& DecoratorOperations) const
{
	const UBehaviorTreeDecoratorGraphNode* RootNode = FindRootNode();
	CollectDecoratorDataWorker(RootNode, DecoratorInstances, DecoratorOperations);
}

const UBehaviorTreeDecoratorGraphNode* UBehaviorTreeDecoratorGraph::FindRootNode() const
{
	for (int32 i = 0; i < Nodes.Num(); i++)
	{
		const UBehaviorTreeDecoratorGraphNode_Logic* TestNode = Cast<const UBehaviorTreeDecoratorGraphNode_Logic>(Nodes[i]);
		if (TestNode && TestNode->LogicMode == EDecoratorLogicMode::Sink)
		{
			return TestNode;
		}
	}

	return NULL;
}

void UBehaviorTreeDecoratorGraph::CollectDecoratorDataWorker(const UBehaviorTreeDecoratorGraphNode* Node,
															 TArray<UBTDecorator*>& DecoratorInstances, TArray<FBTDecoratorLogic>& DecoratorOperations) const
{
	if (Node == NULL)
	{
		return;
	}

	TArray<const UBehaviorTreeDecoratorGraphNode*> LinkedNodes;
	for (int32 i = 0; i < Node->Pins.Num(); i++)
	{
		if (Node->Pins[i]->Direction == EGPD_Input &&
			Node->Pins[i]->LinkedTo.Num() > 0)
		{
			const UBehaviorTreeDecoratorGraphNode* LinkedNode = Cast<const UBehaviorTreeDecoratorGraphNode>(Node->Pins[i]->LinkedTo[0]->GetOwningNode());
			if (LinkedNode)
			{
				LinkedNodes.Add(LinkedNode);
			}
		}
	}

	FBTDecoratorLogic LogicOp(Node->GetOperationType(), LinkedNodes.Num());

	// special case: invalid
	if (LogicOp.Operation == EBTDecoratorLogic::Invalid)
	{
		// discard
	}
	// special case: test
	else if (LogicOp.Operation == EBTDecoratorLogic::Test)
	{
		// add decorator instance
		const UBehaviorTreeDecoratorGraphNode_Decorator* DecoratorNode = Cast<const UBehaviorTreeDecoratorGraphNode_Decorator>(Node);
		UBTDecorator* DecoratorInstance = DecoratorNode ? (UBTDecorator*)DecoratorNode->NodeInstance : NULL;
		if (DecoratorInstance)
		{
			LogicOp.Number = DecoratorInstances.Add(DecoratorInstance);
			DecoratorOperations.Add(LogicOp);
		}
	}
	else
	{
		DecoratorOperations.Add(LogicOp);
	}

	for (int32 i = 0; i < LinkedNodes.Num(); i++)
	{
		CollectDecoratorDataWorker(LinkedNodes[i], DecoratorInstances, DecoratorOperations);
	}
}
