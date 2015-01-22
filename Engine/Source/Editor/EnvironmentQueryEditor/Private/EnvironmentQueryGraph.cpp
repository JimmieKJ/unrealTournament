// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "EnvironmentQueryEditorPrivatePCH.h"
#include "EnvironmentQueryEditorModule.h"


//////////////////////////////////////////////////////////////////////////
// EnvironmentQueryGraph

namespace EQSGraphVersion
{
	const int32 Initial = 0;
	const int32 NestedNodes = 1;
	const int32 CopyPasteOutersBug = 2;

	const int32 Latest = CopyPasteOutersBug;
}

UEnvironmentQueryGraph::UEnvironmentQueryGraph(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Schema = UEdGraphSchema_EnvironmentQuery::StaticClass();
	bLockUpdates = false;
}

struct FCompareNodeXLocation
{
	FORCEINLINE bool operator()(const UEdGraphPin& A, const UEdGraphPin& B) const
	{
		return A.GetOwningNode()->NodePosX < B.GetOwningNode()->NodePosX;
	}
};

void UEnvironmentQueryGraph::UpdateAsset()
{
	if (bLockUpdates)
	{
		return;
	}

	//let's find root node
	UEnvironmentQueryGraphNode_Root* RootNode = NULL;
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		RootNode = Cast<UEnvironmentQueryGraphNode_Root>(Nodes[Index]);
		if (RootNode != NULL)
		{
			break;
		}
	}

	UEnvQuery* Query = Cast<UEnvQuery>(GetOuter());
	Query->Options.Reset();
	if (RootNode && RootNode->Pins.Num() > 0 && RootNode->Pins[0]->LinkedTo.Num() > 0)
	{
		UEdGraphPin* MyPin = RootNode->Pins[0];

		// sort connections so that they're organized the same as user can see in the editor
		MyPin->LinkedTo.Sort(FCompareNodeXLocation());

		for (int32 Index=0; Index < MyPin->LinkedTo.Num(); ++Index)
		{
			UEnvironmentQueryGraphNode_Option* OptionNode = Cast<UEnvironmentQueryGraphNode_Option>(MyPin->LinkedTo[Index]->GetOwningNode());
			if (OptionNode)
			{
				UEnvQueryOption* OptionInstance = Cast<UEnvQueryOption>(OptionNode->NodeInstance);
				if (OptionInstance != NULL)
				{
					OptionInstance->Tests.Reset();

					for (int32 iTest = 0; iTest < OptionNode->Tests.Num(); iTest++)
					{
						UEnvironmentQueryGraphNode_Test* TestNode = OptionNode->Tests[iTest];
						if (TestNode && TestNode->bTestEnabled)
						{
							UEnvQueryTest* TestInstance = Cast<UEnvQueryTest>(TestNode->NodeInstance);
							if (TestInstance)
							{
								OptionInstance->Tests.Add(TestInstance);
							}
						}
					}

					Query->Options.Add(OptionInstance);
				}
			}
		}
	}

	RemoveOrphanedNodes();
#if USE_EQS_DEBUGGER
	UEnvQueryManager::NotifyAssetUpdate(Query);
#endif
}

void UEnvironmentQueryGraph::CalculateAllWeights()
{
	for (int32 i = 0; i < Nodes.Num(); i++)
	{
		UEnvironmentQueryGraphNode_Option* OptionNode = Cast<UEnvironmentQueryGraphNode_Option>(Nodes[i]);
		if (OptionNode)
		{
			OptionNode->CalculateWeights();
		}
	}
}

void UEnvironmentQueryGraph::MarkVersion()
{
	GraphVersion = EQSGraphVersion::Latest;
}

void UEnvironmentQueryGraph::UpdateVersion()
{
	if (GraphVersion == EQSGraphVersion::Latest)
	{
		return;
	}

	// convert to nested nodes
	if (GraphVersion < EQSGraphVersion::NestedNodes)
	{
		UpdateVersion_NestedNodes();
	}

	if (GraphVersion < EQSGraphVersion::CopyPasteOutersBug)
	{
		UpdateVersion_FixupOuters();
	}

	GraphVersion = EQSGraphVersion::Latest;
	Modify();
}

void UEnvironmentQueryGraph::UpdateVersion_NestedNodes()
{
	for (int32 i = 0; i < Nodes.Num(); i++)
	{
		UEnvironmentQueryGraphNode_Option* OptionNode = Cast<UEnvironmentQueryGraphNode_Option>(Nodes[i]);
		if (OptionNode)
		{
			UEnvironmentQueryGraphNode* Node = OptionNode;
			while (Node)
			{
				UEnvironmentQueryGraphNode* NextNode = NULL;
				for (int32 iPin = 0; iPin < Node->Pins.Num(); iPin++)
				{
					UEdGraphPin* TestPin = Node->Pins[iPin];
					if (TestPin && TestPin->Direction == EGPD_Output)
					{
						for (int32 iLink = 0; iLink < TestPin->LinkedTo.Num(); iLink++)
						{
							UEdGraphPin* LinkedTo = TestPin->LinkedTo[iLink];
							UEnvironmentQueryGraphNode_Test* LinkedTest = LinkedTo ? Cast<UEnvironmentQueryGraphNode_Test>(LinkedTo->GetOwningNode()) : NULL;
							if (LinkedTest)
							{
								LinkedTest->ParentNode = OptionNode;
								OptionNode->Tests.Add(LinkedTest);

								NextNode = LinkedTest;
								break;
							}
						}

						break;
					}
				}

				Node = NextNode;
			}
		}
	}

	for (int32 i = Nodes.Num() - 1; i >= 0; i--)
	{
		UEnvironmentQueryGraphNode_Test* TestNode = Cast<UEnvironmentQueryGraphNode_Test>(Nodes[i]);
		if (TestNode)
		{
			TestNode->Pins.Empty();
			Nodes.RemoveAt(i);
			continue;
		}

		UEnvironmentQueryGraphNode_Option* OptionNode = Cast<UEnvironmentQueryGraphNode_Option>(Nodes[i]);
		if (OptionNode && OptionNode->Pins.IsValidIndex(1))
		{
			OptionNode->Pins.RemoveAt(1);
		}
	}
}

void UEnvironmentQueryGraph::UpdateVersion_FixupOuters()
{
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		UEnvironmentQueryGraphNode* MyNode = Cast<UEnvironmentQueryGraphNode>(Nodes[Index]);
		if (MyNode)
		{
			MyNode->PostEditImport();
		}
	}
}

void UEnvironmentQueryGraph::RemoveOrphanedNodes()
{
	UEnvQuery* QueryAsset = CastChecked<UEnvQuery>(GetOuter());

	// Obtain a list of all nodes that should be in the asset
	TSet<UObject*> AllNodes;
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		UEnvironmentQueryGraphNode_Option* OptionNode = Cast<UEnvironmentQueryGraphNode_Option>(Nodes[Index]);
		if (OptionNode)
		{
			UEnvQueryOption* OptionInstance = Cast<UEnvQueryOption>(OptionNode->NodeInstance);
			if (OptionInstance)
			{
				AllNodes.Add(OptionInstance);
				if (OptionInstance->Generator)
				{
					AllNodes.Add(OptionInstance->Generator);
				}
			}

			for (int32 SubIdx = 0; SubIdx < OptionNode->Tests.Num(); SubIdx++)
			{
				if (OptionNode->Tests[SubIdx] && OptionNode->Tests[SubIdx]->NodeInstance)
				{
					AllNodes.Add(OptionNode->Tests[SubIdx]->NodeInstance);
				}
			}
		}
	}

	// Obtain a list of all nodes actually in the asset and discard unused nodes
	TArray<UObject*> AllInners;
	const bool bIncludeNestedObjects = false;
	GetObjectsWithOuter(QueryAsset, AllInners, bIncludeNestedObjects);
	for (auto InnerIt = AllInners.CreateConstIterator(); InnerIt; ++InnerIt)
	{
		UObject* Node = *InnerIt;
		const bool bEQSNode =
			Node->IsA(UEnvQueryGenerator::StaticClass()) ||
			Node->IsA(UEnvQueryTest::StaticClass()) ||
			Node->IsA(UEnvQueryOption::StaticClass());

		if (bEQSNode && !AllNodes.Contains(Node))
		{
			Node->SetFlags(RF_Transient);
			Node->Rename(NULL, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);
		}
	}
}

void UEnvironmentQueryGraph::LockUpdates()
{
	bLockUpdates = true;
}

void UEnvironmentQueryGraph::UnlockUpdates()
{
	bLockUpdates = false;

	UpdateAsset();
}
