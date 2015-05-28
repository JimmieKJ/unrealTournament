// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimationTransitionSchema.cpp
=============================================================================*/

#include "AnimGraphPrivatePCH.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimationTransitionGraph.h"
#include "AnimationTransitionSchema.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_TransitionResult.h"

#include "BlueprintUtilities.h"
#include "AnimGraphDefinitions.h"
#include "GraphEditorActions.h"
#include "ScopedTransaction.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "K2Node_TransitionRuleGetter.h"

/////////////////////////////////////////////////////
// UAnimationTransitionSchema

#define LOCTEXT_NAMESPACE "AnimationTransitionSchema"

UAnimationTransitionSchema::UAnimationTransitionSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Initialize defaults
}

void UAnimationTransitionSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	// Create the entry/exit tunnels
	FGraphNodeCreator<UAnimGraphNode_TransitionResult> NodeCreator(Graph);
	UAnimGraphNode_TransitionResult* ResultSinkNode = NodeCreator.CreateNode();
	NodeCreator.Finalize();
	SetNodeMetaData(ResultSinkNode, FNodeMetadata::DefaultGraphNode);

	UAnimationTransitionGraph* TypedGraph = CastChecked<UAnimationTransitionGraph>(&Graph);
	TypedGraph->MyResultNode = ResultSinkNode;
}

void UAnimationTransitionSchema::GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const
{
	DisplayInfo.PlainName = FText::FromString( Graph.GetName() );

	const UAnimStateTransitionNode* TransNode = Cast<const UAnimStateTransitionNode>(Graph.GetOuter());
	if (TransNode == NULL)
	{
		//@TODO: Transition graphs should be created with the transition node as their outer as well!
		UAnimBlueprint* Blueprint = CastChecked<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForGraph(&Graph));
		if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Blueprint->GetAnimBlueprintSkeletonClass())
		{
			TransNode = GetTransitionNodeFromGraph(AnimBlueprintClass->GetAnimBlueprintDebugData(), &Graph);
		}
	}

	if (TransNode)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("NodeTitle"), TransNode->GetNodeTitle(ENodeTitleType::FullTitle));
		DisplayInfo.PlainName = FText::Format( NSLOCTEXT("Animation", "TransitionRuleGraphTitle", "{NodeTitle} (rule)"), Args );
	}

	DisplayInfo.DisplayName = DisplayInfo.PlainName;
}

UAnimStateTransitionNode* UAnimationTransitionSchema::GetTransitionNodeFromGraph(const FAnimBlueprintDebugData& DebugData, const UEdGraph* Graph)
{
	if (const TWeakObjectPtr<UAnimStateTransitionNode>* TransNodePtr = DebugData.TransitionGraphToNodeMap.Find(Graph))
	{
		return TransNodePtr->Get();
	}

	if (const TWeakObjectPtr<UAnimStateTransitionNode>* TransNodePtr = DebugData.TransitionBlendGraphToNodeMap.Find(Graph))
	{
		return TransNodePtr->Get();
	}

	return NULL;
}

UAnimStateNode* UAnimationTransitionSchema::GetStateNodeFromGraph(const FAnimBlueprintDebugData& DebugData, const UEdGraph* Graph)
{
	if (const TWeakObjectPtr<UAnimStateNode>* StateNodePtr = DebugData.StateGraphToNodeMap.Find(Graph))
	{
		return StateNodePtr->Get();
	}

	return NULL;
}

#undef LOCTEXT_NAMESPACE