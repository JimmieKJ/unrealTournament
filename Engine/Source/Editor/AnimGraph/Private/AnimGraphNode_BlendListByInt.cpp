// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "AnimGraphPrivatePCH.h"

#include "GraphEditorActions.h"
#include "ScopedTransaction.h"
#include "AnimGraphNode_BlendListByInt.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_BlendListByInt

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_BlendListByInt::UAnimGraphNode_BlendListByInt(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_BlendListByInt::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_BlendListByInt_Tooltip", "Blend List (by int)");
}

FText UAnimGraphNode_BlendListByInt::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("AnimGraphNode_BlendListByInt_Title", "Blend Poses by int");
}

void UAnimGraphNode_BlendListByInt::PostPlacedNewNode()
{
	// Make sure we start out with a pin
	Node.AddPose();
}

void UAnimGraphNode_BlendListByInt::AddPinToBlendList()
{
	FScopedTransaction Transaction( NSLOCTEXT("A3Nodes", "AddBlendListPin", "AddBlendListPin") );
	Modify();

	Node.AddPose();
	ReconstructNode();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
}

void UAnimGraphNode_BlendListByInt::RemovePinFromBlendList(UEdGraphPin* Pin)
{
	FScopedTransaction Transaction( NSLOCTEXT("A3Nodes", "RemoveBlendListPin", "RemoveBlendListPin") );
	Modify();

	UProperty* AssociatedProperty;
	int32 ArrayIndex;
	GetPinAssociatedProperty(GetFNodeType(), Pin, /*out*/ AssociatedProperty, /*out*/ ArrayIndex);

	if (ArrayIndex != INDEX_NONE)
	{
		//@TODO: ANIMREFACTOR: Need to handle moving pins below up correctly
		// setting up removed pins info
		RemovedPinArrayIndex = ArrayIndex;
		Node.RemovePose(ArrayIndex);
		// removes the selected pin and related properties in reconstructNode()
		// @TODO: Considering passing "RemovedPinArrayIndex" to ReconstructNode as the argument
		ReconstructNode();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}
}

void UAnimGraphNode_BlendListByInt::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	if (!Context.bIsDebugging)
	{
		Context.MenuBuilder->BeginSection("AnimGraphBlendList", NSLOCTEXT("A3Nodes", "BlendListHeader", "BlendList"));
		{
			if (Context.Pin != NULL)
			{
				// we only do this for normal BlendList/BlendList by enum, BlendList by Bool doesn't support add/remove pins
				if ( Context.Pin->Direction == EGPD_Input )
				{
					Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().RemoveBlendListPin);
				}
			}
			else
			{
				Context.MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().AddBlendListPin);
			}
		}
		Context.MenuBuilder->EndSection();	
	}
}

#undef LOCTEXT_NAMESPACE