// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimStateNodeBase.cpp
=============================================================================*/

#include "AnimGraphPrivatePCH.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "EdGraphUtilities.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "AnimStateNodeBase.h"
/////////////////////////////////////////////////////
// FAnimStateNodeNameValidator

class FAnimStateNodeNameValidator : public FStringSetNameValidator
{
public:
	FAnimStateNodeNameValidator(const UAnimStateNodeBase* InStateNode)
		: FStringSetNameValidator(FString())
	{
		TArray<UAnimStateNodeBase*> Nodes;
		UAnimationStateMachineGraph* StateMachine = CastChecked<UAnimationStateMachineGraph>(InStateNode->GetOuter());

		StateMachine->GetNodesOfClass<UAnimStateNodeBase>(Nodes);
		for (auto NodeIt = Nodes.CreateIterator(); NodeIt; ++NodeIt)
		{
			auto Node = *NodeIt;
			if (Node != InStateNode)
			{
				Names.Add(Node->GetStateName());
			}
		}
	}
};

/////////////////////////////////////////////////////
// UAnimStateNodeBase

UAnimStateNodeBase::UAnimStateNodeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimStateNodeBase::PostPasteNode()
{
	Super::PostPasteNode();

	if (UEdGraph* BoundGraph = GetBoundGraph())
	{
		// Add the new graph as a child of our parent graph
		UEdGraph* ParentGraph = GetGraph();
		ParentGraph->SubGraphs.Add(BoundGraph);

		//@TODO: CONDUIT: Merge conflict - May no longer be necessary due to other changes?
//		FBlueprintEditorUtils::RenameGraphWithSuggestion(BoundGraph, NameValidator, GetDesiredNewNodeName());
		//@ENDTODO

		// Restore transactional flag that is lost during copy/paste process
		BoundGraph->SetFlags(RF_Transactional);

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(ParentGraph);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
}

UObject* UAnimStateNodeBase::GetJumpTargetForDoubleClick() const
{
	return GetBoundGraph();
}

bool UAnimStateNodeBase::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const
{
	return Schema->IsA(UAnimationStateMachineSchema::StaticClass());
}

void UAnimStateNodeBase::OnRenameNode(const FString& NewName)
{
	FBlueprintEditorUtils::RenameGraph(GetBoundGraph(), NewName);
}

TSharedPtr<class INameValidatorInterface> UAnimStateNodeBase::MakeNameValidator() const
{
	return MakeShareable(new FAnimStateNodeNameValidator(this));
}

FString UAnimStateNodeBase::GetDocumentationLink() const
{
	return TEXT("Shared/GraphNodes/AnimationStateMachine");
}

