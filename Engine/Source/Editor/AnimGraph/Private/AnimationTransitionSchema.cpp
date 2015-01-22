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
#include "K2ActionMenuBuilder.h" // for FK2ActionMenuBuilder::AddNewNodeAction()
#include "K2Node_TransitionRuleGetter.h"

/////////////////////////////////////////////////////
// UAnimationTransitionSchema

#define LOCTEXT_NAMESPACE "AnimationTransitionSchema"

UAnimationTransitionSchema::UAnimationTransitionSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Initialize defaults
}

void UAnimationTransitionSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	Super::GetGraphContextActions(ContextMenuBuilder);

	GetSourceStateActions(ContextMenuBuilder);
}

void UAnimationTransitionSchema::GetSourceStateActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	if ((ContextMenuBuilder.FromPin == NULL) || ((ContextMenuBuilder.FromPin->Direction == EGPD_Input) && (ContextMenuBuilder.FromPin->PinType.PinCategory == PC_Float)))
	{
		// Find the source state associated with this transition
		UAnimBlueprint* Blueprint = CastChecked<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForGraph(ContextMenuBuilder.CurrentGraph));

		if (UAnimBlueprintGeneratedClass* AnimBlueprintClass = Blueprint->GetAnimBlueprintSkeletonClass())
		{
			if (UAnimStateTransitionNode* TransNode = GetTransitionNodeFromGraph(AnimBlueprintClass->GetAnimBlueprintDebugData(), ContextMenuBuilder.CurrentGraph))
			{
				if (UAnimStateNode* SourceStateNode = Cast<UAnimStateNode>(TransNode->GetPreviousState()))
				{
					// Offer options from the source state

					// Sequence player positions
					ETransitionGetter::Type SequenceSpecificGetters[] =
					{
						ETransitionGetter::AnimationAsset_GetCurrentTime,
						ETransitionGetter::AnimationAsset_GetLength,
						ETransitionGetter::AnimationAsset_GetCurrentTimeFraction,
						ETransitionGetter::AnimationAsset_GetTimeFromEnd,
						ETransitionGetter::AnimationAsset_GetTimeFromEndFraction
					};

					TArray<UK2Node*> AssetPlayers;
					SourceStateNode->BoundGraph->GetNodesOfClassEx<UAnimGraphNode_Base, UK2Node>(/*out*/ AssetPlayers);

					const FString Category_AssetPlayer(TEXT("Asset Player"));

					for (int32 TypeIndex = 0; TypeIndex < ARRAY_COUNT(SequenceSpecificGetters); ++TypeIndex)
					{
						for (auto NodeIt = AssetPlayers.CreateConstIterator(); NodeIt; ++NodeIt)
						{
							UAnimGraphNode_Base* AnimNode = CastChecked<UAnimGraphNode_Base>(*NodeIt);
							
							if (AnimNode->DoesSupportTimeForTransitionGetter())
							{
								UK2Node_TransitionRuleGetter* NodeTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_TransitionRuleGetter>();

								FString AssetName;

								UAnimationAsset * AnimAsset = AnimNode->GetAnimationAsset();
								if (AnimAsset)
								{
									NodeTemplate->AssociatedAnimAssetPlayerNode = AnimNode;
									AssetName = AnimAsset->GetName();
								}

								NodeTemplate->GetterType = SequenceSpecificGetters[TypeIndex];

								FFormatNamedArguments Args;
								Args.Add(TEXT("NodeName"), UK2Node_TransitionRuleGetter::GetFriendlyName(NodeTemplate->GetterType));
								Args.Add(TEXT("AssetName"), FText::FromString(AssetName));
								FText Title = FText::Format(LOCTEXT("TransitionFor", "{NodeName} for '{AssetName}'"), Args);

								TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, Category_AssetPlayer, Title, NodeTemplate->GetTooltipText().ToString(), 0, NodeTemplate->GetKeywords());
								Action->NodeTemplate = NodeTemplate;
							}
						}
					}

					// Non-sequence specific ones
					ETransitionGetter::Type NonSpecificGetters[] =
					{
						ETransitionGetter::CurrentTransitionDuration,
						ETransitionGetter::CurrentState_ElapsedTime,
						ETransitionGetter::CurrentState_GetBlendWeight
					};

					for (int32 TypeIndex = 0; TypeIndex < ARRAY_COUNT(NonSpecificGetters); ++TypeIndex)
					{
						FString Category_Transition(TEXT("Transition"));

						UK2Node_TransitionRuleGetter* NodeTemplate = ContextMenuBuilder.CreateTemplateNode<UK2Node_TransitionRuleGetter>();
						NodeTemplate->GetterType = NonSpecificGetters[TypeIndex];

						FText Title = UK2Node_TransitionRuleGetter::GetFriendlyName(NodeTemplate->GetterType);

						TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = FK2ActionMenuBuilder::AddNewNodeAction(ContextMenuBuilder, Category_Transition, Title, NodeTemplate->GetTooltipText().ToString(), 0, NodeTemplate->GetKeywords());
						Action->NodeTemplate = NodeTemplate;
					}
				}
			}
		}
	}
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