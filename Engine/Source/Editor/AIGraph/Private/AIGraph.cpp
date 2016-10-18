// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AIGraphPrivatePCH.h"
#include "AIGraphModule.h"

UAIGraph::UAIGraph(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bLockUpdates = false;
}

void UAIGraph::UpdateAsset(int32 UpdateFlags)
{
	// empty in base class
}

void UAIGraph::OnCreated()
{
	MarkVersion();
}

void UAIGraph::OnLoaded()
{
	UpdateDeprecatedClasses();
	UpdateUnknownNodeClasses();
}

void UAIGraph::Initialize()
{
	UpdateVersion();
}

void UAIGraph::UpdateVersion()
{
	if (GraphVersion == 1)
	{
		return;
	}

	MarkVersion();
	Modify();
}

void UAIGraph::MarkVersion()
{
	GraphVersion = 1;
}

bool UAIGraph::UpdateUnknownNodeClasses()
{
	bool bUpdated = false;
	for (int32 NodeIdx = 0; NodeIdx < Nodes.Num(); NodeIdx++)
	{
		UAIGraphNode* MyNode = Cast<UAIGraphNode>(Nodes[NodeIdx]);
		if (MyNode)
		{
			const bool bUpdatedNode = MyNode->RefreshNodeClass();
			bUpdated = bUpdated || bUpdatedNode;

			for (int32 SubNodeIdx = 0; SubNodeIdx < MyNode->SubNodes.Num(); SubNodeIdx++)
			{
				if (MyNode->SubNodes[SubNodeIdx])
				{
					const bool bUpdatedSubNode = MyNode->SubNodes[SubNodeIdx]->RefreshNodeClass();
					bUpdated = bUpdated || bUpdatedSubNode;
				}
			}
		}
	}

	return bUpdated;
}

void UAIGraph::UpdateDeprecatedClasses()
{
	for (int32 Idx = 0; Idx < Nodes.Num(); Idx++)
	{
		UAIGraphNode* Node = Cast<UAIGraphNode>(Nodes[Idx]);
		if (Node)
		{
			if (Node->NodeInstance)
			{
				Node->ErrorMessage = FGraphNodeClassHelper::GetDeprecationMessage(Node->NodeInstance->GetClass());
			}

			for (int32 SubIdx = 0; SubIdx < Node->SubNodes.Num(); SubIdx++)
			{
				if (Node->SubNodes[SubIdx] && Node->SubNodes[SubIdx]->NodeInstance)
				{
					Node->SubNodes[SubIdx]->ErrorMessage = FGraphNodeClassHelper::GetDeprecationMessage(Node->SubNodes[SubIdx]->NodeInstance->GetClass());
				}
			}
		}
	}
}

void UAIGraph::UpdateClassData()
{
	for (int32 Idx = 0; Idx < Nodes.Num(); Idx++)
	{
		UAIGraphNode* Node = Cast<UAIGraphNode>(Nodes[Idx]);
		if (Node)
		{
			Node->UpdateNodeClassData();

			for (int32 SubIdx = 0; SubIdx < Node->SubNodes.Num(); SubIdx++)
			{
				if (Node->SubNodes[SubIdx])
				{
					Node->UpdateNodeClassData();
				}
			}
		}
	}
}

void UAIGraph::CollectAllNodeInstances(TSet<UObject*>& NodeInstances)
{
	for (int32 Idx = 0; Idx < Nodes.Num(); Idx++)
	{
		UAIGraphNode* MyNode = Cast<UAIGraphNode>(Nodes[Idx]);
		if (MyNode)
		{
			NodeInstances.Add(MyNode->NodeInstance);

			for (int32 SubIdx = 0; SubIdx < MyNode->SubNodes.Num(); SubIdx++)
			{
				if (MyNode->SubNodes[SubIdx])
				{
					NodeInstances.Add(MyNode->SubNodes[SubIdx]->NodeInstance);
				}
			}
		}
	}
}

bool UAIGraph::CanRemoveNestedObject(UObject* TestObject) const
{
	return !TestObject->IsA(UEdGraphNode::StaticClass()) &&
		!TestObject->IsA(UEdGraph::StaticClass()) &&
		!TestObject->IsA(UEdGraphSchema::StaticClass());
}

void UAIGraph::RemoveOrphanedNodes()
{
	TSet<UObject*> NodeInstances;
	CollectAllNodeInstances(NodeInstances);

	NodeInstances.Remove(nullptr);

	// Obtain a list of all nodes actually in the asset and discard unused nodes
	TArray<UObject*> AllInners;
	const bool bIncludeNestedObjects = false;
	GetObjectsWithOuter(GetOuter(), AllInners, bIncludeNestedObjects);
	for (auto InnerIt = AllInners.CreateConstIterator(); InnerIt; ++InnerIt)
	{
		UObject* TestObject = *InnerIt;
		if (!NodeInstances.Contains(TestObject) && CanRemoveNestedObject(TestObject))
		{
			OnNodeInstanceRemoved(TestObject);

			TestObject->SetFlags(RF_Transient);
			TestObject->Rename(NULL, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);
		}
	}
}

void UAIGraph::OnNodeInstanceRemoved(UObject* NodeInstance)
{
	// empty in base class
}

void UAIGraph::OnNodesPasted(const FString& ImportStr)
{
	// empty in base class
}

UEdGraphPin* UAIGraph::FindGraphNodePin(UEdGraphNode* Node, EEdGraphPinDirection Dir)
{
	UEdGraphPin* Pin = nullptr;
	for (int32 Idx = 0; Idx < Node->Pins.Num(); Idx++)
	{
		if (Node->Pins[Idx]->Direction == Dir)
		{
			Pin = Node->Pins[Idx];
			break;
		}
	}

	return Pin;
}

bool UAIGraph::IsLocked() const
{
	return bLockUpdates;
}

void UAIGraph::LockUpdates()
{
	bLockUpdates = true;
}

void UAIGraph::UnlockUpdates()
{
	bLockUpdates = false;
	UpdateAsset();
}

void UAIGraph::OnSubNodeDropped()
{
	NotifyGraphChanged();
}
