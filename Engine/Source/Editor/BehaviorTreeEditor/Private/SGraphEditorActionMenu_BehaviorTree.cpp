// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeEditorPrivatePCH.h"
#include "SGraphEditorActionMenu_BehaviorTree.h"
#include "BlueprintEditorUtils.h"
#include "K2ActionMenuBuilder.h" // for FBlueprintGraphActionListBuilder

SGraphEditorActionMenu_BehaviorTree::~SGraphEditorActionMenu_BehaviorTree()
{
	OnClosedCallback.ExecuteIfBound();
}

void SGraphEditorActionMenu_BehaviorTree::Construct( const FArguments& InArgs )
{
	this->GraphObj = InArgs._GraphObj;
	this->GraphNode = InArgs._GraphNode;
	this->SubNodeType = InArgs._SubNodeType;
	this->DraggedFromPins = InArgs._DraggedFromPins;
	this->NewNodePosition = InArgs._NewNodePosition;
	this->OnClosedCallback = InArgs._OnClosedCallback;
	this->AutoExpandActionMenu = InArgs._AutoExpandActionMenu;

	// Build the widget layout
	SBorder::Construct( SBorder::FArguments()
		.BorderImage( FEditorStyle::GetBrush("Menu.Background") )
		.Padding(5)
		[
			// Achieving fixed width by nesting items within a fixed width box.
			SNew(SBox)
			.WidthOverride(400)
			[
				SAssignNew(GraphActionMenu, SGraphActionMenu)
				.OnActionSelected(this, &SGraphEditorActionMenu_BehaviorTree::OnActionSelected)
				.OnCollectAllActions(this, &SGraphEditorActionMenu_BehaviorTree::CollectAllActions)
				.AutoExpandActionMenu(AutoExpandActionMenu)
			]
		]
	);
}

void SGraphEditorActionMenu_BehaviorTree::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	// Build up the context object
	FBlueprintGraphActionListBuilder ContextMenuBuilder(GraphObj);
	if (GraphNode != NULL)
	{
		ContextMenuBuilder.SelectedObjects.Add(GraphNode);
	}
	if (DraggedFromPins.Num() > 0)
	{
		ContextMenuBuilder.FromPin = DraggedFromPins[0];
	}

	// Determine all possible actions
	Cast<const UEdGraphSchema_BehaviorTree>(GraphObj->GetSchema())->GetGraphNodeContextActions(ContextMenuBuilder,SubNodeType);

	// Copy the added options back to the main list
	//@TODO: Avoid this copy
	OutAllActions.Append(ContextMenuBuilder);
}

TSharedRef<SEditableTextBox> SGraphEditorActionMenu_BehaviorTree::GetFilterTextBox()
{
	return GraphActionMenu->GetFilterTextBox();
}


void SGraphEditorActionMenu_BehaviorTree::OnActionSelected( const TArray< TSharedPtr<FEdGraphSchemaAction> >& SelectedAction )
{
	bool bDoDismissMenus = false;

	if ( GraphObj != NULL )
	{
		for ( int32 ActionIndex = 0; ActionIndex < SelectedAction.Num(); ActionIndex++ )
		{
			TSharedPtr<FEdGraphSchemaAction> CurrentAction = SelectedAction[ActionIndex];

			if ( CurrentAction.IsValid() )
			{
				CurrentAction->PerformAction(GraphObj, DraggedFromPins, NewNodePosition);
				bDoDismissMenus = true;
			}
		}
	}

	if ( bDoDismissMenus )
	{
		FSlateApplication::Get().DismissAllMenus();
	}
}