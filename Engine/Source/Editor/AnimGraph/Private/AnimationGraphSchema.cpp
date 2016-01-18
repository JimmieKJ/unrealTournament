// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimationGraphSchema.cpp
=============================================================================*/

#include "AnimGraphPrivatePCH.h"
#include "BlueprintUtilities.h"
#include "GraphEditorActions.h"
#include "ScopedTransaction.h"
#include "AssetData.h"
#include "AnimationGraphSchema.h"
#include "K2Node_TransitionRuleGetter.h"
#include "AnimStateNode.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_RotationOffsetBlendSpace.h"
#include "AnimGraphNode_SequencePlayer.h"

#define LOCTEXT_NAMESPACE "AnimationGraphSchema"

/////////////////////////////////////////////////////
// UAnimationGraphSchema

UAnimationGraphSchema::UAnimationGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PN_SequenceName = TEXT("Sequence");

	NAME_NeverAsPin = TEXT("NeverAsPin");
	NAME_PinHiddenByDefault = TEXT("PinHiddenByDefault");
	NAME_PinShownByDefault = TEXT("PinShownByDefault");
	NAME_AlwaysAsPin = TEXT("AlwaysAsPin");
	NAME_OnEvaluate = TEXT("OnEvaluate");
	NAME_CustomizeProperty = TEXT("CustomizeProperty");
	DefaultEvaluationHandlerName = TEXT("EvaluateGraphExposedInputs");
}

FLinearColor UAnimationGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	const bool bAdditive = PinType.PinSubCategory == TEXT("Additive");
	if (UAnimationGraphSchema::IsLocalSpacePosePin(PinType))
	{
		if (bAdditive) 
		{
			return FLinearColor(0.12, 0.60, 0.10);
		}
		else
		{
			return FLinearColor::White;
		}
	}
	else if (UAnimationGraphSchema::IsComponentSpacePosePin(PinType))
	{
		//@TODO: Pick better colors
		if (bAdditive) 
		{
			return FLinearColor(0.12, 0.60, 0.60);
		}
		else
		{
			return FLinearColor(0.20f, 0.50f, 1.00f);
		}
	}

	return Super::GetPinTypeColor(PinType);
}

EGraphType UAnimationGraphSchema::GetGraphType(const UEdGraph* TestEdGraph) const
{
	return GT_Animation;
}

void UAnimationGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	// Create the result node
	FGraphNodeCreator<UAnimGraphNode_Root> NodeCreator(Graph);
	UAnimGraphNode_Root* ResultSinkNode = NodeCreator.CreateNode();
	NodeCreator.Finalize();
	SetNodeMetaData(ResultSinkNode, FNodeMetadata::DefaultGraphNode);
}

void UAnimationGraphSchema::HandleGraphBeingDeleted(UEdGraph& GraphBeingRemoved) const
{
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(&GraphBeingRemoved))
	{
		// Look for state nodes that reference this graph
		TArray<UAnimStateNode*> StateNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass<UAnimStateNode>(Blueprint, /*out*/ StateNodes);

		TSet<UAnimStateNode*> NodesToDelete;
		for (int32 i = 0; i < StateNodes.Num(); ++i)
		{
			UAnimStateNode* StateNode = StateNodes[i];
			if (StateNode->BoundGraph == &GraphBeingRemoved)
			{
				NodesToDelete.Add(StateNode);
			}
		}

		// Delete the node that owns us
		ensure(NodesToDelete.Num() <= 1);
		for (TSet<UAnimStateNode*>::TIterator It(NodesToDelete); It; ++It)
		{
			UAnimStateNode* NodeToDelete = *It;

			// Prevent re-entrancy here
			NodeToDelete->BoundGraph = NULL;

			NodeToDelete->Modify();
			NodeToDelete->DestroyNode();
		}
	}
}

bool UAnimationGraphSchema::IsPosePin(const FEdGraphPinType& PinType)
{
	return IsLocalSpacePosePin(PinType) || IsComponentSpacePosePin(PinType);
}

bool UAnimationGraphSchema::IsLocalSpacePosePin(const FEdGraphPinType& PinType)
{
	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();

	UScriptStruct* PoseLinkStruct = FPoseLink::StaticStruct();
	return (PinType.PinCategory == Schema->PC_Struct) && (PinType.PinSubCategoryObject == PoseLinkStruct);
}

bool UAnimationGraphSchema::IsComponentSpacePosePin(const FEdGraphPinType& PinType)
{
	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();

	UScriptStruct* ComponentSpacePoseLinkStruct = FComponentSpacePoseLink::StaticStruct();
	return (PinType.PinCategory == Schema->PC_Struct) && (PinType.PinSubCategoryObject == ComponentSpacePoseLinkStruct);
}

const FPinConnectionResponse UAnimationGraphSchema::DetermineConnectionResponseOfCompatibleTypedPins(const UEdGraphPin* PinA, const UEdGraphPin* PinB, const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const
{
	// Enforce a tree hierarchy; where poses can only have one output (parent) connection
	if (IsPosePin(OutputPin->PinType) && IsPosePin(InputPin->PinType))
	{
		if ((OutputPin->LinkedTo.Num() > 0) || (InputPin->LinkedTo.Num() > 0))
		{
			const ECanCreateConnectionResponse ReplyBreakOutputs = CONNECT_RESPONSE_BREAK_OTHERS_AB;
			return FPinConnectionResponse(ReplyBreakOutputs, TEXT("Replace existing connections"));
		}
	}

	// Fall back to standard K2 rules
	return Super::DetermineConnectionResponseOfCompatibleTypedPins(PinA, PinB, InputPin, OutputPin);
}

bool UAnimationGraphSchema::ArePinsCompatible(const UEdGraphPin* PinA, const UEdGraphPin* PinB, const UClass* CallingContext, bool bIgnoreArray) const
{
	// both are pose pin, but doesn't match type, then return false;
	if (IsPosePin(PinA->PinType) && IsPosePin(PinB->PinType) && IsLocalSpacePosePin(PinA->PinType) != IsLocalSpacePosePin(PinB->PinType))
	{
		return false;
	}

	return Super::ArePinsCompatible(PinA, PinB, CallingContext, bIgnoreArray);
}

bool UAnimationGraphSchema::DoesSupportAnimNotifyActions() const
{
	// Don't offer notify items in anim graph
	return false;
}

bool UAnimationGraphSchema::SearchForAutocastFunction(const UEdGraphPin* OutputPin, const UEdGraphPin* InputPin, FName& TargetFunction, /*out*/ UClass*& FunctionOwner) const
{
	if (IsComponentSpacePosePin(OutputPin->PinType) && IsLocalSpacePosePin(InputPin->PinType))
	{
		// Insert a Component To LocalSpace conversion
		return true;
	}
	else if (IsLocalSpacePosePin(OutputPin->PinType) && IsComponentSpacePosePin(InputPin->PinType))
	{
		// Insert a Local To ComponentSpace conversion
		return true;
	}
	else
	{
		return Super::SearchForAutocastFunction(OutputPin, InputPin, TargetFunction, FunctionOwner);
	}
}

bool UAnimationGraphSchema::CreateAutomaticConversionNodeAndConnections(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	// Determine which pin is an input and which pin is an output
	UEdGraphPin* InputPin = NULL;
	UEdGraphPin* OutputPin = NULL;
	if (!CategorizePinsByDirection(PinA, PinB, /*out*/ InputPin, /*out*/ OutputPin))
	{
		return false;
	}

	// Look for animation specific conversion operations
	UK2Node* TemplateNode = NULL;
	if (IsComponentSpacePosePin(OutputPin->PinType) && IsLocalSpacePosePin(InputPin->PinType))
	{
		TemplateNode = NewObject<UAnimGraphNode_ComponentToLocalSpace>();
	}
	else if (IsLocalSpacePosePin(OutputPin->PinType) && IsComponentSpacePosePin(InputPin->PinType))
	{
		TemplateNode = NewObject<UAnimGraphNode_LocalToComponentSpace>();
	}

	// Spawn the conversion node if it's specific to animation
	if (TemplateNode != NULL)
	{
		UEdGraph* Graph = InputPin->GetOwningNode()->GetGraph();
		FVector2D AverageLocation = CalculateAveragePositionBetweenNodes(InputPin, OutputPin);

		UK2Node* ConversionNode = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node>(Graph, TemplateNode, AverageLocation);
		AutowireConversionNode(InputPin, OutputPin, ConversionNode);

		return true;
	}
	else
	{
		// Give the regular conversions a shot
		return Super::CreateAutomaticConversionNodeAndConnections(PinA, PinB);
	}
}

bool IsAimOffsetBlendSpace(UBlendSpaceBase* BlendSpace)
{
	return	BlendSpace->IsA(UAimOffsetBlendSpace::StaticClass()) ||
			BlendSpace->IsA(UAimOffsetBlendSpace1D::StaticClass());
}

void UAnimationGraphSchema::SpawnNodeFromAsset(UAnimationAsset* Asset, const FVector2D& GraphPosition, UEdGraph* Graph, UEdGraphPin* PinIfAvailable)
{
	check(Graph);
	check(Graph->GetSchema()->IsA(UAnimationGraphSchema::StaticClass()));
	check(Asset);

	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForGraph(Graph));

	const bool bSkelMatch = (AnimBlueprint != NULL) && (AnimBlueprint->TargetSkeleton == Asset->GetSkeleton());
	const bool bTypeMatch = (PinIfAvailable == NULL) || UAnimationGraphSchema::IsLocalSpacePosePin(PinIfAvailable->PinType);
	const bool bDirectionMatch = (PinIfAvailable == NULL) || (PinIfAvailable->Direction == EGPD_Input);

	if (bSkelMatch && bTypeMatch && bDirectionMatch)
	{
		FEdGraphSchemaAction_K2NewNode Action;

		if (UAnimSequence* Sequence = Cast<UAnimSequence>(Asset))
		{
			UAnimGraphNode_SequencePlayer* PlayerNode = NewObject<UAnimGraphNode_SequencePlayer>();
			PlayerNode->Node.Sequence = Sequence;
			Action.NodeTemplate = PlayerNode;
		}
		else if (UBlendSpaceBase* BlendSpace = Cast<UBlendSpaceBase>(Asset))
		{
			if (IsAimOffsetBlendSpace(BlendSpace))
			{
				UAnimGraphNode_RotationOffsetBlendSpace* PlayerNode = NewObject<UAnimGraphNode_RotationOffsetBlendSpace>();
				PlayerNode->Node.BlendSpace = BlendSpace;

				Action.NodeTemplate = PlayerNode;
			}
			else
			{
				UAnimGraphNode_BlendSpacePlayer* PlayerNode = NewObject<UAnimGraphNode_BlendSpacePlayer>();
				PlayerNode->Node.BlendSpace = BlendSpace;

				Action.NodeTemplate = PlayerNode;
			}
		}
		else if (UAnimComposite* Composite = Cast<UAnimComposite>(Asset))
		{
			UAnimGraphNode_SequencePlayer* PlayerNode = NewObject<UAnimGraphNode_SequencePlayer>();
			PlayerNode->Node.Sequence = Composite;
			Action.NodeTemplate = PlayerNode;
		}
		else
		{
			//unknown type
			return;
		}

		Action.PerformAction(Graph, PinIfAvailable, GraphPosition);
	}
}


void UAnimationGraphSchema::UpdateNodeWithAsset(UK2Node* K2Node, UAnimationAsset* Asset)
{
	if (Asset != NULL)
	{
		if (UAnimGraphNode_SequencePlayer* SequencePlayerNode = Cast<UAnimGraphNode_SequencePlayer>(K2Node))
		{
			if (UAnimSequence* Sequence = Cast<UAnimSequence>(Asset))
			{
				// Skeleton matches, and it's a sequence player; replace the existing sequence with the dragged one
				SequencePlayerNode->Node.Sequence = Sequence;
			}
		}
		else if (UBlendSpace* BlendSpace = Cast<UBlendSpace>(Asset))
		{
			if (IsAimOffsetBlendSpace(BlendSpace))
			{
				if (UAnimGraphNode_RotationOffsetBlendSpace* RotationOffsetNode = Cast<UAnimGraphNode_RotationOffsetBlendSpace>(K2Node))
				{
					// Skeleton matches, and it's a blendspace player; replace the existing blendspace with the dragged one
					RotationOffsetNode->Node.BlendSpace = BlendSpace;
				}
			}
			else
			{
				if (UAnimGraphNode_BlendSpacePlayer* BlendSpacePlayerNode = Cast<UAnimGraphNode_BlendSpacePlayer>(K2Node))
				{
					// Skeleton matches, and it's a blendspace player; replace the existing blendspace with the dragged one
					BlendSpacePlayerNode->Node.BlendSpace = BlendSpace;
				}
			}
		}
	}
}


void UAnimationGraphSchema::DroppedAssetsOnGraph( const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph ) const 
{
	UAnimationAsset* Asset = FAssetData::GetFirstAsset<UAnimationAsset>(Assets);
	if ((Asset != NULL) && (Graph != NULL))
	{
		SpawnNodeFromAsset(Asset, GraphPosition, Graph, NULL);
	}
}



void UAnimationGraphSchema::DroppedAssetsOnNode(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphNode* Node) const
{
	UAnimationAsset* Asset = FAssetData::GetFirstAsset<UAnimationAsset>(Assets);
	UK2Node* K2Node = Cast<UK2Node>(Node);
	if ((Asset != NULL) && (K2Node!= NULL))
	{
		UpdateNodeWithAsset(K2Node, Asset);
	}
}

void UAnimationGraphSchema::DroppedAssetsOnPin(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphPin* Pin) const
{
	UAnimationAsset* Asset = FAssetData::GetFirstAsset<UAnimationAsset>(Assets);
	if ((Asset != NULL) && (Pin != NULL))
	{
		SpawnNodeFromAsset(Asset, GraphPosition, Pin->GetOwningNode()->GetGraph(), Pin);
	}
}

void UAnimationGraphSchema::GetAssetsNodeHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphNode* HoverNode, FString& OutTooltipText, bool& OutOkIcon) const 
{ 
	UAnimationAsset* Asset = FAssetData::GetFirstAsset<UAnimationAsset>(Assets);
	if ((Asset == NULL) || (HoverNode == NULL))
	{
		OutTooltipText = TEXT("");
		OutOkIcon = false;
		return;
	}

	const UK2Node* PlayerNodeUnderCursor = NULL;
	if (Asset->IsA(UAnimSequence::StaticClass()))
	{
		PlayerNodeUnderCursor = Cast<const UAnimGraphNode_SequencePlayer>(HoverNode);
	}
	else if (Asset->IsA(UBlendSpace::StaticClass()))
	{
		UBlendSpace* BlendSpace = CastChecked<UBlendSpace>(Asset);
		if (IsAimOffsetBlendSpace(BlendSpace))
		{
			PlayerNodeUnderCursor = Cast<const UAnimGraphNode_RotationOffsetBlendSpace>(HoverNode);
		}
		else
		{
			PlayerNodeUnderCursor = Cast<const UAnimGraphNode_BlendSpacePlayer>(HoverNode);
		}
	}

	// this one only should happen when there is an Anim Blueprint
	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForNode(HoverNode));
	const bool bSkelMatch = (AnimBlueprint != NULL) && (AnimBlueprint->TargetSkeleton == Asset->GetSkeleton());

	if (!bSkelMatch)
	{
		OutOkIcon = false;
		OutTooltipText = FString::Printf(TEXT("Skeletons are not compatible"));
	}
	else if (PlayerNodeUnderCursor != NULL)
	{
		OutOkIcon = true;
		OutTooltipText = FString::Printf(TEXT("Change node to play %s"), *(Asset->GetName()));
	}
	else
	{
		OutOkIcon = false;
		OutTooltipText = FString::Printf(TEXT("Cannot replace '%s' with a sequence player"), *(HoverNode->GetName()));
	}
}

void UAnimationGraphSchema::GetAssetsPinHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphPin* HoverPin, FString& OutTooltipText, bool& OutOkIcon) const 
{ 
	UAnimationAsset* Asset = FAssetData::GetFirstAsset<UAnimationAsset>(Assets);
	if ((Asset == NULL) || (HoverPin == NULL))
	{
		OutTooltipText = TEXT("");
		OutOkIcon = false;
		return;
	}

	// this one only should happen when there is an Anim Blueprint
	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForNode(HoverPin->GetOwningNode()));

	const bool bSkelMatch = (AnimBlueprint != NULL) && (AnimBlueprint->TargetSkeleton == Asset->GetSkeleton());
	const bool bTypeMatch = UAnimationGraphSchema::IsLocalSpacePosePin(HoverPin->PinType);
	const bool bDirectionMatch = HoverPin->Direction == EGPD_Input;

	if (bSkelMatch && bTypeMatch && bDirectionMatch)
	{
		OutOkIcon = true;
		OutTooltipText = FString::Printf(TEXT("Play %s and feed to %s"), *(Asset->GetName()), *HoverPin->PinName);
	}
	else
	{
		OutOkIcon = false;
		OutTooltipText = FString::Printf(TEXT("Type or direction mismatch; must be wired to a pose input"));
	}
}

void UAnimationGraphSchema::GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets, const UEdGraph* HoverGraph, FString& OutTooltipText, bool& OutOkIcon) const
{
	UAnimationAsset* Asset = FAssetData::GetFirstAsset<UAnimationAsset>(Assets);
	if (Asset)
	{
		UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForGraph(HoverGraph));
		const bool bSkelMatch = (AnimBlueprint != NULL) && (AnimBlueprint->TargetSkeleton == Asset->GetSkeleton());
		if (!bSkelMatch)
		{
			OutOkIcon = false;
			OutTooltipText = FString::Printf(TEXT("Skeletons are not compatible"));
		}
	}
}

void UAnimationGraphSchema::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, FMenuBuilder* MenuBuilder, bool bIsDebugging) const
{
	Super::GetContextMenuActions(CurrentGraph, InGraphNode, InGraphPin, MenuBuilder, bIsDebugging);
}

FText UAnimationGraphSchema::GetPinDisplayName(const UEdGraphPin* Pin) const 
{
	check(Pin != NULL);

	FText DisplayName = Super::GetPinDisplayName(Pin);

	if (UAnimGraphNode_Base* Node = Cast<UAnimGraphNode_Base>(Pin->GetOwningNode()))
	{
		FString ProcessedDisplayName = DisplayName.ToString();
		Node->PostProcessPinName(Pin, ProcessedDisplayName);
		DisplayName = FText::FromString(ProcessedDisplayName);
	}

	return DisplayName;
}

#undef LOCTEXT_NAMESPACE
