// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "EdGraphUtilities.h"
#include "Factories.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "UnrealExporter.h"

/////////////////////////////////////////////////////
// FGraphObjectTextFactory

/** Helper class used to paste a buffer of text and create nodes and pins from it */
struct FGraphObjectTextFactory : public FCustomizableTextObjectFactory
{
public:
	TSet<UEdGraphNode*> SpawnedNodes;
	const UEdGraph* DestinationGraph;
public:
	FGraphObjectTextFactory(const UEdGraph* InDestinationGraph)
		: FCustomizableTextObjectFactory(GWarn)
		, DestinationGraph(InDestinationGraph)
	{
	}

protected:
	virtual bool CanCreateClass(UClass* ObjectClass) const override
	{
		if (const UEdGraphNode* DefaultNode = Cast<UEdGraphNode>(ObjectClass->GetDefaultObject()))
		{
			if (DefaultNode->CanDuplicateNode())
			{
				if (DestinationGraph != NULL)
				{
					if (DefaultNode->CanCreateUnderSpecifiedSchema(DestinationGraph->GetSchema()))
					{
						return true;
					}
				}
				else
				{
					return true;
				}
			}
		}

		return false;
	}

	virtual void ProcessConstructedObject(UObject* CreatedObject) override
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(CreatedObject))
		{
			if(!Node->CanPasteHere(DestinationGraph))
			{
				// Attempt to create a substitute node if it cannot be pasted (note: the return value can be NULL, indicating that the node cannot be pasted into the graph)
				Node = DestinationGraph->GetSchema()->CreateSubstituteNode(Node, DestinationGraph, &InstanceGraph);
			}

			if(Node != CreatedObject)
			{
				// Move the old node into the transient package so that it is GC'd
				CreatedObject->Rename(NULL, GetTransientPackage());
			}

			if(Node)
			{
				SpawnedNodes.Add(Node);

				Node->GetGraph()->Nodes.Add(Node);
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// FEdGraphUtilities

TArray< TSharedPtr<FGraphPanelNodeFactory> > FEdGraphUtilities::VisualNodeFactories;
TArray< TSharedPtr<FGraphPanelPinFactory> > FEdGraphUtilities::VisualPinFactories;

// Reconcile other pin links:
//   - Links between nodes within the copied set are fine
//   - Links to nodes that were not copied need to be fixed up if the copy-paste was in the same graph or broken completely
// Call PostPasteNode on each node
void FEdGraphUtilities::PostProcessPastedNodes(TSet<UEdGraphNode*>& SpawnedNodes)
{
	// Run thru and fix up the node's pin links; they may point to invalid pins if the paste was to another graph
	for (TSet<UEdGraphNode*>::TIterator It(SpawnedNodes); It; ++It)
	{
		UEdGraphNode* Node = *It;
		UEdGraph* CurrentGraph = Node->GetGraph();

		for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
		{
			UEdGraphPin* ThisPin = Node->Pins[PinIndex];

			for (int32 LinkIndex = 0; LinkIndex < ThisPin->LinkedTo.Num(); )
			{
				UEdGraphPin* OtherPin = ThisPin->LinkedTo[LinkIndex];

				if (OtherPin == NULL)
				{
					// Totally bogus link
					ThisPin->LinkedTo.RemoveAtSwap(LinkIndex);
				}
				else if (!SpawnedNodes.Contains(OtherPin->GetOwningNode()))
				{
					// It's a link across the selection set, so it should be broken
					OtherPin->LinkedTo.RemoveSwap(ThisPin);
					ThisPin->LinkedTo.RemoveAtSwap(LinkIndex);
				}
				else if (!OtherPin->LinkedTo.Contains(ThisPin))
				{
					// The link needs to be reciprocal
					check(OtherPin->GetOwningNode()->GetGraph() == CurrentGraph);
					OtherPin->LinkedTo.Add(ThisPin);
					++LinkIndex;
				}
				else
				{
					// Everything seems fine but sanity check the graph
					check(OtherPin->GetOwningNode()->GetGraph() == CurrentGraph);
					++LinkIndex;
				}
			}
		}
	}

	// Give every node a chance to deep copy associated resources, etc...
	for (TSet<UEdGraphNode*>::TIterator It(SpawnedNodes); It; ++It)
	{
		UEdGraphNode* Node = *It;

		Node->PostPasteNode();
		Node->ReconstructNode();

		// Ensure we have RF_Transactional set on all pasted nodes, as its not copied in the T3D format
		Node->SetFlags(RF_Transactional);
	}
}

UEdGraphPin* FEdGraphUtilities::GetNetFromPin(UEdGraphPin* Pin)
{
	if ((Pin->Direction == EGPD_Input) && (Pin->LinkedTo.Num() > 0))
	{
		return Pin->LinkedTo[0];
	}
	else
	{
		return Pin;
	}
}

// Clones (deep copies) a UEdGraph, including all of it's nodes and pins and their links,
// maintaining a mapping from the clone to the source nodes (even across multiple clonings)
UEdGraph* FEdGraphUtilities::CloneGraph(UEdGraph* InSource, UObject* NewOuter, FCompilerResultsLog* MessageLog, bool bCloningForCompile)
{
	// Duplicate the graph, keeping track of what was duplicated
	TMap<UObject*, UObject*> DuplicatedObjectList;

	UObject* UseOuter = (NewOuter != NULL) ? NewOuter : GetTransientPackage();
	FObjectDuplicationParameters Parameters(InSource, UseOuter);
	Parameters.CreatedObjects = &DuplicatedObjectList;

	if (bCloningForCompile || (NewOuter == NULL))
	{
		Parameters.ApplyFlags |= RF_Transient;
	}

	UEdGraph* ClonedGraph = CastChecked<UEdGraph>(StaticDuplicateObjectEx(Parameters));

	// Store backtrack links from each duplicated object to the original source object
	if (MessageLog != NULL)
	{
		for (TMap<UObject*, UObject*>::TIterator It(DuplicatedObjectList); It; ++It)
		{
			UObject* const Source = It.Key();
			UObject* const Dest = It.Value();

			MessageLog->NotifyIntermediateObjectCreation(Dest, Source);
		}
	}

	return ClonedGraph;
}

// Clones the content from SourceGraph and merges it into MergeTarget; including merging/flattening all of the children from the SourceGraph into MergeTarget
void FEdGraphUtilities::CloneAndMergeGraphIn(UEdGraph* MergeTarget, UEdGraph* SourceGraph, FCompilerResultsLog& MessageLog, bool bRequireSchemaMatch, bool bInIsCompiling/* = false*/, TArray<UEdGraphNode*>* OutClonedNodes)
{
	// Clone the graph, then move all of it's children
	UEdGraph* ClonedGraph = CloneGraph(SourceGraph, NULL, &MessageLog, true);
	MergeChildrenGraphsIn(ClonedGraph, ClonedGraph, bRequireSchemaMatch);

	// Duplicate the list of cloned nodes
	if (OutClonedNodes != NULL)
	{
		OutClonedNodes->Append(ClonedGraph->Nodes);
	}

	// Determine if we are regenerating a blueprint on load
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(MergeTarget);
	const bool bIsLoading = Blueprint ? Blueprint->bIsRegeneratingOnLoad : false;

	// Move them all to the destination
	ClonedGraph->MoveNodesToAnotherGraph(MergeTarget, IsAsyncLoading() || bIsLoading, bInIsCompiling);
}

// Moves the contents of all of the children graphs (recursively) into the target graph.  This does not clone, it's destructive to the source
void FEdGraphUtilities::MergeChildrenGraphsIn(UEdGraph* MergeTarget, UEdGraph* ParentGraph, bool bRequireSchemaMatch, bool bInIsCompiling/* = false*/)
{
	// Determine if we are regenerating a blueprint on load
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(MergeTarget);
	const bool bIsLoading = Blueprint ? Blueprint->bIsRegeneratingOnLoad : false;

	// Merge all children nodes in too
	for (int32 Index = 0; Index < ParentGraph->SubGraphs.Num(); ++Index)
	{
		UEdGraph* ChildGraph = ParentGraph->SubGraphs[Index];

		// Only merge children in with the same schema as the parent
		const bool bSchemaMatches = ChildGraph->GetSchema()->GetClass()->IsChildOf(MergeTarget->GetSchema()->GetClass());
		if (!bRequireSchemaMatch || bSchemaMatches)
		{
			// Even if we don't require a match to recurse, we do to actually copy the nodes
			if (bSchemaMatches)
			{
				ChildGraph->MoveNodesToAnotherGraph(MergeTarget, IsAsyncLoading() || bIsLoading, bInIsCompiling);
			}

			MergeChildrenGraphsIn(MergeTarget, ChildGraph, bRequireSchemaMatch, bInIsCompiling);
		}
	}
}

// Tries to rename the graph to have a name similar to BaseName
void FEdGraphUtilities::RenameGraphCloseToName(UEdGraph* Graph, const FString& BaseName, int32 StartIndex)
{
	FString NewName = BaseName;

	int32 NameIndex = StartIndex;
	for (;;)
	{
		if (Graph->Rename(*NewName, Graph->GetOuter(), REN_Test))
		{
			UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForGraphChecked(Graph);
			Graph->Rename(*NewName, Graph->GetOuter(), (BP->bIsRegeneratingOnLoad ? REN_ForceNoResetLoaders : 0) | REN_DontCreateRedirectors);
			return;
		}

		NewName = FString::Printf(TEXT("%s_%d"), *BaseName, NameIndex);
		++NameIndex;
	}
}

void FEdGraphUtilities::RenameGraphToNameOrCloseToName(UEdGraph* Graph, const FString& DesiredName)
{
	if (Graph->Rename(*DesiredName, Graph->GetOuter(), REN_Test))
	{
		UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForGraphChecked(Graph);
		Graph->Rename(*DesiredName, Graph->GetOuter(), (BP->bIsRegeneratingOnLoad ? REN_ForceNoResetLoaders : 0) | REN_DontCreateRedirectors);
	}
	else
	{
		RenameGraphCloseToName(Graph, DesiredName);
	}
}

// Exports a set of nodes to text
void FEdGraphUtilities::ExportNodesToText(TSet<UObject*> NodesToExport, /*out*/ FString& ExportedText)
{
	// Clear the mark state for saving.
	UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));

	FStringOutputDevice Archive;
	const FExportObjectInnerContext Context;

	// Export each of the selected nodes
	UObject* LastOuter = NULL;
	for (TSet<UObject*>::TConstIterator NodeIt(NodesToExport); NodeIt; ++NodeIt)
	{
		UObject* Node = *NodeIt;

		// The nodes should all be from the same scope
		UObject* ThisOuter = Node->GetOuter();
		check((LastOuter == ThisOuter) || (LastOuter == NULL));
		LastOuter = ThisOuter;

		UExporter::ExportToOutputDevice(&Context, Node, NULL, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified|PPF_Copy|PPF_Delimited, false, ThisOuter);
	}

	ExportedText = Archive;
}

// Imports a set of previously exported nodes into a graph
void FEdGraphUtilities::ImportNodesFromText(UEdGraph* DestinationGraph, const FString& TextToImport, /*out*/ TSet<UEdGraphNode*>& ImportedNodeSet)
{
	// Turn the text buffer into objects
	FGraphObjectTextFactory Factory(DestinationGraph);
	Factory.ProcessBuffer(DestinationGraph, RF_Transactional, TextToImport);

	// Fix up pin cross-links, etc...
	FEdGraphUtilities::PostProcessPastedNodes(Factory.SpawnedNodes);

	ImportedNodeSet.Append(Factory.SpawnedNodes);
}

bool FEdGraphUtilities::CanImportNodesFromText(const UEdGraph* DestinationGraph, const FString& TextToImport )
{
	FGraphObjectTextFactory Factory(DestinationGraph);
	return Factory.CanCreateObjectsFromText(TextToImport);
}

FIntRect FEdGraphUtilities::CalculateApproximateNodeBoundaries(const TArray<UEdGraphNode*>& Nodes)
{
	int32 MinNodeX = +(int32)(1<<30);
	int32 MinNodeY = +(int32)(1<<30);
	int32 MaxNodeX = -(int32)(1<<30);
	int32 MaxNodeY = -(int32)(1<<30);

	for (auto NodeIt(Nodes.CreateConstIterator()); NodeIt; ++NodeIt)
	{
		UEdGraphNode* Node = *NodeIt;
		if (Node)
		{
			// Update stats
			MinNodeX = FMath::Min<int32>(MinNodeX, Node->NodePosX);
			MinNodeY = FMath::Min<int32>(MinNodeY, Node->NodePosY);
			MaxNodeX = FMath::Max<int32>(MaxNodeX, Node->NodePosX + Node->NodeWidth);
			MaxNodeY = FMath::Max<int32>(MaxNodeY, Node->NodePosY + Node->NodeHeight);
		}
	}

	const int32 AverageNodeWidth = 200;
	const int32 AverageNodeHeight = 128;

	return FIntRect(MinNodeX, MinNodeY, MaxNodeX + AverageNodeWidth, MaxNodeY + AverageNodeHeight);
}

void FEdGraphUtilities::CopyCommonState(UEdGraphNode* OldNode, UEdGraphNode* NewNode)
{
	// Copy common inheritable state (comment, location, etc...)
	NewNode->NodePosX = OldNode->NodePosX;
	NewNode->NodePosY = OldNode->NodePosY;
	NewNode->NodeWidth = OldNode->NodeWidth;
	NewNode->NodeHeight = OldNode->NodeHeight;
	NewNode->NodeComment = OldNode->NodeComment;
}

void FEdGraphUtilities::RegisterVisualNodeFactory(TSharedPtr<FGraphPanelNodeFactory> NewFactory)
{
	VisualNodeFactories.Add(NewFactory);
}

void FEdGraphUtilities::UnregisterVisualNodeFactory(TSharedPtr<FGraphPanelNodeFactory> OldFactory)
{
	VisualNodeFactories.Remove(OldFactory);
}

void FEdGraphUtilities::RegisterVisualPinFactory(TSharedPtr<FGraphPanelPinFactory> NewFactory)
{
	VisualPinFactories.Add(NewFactory);
}

void FEdGraphUtilities::UnregisterVisualPinFactory(TSharedPtr<FGraphPanelPinFactory> OldFactory)
{
	VisualPinFactories.Remove(OldFactory);
}

//////////////////////////////////////////////////////////////////////////
// FWeakGraphPinPtr

void FWeakGraphPinPtr::operator=(const class UEdGraphPin* Pin)
{
	PinObjectPtr = Pin;
	if(PinObjectPtr.IsValid())
	{
		PinName = Pin->PinName;
		NodeObjectPtr = Pin->GetOwningNode();
	}
	else
	{
		Reset();
	}
}

UEdGraphPin* FWeakGraphPinPtr::Get()
{
	UEdGraphNode* Node = NodeObjectPtr.Get();
	if(Node != NULL)
	{
		// If pin is no longer valid or has a different owner, attempt to fix up the reference
		UEdGraphPin* Pin = PinObjectPtr.Get();
		if(Pin == NULL || Pin->GetOuter() != Node)
		{
			for(auto PinIter = Node->Pins.CreateConstIterator(); PinIter; ++PinIter)
			{
				UEdGraphPin* TestPin = *PinIter;
				if(TestPin->PinName.Equals(PinName))
				{
					Pin = TestPin;
					PinObjectPtr = Pin;
					break;
				}
			}
		}

		return Pin;
	}

	return NULL;
}