// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "BehaviorTreeEditorPrivatePCH.h"
#include "BehaviorTreeEditorModule.h"
#include "BehaviorTree/Tasks/BTTask_RunBehavior.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTree.h"

//////////////////////////////////////////////////////////////////////////
// BehaviorTreeGraph

UBehaviorTreeGraph::UBehaviorTreeGraph(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Schema = UEdGraphSchema_BehaviorTree::StaticClass();
	bLockUpdates = false;
}

void UBehaviorTreeGraph::UpdateBlackboardChange()
{
	UBehaviorTree* BTAsset = Cast<UBehaviorTree>(GetOuter());
	if (BTAsset == nullptr)
	{
		return;
	}

	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		UBehaviorTreeGraphNode* MyNode = Cast<UBehaviorTreeGraphNode>(Nodes[Index]);

		if (MyNode)
		{
			UBTNode* MyNodeInstance = Cast<UBTNode>(MyNode->NodeInstance);
			if (MyNodeInstance)
			{
				MyNodeInstance->InitializeFromAsset(*BTAsset);
			}
		}

		for (int32 iDecorator = 0; iDecorator < MyNode->Decorators.Num(); iDecorator++)
		{
			UBTNode* MyNodeInstance = MyNode->Decorators[iDecorator] ? Cast<UBTNode>(MyNode->Decorators[iDecorator]->NodeInstance) : NULL;
			if (MyNodeInstance)
			{
				MyNodeInstance->InitializeFromAsset(*BTAsset);
			}

			UBehaviorTreeGraphNode_CompositeDecorator* CompDecoratorNode = Cast<UBehaviorTreeGraphNode_CompositeDecorator>(MyNode->Decorators[iDecorator]);
			if (CompDecoratorNode)
			{
				CompDecoratorNode->OnBlackboardUpdate();
			}
		}

		for (int32 iService = 0; iService < MyNode->Services.Num(); iService++)
		{
			UBTNode* MyNodeInstance = MyNode->Services[iService] ? Cast<UBTNode>(MyNode->Services[iService]->NodeInstance) : NULL;
			if (MyNodeInstance)
			{
				MyNodeInstance->InitializeFromAsset(*BTAsset);
			}
		}
	}
}

void UBehaviorTreeGraph::UpdateAsset(EDebuggerFlags DebuggerFlags, bool bBumpVersion)
{
	if (bLockUpdates)
	{
		return;
	}

	// initial cleanup & root node search
	UBehaviorTreeGraphNode_Root* RootNode = NULL;
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		UBehaviorTreeGraphNode* Node = Cast<UBehaviorTreeGraphNode>(Nodes[Index]);

		// debugger flags
		if (DebuggerFlags == ClearDebuggerFlags)
		{
			Node->ClearDebuggerState();

			for (int32 iAux = 0; iAux < Node->Services.Num(); iAux++)
			{
				Node->Services[iAux]->ClearDebuggerState();
			}

			for (int32 iAux = 0; iAux < Node->Decorators.Num(); iAux++)
			{
				Node->Decorators[iAux]->ClearDebuggerState();
			}
		}

		// parent chain
		Node->ParentNode = NULL;
		for (int32 iAux = 0; iAux < Node->Services.Num(); iAux++)
		{
			Node->Services[iAux]->ParentNode = Node;
		}

		for (int32 iAux = 0; iAux < Node->Decorators.Num(); iAux++)
		{
			Node->Decorators[iAux]->ParentNode = Node;
		}

		// prepare node instance
		UBTNode* NodeInstance = Cast<UBTNode>(Node->NodeInstance);
		if (NodeInstance)
		{
			// mark all nodes as disconnected first, path from root will replace it with valid values later
			NodeInstance->InitializeNode(NULL, MAX_uint16, 0, 0);
		}

		// cache root
		if (RootNode == NULL)
		{
			RootNode = Cast<UBehaviorTreeGraphNode_Root>(Nodes[Index]);
		}

		UBehaviorTreeGraphNode_CompositeDecorator* CompositeDecorator = Cast<UBehaviorTreeGraphNode_CompositeDecorator>(Nodes[Index]);
		if (CompositeDecorator)
		{
			CompositeDecorator->ResetExecutionRange();
		}
	}

	if (RootNode && RootNode->Pins.Num() > 0 && RootNode->Pins[0]->LinkedTo.Num() > 0)
	{
		UBehaviorTreeGraphNode* Node = Cast<UBehaviorTreeGraphNode>(RootNode->Pins[0]->LinkedTo[0]->GetOwningNode());
		if (Node)
		{
			CreateBTFromGraph(Node);

			if (bBumpVersion)
			{
				GraphVersion++;
			}
		}
	}
}

void UBehaviorTreeGraph::UpdatePinConnectionTypes()
{
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		UEdGraphNode* Node = Nodes[Index];

		const bool bIsCompositeNode = Node && Node->IsA(UBehaviorTreeGraphNode_Composite::StaticClass());

		for (int32 iPin = 0; iPin < Node->Pins.Num(); iPin++)
		{
			FString& PinCategory = Node->Pins[iPin]->PinType.PinCategory;
			if (PinCategory == TEXT("Transition"))
			{
				PinCategory = bIsCompositeNode ? 
					UBehaviorTreeEditorTypes::PinCategory_MultipleNodes :
					UBehaviorTreeEditorTypes::PinCategory_SingleComposite;
			}
		}
	}
}

void UBehaviorTreeGraph::ReplaceNodeConnections(UEdGraphNode* OldNode, UEdGraphNode* NewNode)
{
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		UEdGraphNode* Node = Nodes[Index];
		for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); PinIndex++)
		{
			UEdGraphPin* Pin = Node->Pins[PinIndex];
			for (int32 LinkedIndex = 0; LinkedIndex < Pin->LinkedTo.Num(); LinkedIndex++)
			{
				UEdGraphPin* LinkedPin = Pin->LinkedTo[LinkedIndex];
				const UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : NULL;

				if (LinkedNode == OldNode)
				{
					const int32 LinkedPinIndex = LinkedNode->Pins.IndexOfByKey(LinkedPin);
					Pin->LinkedTo[LinkedIndex] = NewNode->Pins[LinkedPinIndex];
				}
			}
		}
	}
}

void UBehaviorTreeGraph::UpdateDeprecatedNodes()
{
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		UBehaviorTreeGraphNode* Node = Cast<UBehaviorTreeGraphNode>(Nodes[Index]);
		if (Node)
		{
			// UBTTask_RunBehavior is now handled by dedicated graph node
			if (Node->NodeInstance && Node->NodeInstance->IsA(UBTTask_RunBehavior::StaticClass()))
			{
				UBehaviorTreeGraphNode* NewNode = Cast<UBehaviorTreeGraphNode>(StaticDuplicateObject(Node, this, TEXT(""), RF_AllFlags, UBehaviorTreeGraphNode_SubtreeTask::StaticClass()));
				check(NewNode);

				ReplaceNodeConnections(Node, NewNode);
				Nodes[Index] = NewNode;
				Node = NewNode;
			}

			if (Node->NodeInstance)
			{
				Node->ErrorMessage = FClassBrowseHelper::GetDeprecationMessage(Node->NodeInstance->GetClass());
			}

			for (int32 i = 0; i < Node->Decorators.Num(); i++)
			{
				if (Node->Decorators[i] && Node->Decorators[i]->NodeInstance)
				{
					Node->Decorators[i]->ErrorMessage = FClassBrowseHelper::GetDeprecationMessage(Node->Decorators[i]->NodeInstance->GetClass());
				}
			}

			for (int32 i = 0; i < Node->Services.Num(); i++)
			{
				if (Node->Services[i] && Node->Services[i]->NodeInstance)
				{
					Node->Services[i]->ErrorMessage = FClassBrowseHelper::GetDeprecationMessage(Node->Services[i]->NodeInstance->GetClass());
				}
			}
		}
	}
}

namespace BTGraphHelpers
{
	struct FIntIntPair
	{
		int32 FirstIdx;
		int32 LastIdx;
	};

	void CollectDecorators(UBehaviorTree* BTAsset,
		UBehaviorTreeGraphNode* GraphNode, TArray<UBTDecorator*>& DecoratorInstances, TArray<FBTDecoratorLogic>& DecoratorOperations,
		bool bInitializeNodes, UBTCompositeNode* RootNode, uint16* ExecutionIndex, uint8 TreeDepth, int32 ChildIdx)
	{
		TMap<UBehaviorTreeGraphNode_CompositeDecorator*, FIntIntPair> CompositeMap;
		int32 NumNodes = 0;

		for (int32 i = 0; i < GraphNode->Decorators.Num(); i++)
		{
			UBehaviorTreeGraphNode* MyNode = GraphNode->Decorators[i];
			if (MyNode == NULL || MyNode->bInjectedNode)
			{
				continue;
			}

			UBehaviorTreeGraphNode_Decorator* MyDecoratorNode = Cast<UBehaviorTreeGraphNode_Decorator>(MyNode);
			UBehaviorTreeGraphNode_CompositeDecorator* MyCompositeNode = Cast<UBehaviorTreeGraphNode_CompositeDecorator>(MyNode);

			if (MyDecoratorNode)
			{
				MyDecoratorNode->CollectDecoratorData(DecoratorInstances, DecoratorOperations);
				NumNodes++;
			}
			else if (MyCompositeNode)
			{
				MyCompositeNode->SetDecoratorData(RootNode, ChildIdx);

				FIntIntPair RangeData;
				RangeData.FirstIdx = DecoratorInstances.Num();

				MyCompositeNode->CollectDecoratorData(DecoratorInstances, DecoratorOperations);
				NumNodes++;

				RangeData.LastIdx = DecoratorInstances.Num() - 1;
				CompositeMap.Add(MyCompositeNode, RangeData);
			}
		}

		for (int32 i = 0; i < DecoratorInstances.Num(); i++)
		{
			if (DecoratorInstances[i] && BTAsset && Cast<UBehaviorTree>(DecoratorInstances[i]->GetOuter()) == NULL)
			{
				DecoratorInstances[i]->Rename(NULL, BTAsset);
			}

			DecoratorInstances[i]->InitializeNode(RootNode, *ExecutionIndex, 0, TreeDepth);
			if (bInitializeNodes)
			{
				DecoratorInstances[i]->InitializeDecorator(ChildIdx);
				*ExecutionIndex += 1;

				// make sure that flow abort mode matches - skip for root level nodes
				DecoratorInstances[i]->UpdateFlowAbortMode();
			}
		}

		if (bInitializeNodes)
		{
			// initialize composite decorators
			for (TMap<UBehaviorTreeGraphNode_CompositeDecorator*, FIntIntPair>::TIterator It(CompositeMap); It; ++It)
			{
				UBehaviorTreeGraphNode_CompositeDecorator* Node = It.Key();
				const FIntIntPair& PairInfo = It.Value();

				if (DecoratorInstances.IsValidIndex(PairInfo.FirstIdx) &&
					DecoratorInstances.IsValidIndex(PairInfo.LastIdx))
				{
					Node->FirstExecutionIndex = DecoratorInstances[PairInfo.FirstIdx]->GetExecutionIndex();
					Node->LastExecutionIndex = DecoratorInstances[PairInfo.LastIdx]->GetExecutionIndex();
				}
			}
		}

		// setup logic operations only when composite decorator is linked
		if (CompositeMap.Num())
		{
			if (NumNodes > 1)
			{
				FBTDecoratorLogic LogicOp(EBTDecoratorLogic::And, NumNodes);
				DecoratorOperations.Insert(LogicOp, 0);
			}
		}
		else
		{
			DecoratorOperations.Reset();
		}
	}

	void InitializeInjectedNodes(UBehaviorTreeGraphNode* GraphNode, UBTCompositeNode* RootNode, uint16 ExecutionIndex, uint8 TreeDepth, int32 ChildIdx)
	{
		TMap<UBehaviorTreeGraphNode_CompositeDecorator*, FIntIntPair> CompositeMap;
		TArray<UBTDecorator*> DecoratorInstances;
		TArray<FBTDecoratorLogic> DecoratorOperations;

		for (int32 i = 0; i < GraphNode->Decorators.Num(); i++)
		{
			UBehaviorTreeGraphNode* MyNode = GraphNode->Decorators[i];
			if (MyNode == NULL || !MyNode->bInjectedNode)
			{
				continue;
			}

			UBehaviorTreeGraphNode_Decorator* MyDecoratorNode = Cast<UBehaviorTreeGraphNode_Decorator>(MyNode);
			UBehaviorTreeGraphNode_CompositeDecorator* MyCompositeNode = Cast<UBehaviorTreeGraphNode_CompositeDecorator>(MyNode);

			if (MyDecoratorNode)
			{
				MyDecoratorNode->CollectDecoratorData(DecoratorInstances, DecoratorOperations);
			}
			else if (MyCompositeNode)
			{
				MyCompositeNode->SetDecoratorData(RootNode, ChildIdx);

				FIntIntPair RangeData;
				RangeData.FirstIdx = DecoratorInstances.Num();

				MyCompositeNode->CollectDecoratorData(DecoratorInstances, DecoratorOperations);

				RangeData.LastIdx = DecoratorInstances.Num() - 1;
				CompositeMap.Add(MyCompositeNode, RangeData);
			}
		}

		for (int32 i = 0; i < DecoratorInstances.Num(); i++)
		{
			DecoratorInstances[i]->InitializeNode(RootNode, ExecutionIndex, 0, TreeDepth);
			DecoratorInstances[i]->InitializeDecorator(ChildIdx);
			ExecutionIndex++;
		}

		// initialize composite decorators
		for (TMap<UBehaviorTreeGraphNode_CompositeDecorator*, FIntIntPair>::TIterator It(CompositeMap); It; ++It)
		{
			UBehaviorTreeGraphNode_CompositeDecorator* Node = It.Key();
			const FIntIntPair& PairInfo = It.Value();

			if (DecoratorInstances.IsValidIndex(PairInfo.FirstIdx) &&
				DecoratorInstances.IsValidIndex(PairInfo.LastIdx))
			{
				Node->FirstExecutionIndex = DecoratorInstances[PairInfo.FirstIdx]->GetExecutionIndex();
				Node->LastExecutionIndex = DecoratorInstances[PairInfo.LastIdx]->GetExecutionIndex();
			}
		}
	}

	void VerifyDecorators(UBehaviorTreeGraphNode* GraphNode)
	{
		TArray<UBTDecorator*> DecoratorInstances;
		TArray<FBTDecoratorLogic> DecoratorOperations;

		for (int32 i = 0; i < GraphNode->Decorators.Num(); i++)
		{
			UBehaviorTreeGraphNode* MyNode = GraphNode->Decorators[i];
			if (MyNode == NULL)
			{
				continue;
			}

			DecoratorInstances.Reset();
			DecoratorOperations.Reset();

			UBehaviorTreeGraphNode_Decorator* MyDecoratorNode = Cast<UBehaviorTreeGraphNode_Decorator>(MyNode);
			UBehaviorTreeGraphNode_CompositeDecorator* MyCompositeNode = Cast<UBehaviorTreeGraphNode_CompositeDecorator>(MyNode);

			if (MyDecoratorNode)
			{
				MyDecoratorNode->CollectDecoratorData(DecoratorInstances, DecoratorOperations);
			}
			else if (MyCompositeNode)
			{
				MyCompositeNode->CollectDecoratorData(DecoratorInstances, DecoratorOperations);
			}

			MyNode->bHasObserverError = false;
			for (int32 SubIdx = 0; SubIdx < DecoratorInstances.Num(); SubIdx++)
			{
				MyNode->bHasObserverError = MyNode->bHasObserverError || !DecoratorInstances[SubIdx]->IsFlowAbortModeValid();
			}
		}
	}

	void CreateChildren(UBehaviorTree* BTAsset, UBTCompositeNode* RootNode, const UBehaviorTreeGraphNode* RootEdNode, uint16* ExecutionIndex, uint8 TreeDepth)
	{
		if (RootEdNode == NULL)
		{
			return;
		}

		RootNode->Children.Reset();
		RootNode->Services.Reset();

		// collect services
		if (RootEdNode->Services.Num())
		{
			for (int32 i = 0; i < RootEdNode->Services.Num(); i++)
			{
				UBTService* ServiceInstance = RootEdNode->Services[i] ? Cast<UBTService>(RootEdNode->Services[i]->NodeInstance) : NULL;
				if (ServiceInstance)
				{
					if (Cast<UBehaviorTree>(ServiceInstance->GetOuter()) == NULL)
					{
						ServiceInstance->Rename(NULL, BTAsset);
					}
					ServiceInstance->InitializeNode(RootNode, *ExecutionIndex, 0, TreeDepth);
					*ExecutionIndex += 1;

					RootNode->Services.Add(ServiceInstance);
				}
			}
		}

		// gather all nodes
		int32 ChildIdx = 0;
		for (int32 PinIdx = 0; PinIdx < RootEdNode->Pins.Num(); PinIdx++)
		{
			UEdGraphPin* Pin = RootEdNode->Pins[PinIdx];
			if (Pin->Direction != EGPD_Output)
			{
				continue;
			}

			// sort connections so that they're organized the same as user can see in the editor
			Pin->LinkedTo.Sort(FCompareNodeXLocation());

			for (int32 Index = 0; Index < Pin->LinkedTo.Num(); ++Index)
			{
				UBehaviorTreeGraphNode* GraphNode = Cast<UBehaviorTreeGraphNode>(Pin->LinkedTo[Index]->GetOwningNode());
				if (GraphNode == NULL)
				{
					continue;
				}

				UBTTaskNode* TaskInstance = Cast<UBTTaskNode>(GraphNode->NodeInstance);
				if (TaskInstance && Cast<UBehaviorTree>(TaskInstance->GetOuter()) == NULL)
				{
					TaskInstance->Rename(NULL, BTAsset);
				}

				UBTCompositeNode* CompositeInstance = Cast<UBTCompositeNode>(GraphNode->NodeInstance);
				if (CompositeInstance && Cast<UBehaviorTree>(CompositeInstance->GetOuter()) == NULL)
				{
					CompositeInstance->Rename(NULL, BTAsset);
				}

				if (TaskInstance == NULL && CompositeInstance == NULL)
				{
					continue;
				}

				// collect decorators
				TArray<UBTDecorator*> DecoratorInstances;
				TArray<FBTDecoratorLogic> DecoratorOperations;
				CollectDecorators(BTAsset, GraphNode, DecoratorInstances, DecoratorOperations, true, RootNode, ExecutionIndex, TreeDepth, ChildIdx);

				// store child data
				RootNode->Children.AddZeroed();
				FBTCompositeChild& ChildInfo = RootNode->Children[ChildIdx];
				ChildInfo.ChildComposite = CompositeInstance;
				ChildInfo.ChildTask = TaskInstance;
				ChildInfo.Decorators = DecoratorInstances;
				ChildInfo.DecoratorOps = DecoratorOperations;
				ChildIdx++;

				UBTNode* ChildNode = CompositeInstance ? (UBTNode*)CompositeInstance : (UBTNode*)TaskInstance;
				if (ChildNode && Cast<UBehaviorTree>(ChildNode->GetOuter()) == NULL)
				{
					ChildNode->Rename(NULL, BTAsset);
				}

				InitializeInjectedNodes(GraphNode, RootNode, *ExecutionIndex, TreeDepth, ChildIdx);
				
				// special case: subtrees
				UBTTask_RunBehavior* SubtreeTask = Cast<UBTTask_RunBehavior>(TaskInstance);
				if (SubtreeTask)
				{
					*ExecutionIndex += SubtreeTask->GetInjectedNodesCount();
				}

				ChildNode->InitializeNode(RootNode, *ExecutionIndex, 0, TreeDepth);
				*ExecutionIndex += 1;

				VerifyDecorators(GraphNode);

				if (CompositeInstance)
				{
					CreateChildren(BTAsset, CompositeInstance, GraphNode, ExecutionIndex, TreeDepth + 1);

					CompositeInstance->InitializeComposite((*ExecutionIndex) - 1);
				}
			}
		}
	}

	void ClearRootLevelFlags(UBehaviorTreeGraph* Graph)
	{
		for (int32 Index = 0; Index < Graph->Nodes.Num(); Index++)
		{
			UBehaviorTreeGraphNode* BTNode = Cast<UBehaviorTreeGraphNode>(Graph->Nodes[Index]);
			if (BTNode)
			{
				BTNode->bRootLevel = false;

				for (int32 SubIndex = 0; SubIndex < BTNode->Decorators.Num(); SubIndex++)
				{
					UBehaviorTreeGraphNode* SubNode = BTNode->Decorators[SubIndex];
					if (SubNode)
					{
						SubNode->bRootLevel = false;
					}
				}
			}
		}
	}

	void RebuildExecutionOrder(UBehaviorTreeGraphNode* RootEdNode, UBTCompositeNode* RootNode, uint16* ExecutionIndex, uint8 TreeDepth)
	{
		if (RootEdNode == NULL || RootEdNode == NULL)
		{
			return;
		}

		// collect services
		if (RootEdNode->Services.Num())
		{
			for (int32 i = 0; i < RootEdNode->Services.Num(); i++)
			{
				UBTService* ServiceInstance = RootEdNode->Services[i] ? Cast<UBTService>(RootEdNode->Services[i]->NodeInstance) : NULL;
				if (ServiceInstance)
				{
					ServiceInstance->InitializeNode(RootNode, *ExecutionIndex, 0, TreeDepth);
					*ExecutionIndex += 1;
				}
			}
		}

		// gather all nodes
		int32 ChildIdx = 0;
		for (int32 PinIdx = 0; PinIdx < RootEdNode->Pins.Num(); PinIdx++)
		{
			UEdGraphPin* Pin = RootEdNode->Pins[PinIdx];
			if (Pin->Direction != EGPD_Output)
			{
				continue;
			}

			// sort connections so that they're organized the same as user can see in the editor
			TArray<UEdGraphPin*> SortedPins = Pin->LinkedTo;
			SortedPins.Sort(FCompareNodeXLocation());

			for (int32 Index = 0; Index < SortedPins.Num(); ++Index)
			{
				UBehaviorTreeGraphNode* GraphNode = Cast<UBehaviorTreeGraphNode>(SortedPins[Index]->GetOwningNode());
				if (GraphNode == NULL)
				{
					continue;
				}

				UBTTaskNode* TaskInstance = Cast<UBTTaskNode>(GraphNode->NodeInstance);
				UBTCompositeNode* CompositeInstance = Cast<UBTCompositeNode>(GraphNode->NodeInstance);
				UBTNode* ChildNode = CompositeInstance ? (UBTNode*)CompositeInstance : (UBTNode*)TaskInstance;
				if (ChildNode == NULL)
				{
					continue;
				}

				// collect decorators
				TArray<UBTDecorator*> DecoratorInstances;
				TArray<FBTDecoratorLogic> DecoratorOperations;
				CollectDecorators(NULL, GraphNode, DecoratorInstances, DecoratorOperations, true, RootNode, ExecutionIndex, TreeDepth, ChildIdx);


				InitializeInjectedNodes(GraphNode, RootNode, *ExecutionIndex, TreeDepth, ChildIdx);

				// special case: subtrees
				UBTTask_RunBehavior* SubtreeTask = Cast<UBTTask_RunBehavior>(TaskInstance);
				if (SubtreeTask)
				{
					*ExecutionIndex += SubtreeTask->GetInjectedNodesCount();
				}

				ChildNode->InitializeNode(RootNode, *ExecutionIndex, 0, TreeDepth);
				*ExecutionIndex += 1;
				ChildIdx++;

				if (CompositeInstance)
				{
					RebuildExecutionOrder(GraphNode, CompositeInstance, ExecutionIndex, TreeDepth + 1);
					CompositeInstance->InitializeComposite((*ExecutionIndex) - 1);
				}
			}
		}
	}

} // namespace BTGraphHelpers

void UBehaviorTreeGraph::CreateBTFromGraph(UBehaviorTreeGraphNode* RootEdNode)
{
	UBehaviorTree* BTAsset = Cast<UBehaviorTree>(GetOuter());
	BTAsset->RootNode = NULL; //discard old tree

	// let's create new tree from graph
	uint16 ExecutionIndex = 0;
	uint8 TreeDepth = 0;

	BTAsset->RootNode = Cast<UBTCompositeNode>(RootEdNode->NodeInstance);
	if (BTAsset->RootNode)
	{
		BTAsset->RootNode->InitializeNode(NULL, ExecutionIndex, 0, TreeDepth);
		ExecutionIndex++;
	}

	// collect root level decorators
	uint16 DummyIndex = MAX_uint16;
	BTAsset->RootDecorators.Empty();
	BTAsset->RootDecoratorOps.Empty();
	BTGraphHelpers::CollectDecorators(BTAsset, RootEdNode, BTAsset->RootDecorators, BTAsset->RootDecoratorOps, false, NULL, &DummyIndex, 0, 0);

	// connect tree nodes
	BTGraphHelpers::CreateChildren(BTAsset, BTAsset->RootNode, RootEdNode, &ExecutionIndex, TreeDepth + 1);

	// mark root level nodes
	BTGraphHelpers::ClearRootLevelFlags(this);

	RootEdNode->bRootLevel = true;
	for (int32 Index = 0; Index < RootEdNode->Decorators.Num(); Index++)
	{
		UBehaviorTreeGraphNode* Node = RootEdNode->Decorators[Index];
		if (Node)
		{
			Node->bRootLevel = true;
		}
	}

	if (BTAsset->RootNode)
	{
		BTAsset->RootNode->InitializeComposite(ExecutionIndex - 1);
	}

	// Now remove any orphaned nodes left behind after regeneration
	RemoveOrphanedNodes();
}

void UBehaviorTreeGraph::RemoveOrphanedNodes()
{
	UBehaviorTree* BTAsset = CastChecked<UBehaviorTree>(GetOuter());

	// Obtain a list of all nodes that should be in the asset
	TSet<UBTNode*> AllNodes;
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		UBehaviorTreeGraphNode* MyNode = Cast<UBehaviorTreeGraphNode>(Nodes[Index]);
		if (MyNode)
		{
			UBTNode* MyNodeInstance = Cast<UBTNode>(MyNode->NodeInstance);
			if (MyNodeInstance)
			{
				AllNodes.Add(MyNodeInstance);
			}

			for (int32 iDecorator = 0; iDecorator < MyNode->Decorators.Num(); iDecorator++)
			{
				UBehaviorTreeGraphNode_CompositeDecorator* SubgraphNode = Cast<UBehaviorTreeGraphNode_CompositeDecorator>(MyNode->Decorators[iDecorator]);
				if (SubgraphNode)
				{
					TArray<UBTDecorator*> NodeInstances;
					TArray<FBTDecoratorLogic> DummyOps;
					SubgraphNode->CollectDecoratorData(NodeInstances, DummyOps);

					for (int32 SubIdx = 0; SubIdx < NodeInstances.Num(); SubIdx++)
					{
						AllNodes.Add(NodeInstances[SubIdx]);
					}
				}
				else
				{
					UBTNode* MyDecoratorNodeInstance = MyNode->Decorators[iDecorator] ? Cast<UBTNode>(MyNode->Decorators[iDecorator]->NodeInstance) : NULL;
					if (MyDecoratorNodeInstance)
					{
						AllNodes.Add(MyDecoratorNodeInstance);
					}
				}
			}

			for (int32 iService = 0; iService < MyNode->Services.Num(); iService++)
			{
				UBTNode* MyServiceNodeInstance = MyNode->Services[iService] ? Cast<UBTNode>(MyNode->Services[iService]->NodeInstance) : NULL;
				if (MyServiceNodeInstance)
				{
					AllNodes.Add(MyServiceNodeInstance);
				}
			}
		}
	}

	// Obtain a list of all nodes actually in the asset and discard unused nodes
	TArray<UObject*> AllInners;
	const bool bIncludeNestedObjects = false;
	GetObjectsWithOuter(BTAsset, AllInners, bIncludeNestedObjects);
	for (auto InnerIt = AllInners.CreateConstIterator(); InnerIt; ++InnerIt)
	{
		UBTNode* Node = Cast<UBTNode>(*InnerIt);
		if (Node && !AllNodes.Contains(Node))
		{
			Node->SetFlags(RF_Transient);
			Node->Rename(NULL, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);
		}
	}
}

void UBehaviorTreeGraph::UpdateAbortHighlight(struct FAbortDrawHelper& Mode0, struct FAbortDrawHelper& Mode1)
{
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		UBehaviorTreeGraphNode* Node = Cast<UBehaviorTreeGraphNode>(Nodes[Index]);
		UBTNode* NodeInstance = Node ? Cast<UBTNode>(Node->NodeInstance) : NULL;
		if (NodeInstance)
		{
			const uint16 ExecIndex = NodeInstance->GetExecutionIndex();

			Node->bHighlightInAbortRange0 = (ExecIndex != MAX_uint16) && (ExecIndex >= Mode0.AbortStart) && (ExecIndex <= Mode0.AbortEnd);
			Node->bHighlightInAbortRange1 = (ExecIndex != MAX_uint16) && (ExecIndex >= Mode1.AbortStart) && (ExecIndex <= Mode1.AbortEnd);
			Node->bHighlightInSearchRange0 = (ExecIndex != MAX_uint16) && (ExecIndex >= Mode0.SearchStart) && (ExecIndex <= Mode0.SearchEnd);
			Node->bHighlightInSearchRange1 = (ExecIndex != MAX_uint16) && (ExecIndex >= Mode1.SearchStart) && (ExecIndex <= Mode1.SearchEnd);
			Node->bHighlightInSearchTree = false;
		}
	}
}

bool UBehaviorTreeGraph::UpdateUnknownNodeClasses()
{
	bool bUpdated = false;
	for (int32 NodeIdx = 0; NodeIdx < Nodes.Num(); NodeIdx++)
	{
		UBehaviorTreeGraphNode* MyNode = Cast<UBehaviorTreeGraphNode>(Nodes[NodeIdx]);
		const bool bUpdatedNode = MyNode->RefreshNodeClass();
		bUpdated = bUpdated || bUpdatedNode;

		for (int32 SubNodeIdx = 0; SubNodeIdx < MyNode->Decorators.Num(); SubNodeIdx++)
		{
			if (MyNode->Decorators[SubNodeIdx])
			{
				const bool bUpdatedSubNode = MyNode->Decorators[SubNodeIdx]->RefreshNodeClass();
				bUpdated = bUpdated || bUpdatedSubNode;
			}
		}

		for (int32 SubNodeIdx = 0; SubNodeIdx < MyNode->Services.Num(); SubNodeIdx++)
		{
			if (MyNode->Services[SubNodeIdx])
			{
				const bool bUpdatedSubNode = MyNode->Services[SubNodeIdx]->RefreshNodeClass();
				bUpdated = bUpdated || bUpdatedSubNode;
			}
		}
	}

	return bUpdated;
}

bool UBehaviorTreeGraph::UpdateInjectedNodes()
{
	bool bHasUpdated = false;
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		UBehaviorTreeGraphNode_SubtreeTask* Node = Cast<UBehaviorTreeGraphNode_SubtreeTask>(Nodes[Index]);
		if (Node)
		{
			const bool bUpdatedSubTree = Node->UpdateInjectedNodes();
			bHasUpdated = bHasUpdated || bUpdatedSubTree;
		}
	}

	return bHasUpdated;
}

UEdGraphNode* UBehaviorTreeGraph::FindInjectedNode(int32 Index)
{
	for (int32 NodeIdx = 0; NodeIdx < Nodes.Num(); NodeIdx++)
	{
		UBehaviorTreeGraphNode* MyNode = Cast<UBehaviorTreeGraphNode>(Nodes[NodeIdx]);
		if (MyNode && MyNode->bRootLevel)
		{
			return MyNode->Decorators.IsValidIndex(Index) ? MyNode->Decorators[Index] : NULL;
		}
	}

	return NULL;
}

void UBehaviorTreeGraph::RebuildExecutionOrder()
{
	// initial cleanup & root node search
	UBehaviorTreeGraphNode_Root* RootNode = NULL;
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		UBehaviorTreeGraphNode* Node = Cast<UBehaviorTreeGraphNode>(Nodes[Index]);

		// prepare node instance
		UBTNode* NodeInstance = Cast<UBTNode>(Node->NodeInstance);
		if (NodeInstance)
		{
			// mark all nodes as disconnected first, path from root will replace it with valid values later
			NodeInstance->InitializeNode(NULL, MAX_uint16, 0, 0);
		}

		// cache root
		if (RootNode == NULL)
		{
			RootNode = Cast<UBehaviorTreeGraphNode_Root>(Nodes[Index]);
		}

		UBehaviorTreeGraphNode_CompositeDecorator* CompositeDecorator = Cast<UBehaviorTreeGraphNode_CompositeDecorator>(Nodes[Index]);
		if (CompositeDecorator)
		{
			CompositeDecorator->ResetExecutionRange();
		}
	}

	if (RootNode && RootNode->Pins.Num() > 0 && RootNode->Pins[0]->LinkedTo.Num() > 0)
	{
		UBehaviorTreeGraphNode* Node = Cast<UBehaviorTreeGraphNode>(RootNode->Pins[0]->LinkedTo[0]->GetOwningNode());
		if (Node)
		{
			UBTCompositeNode* BTNode = Cast<UBTCompositeNode>(Node->NodeInstance);
			if (BTNode)
			{
				uint16 ExecutionIndex = 0;
				uint8 TreeDepth = 0;

				BTNode->InitializeNode(NULL, ExecutionIndex, 0, TreeDepth);
				ExecutionIndex++;

				BTGraphHelpers::RebuildExecutionOrder(Node, BTNode, &ExecutionIndex, TreeDepth);
			}
		}
	}
}

void UBehaviorTreeGraph::LockUpdates()
{
	bLockUpdates = true;
}

void UBehaviorTreeGraph::UnlockUpdates()
{
	bLockUpdates = false;

	UpdateAsset(EDebuggerFlags::SkipDebuggerFlags);
}
