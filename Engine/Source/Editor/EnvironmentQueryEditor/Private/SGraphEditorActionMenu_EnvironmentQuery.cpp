// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQueryEditorPrivatePCH.h"
#include "SGraphEditorActionMenu_EnvironmentQuery.h"
#include "K2ActionMenuBuilder.h" // for FBlueprintGraphActionListBuilder

SGraphEditorActionMenu_EnvironmentQuery::~SGraphEditorActionMenu_EnvironmentQuery()
{
	OnClosedCallback.ExecuteIfBound();
}

void SGraphEditorActionMenu_EnvironmentQuery::Construct( const FArguments& InArgs )
{
	this->GraphObj = InArgs._GraphObj;
	this->GraphNode = InArgs._GraphNode;
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
				.OnActionSelected(this, &SGraphEditorActionMenu_EnvironmentQuery::OnActionSelected)
				.OnCollectAllActions(this, &SGraphEditorActionMenu_EnvironmentQuery::CollectAllActions)
				.AutoExpandActionMenu(AutoExpandActionMenu)
			]
		]
	);
}

void SGraphEditorActionMenu_EnvironmentQuery::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
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
	Cast<const UEdGraphSchema_EnvironmentQuery>(GraphObj->GetSchema())->GetGraphNodeContextActions(ContextMenuBuilder);

	// Copy the added options back to the main list
	//@TODO: Avoid this copy
	OutAllActions.Append(ContextMenuBuilder);
}

TSharedRef<SEditableTextBox> SGraphEditorActionMenu_EnvironmentQuery::GetFilterTextBox()
{
	return GraphActionMenu->GetFilterTextBox();
}


void SGraphEditorActionMenu_EnvironmentQuery::OnActionSelected( const TArray< TSharedPtr<FEdGraphSchemaAction> >& SelectedAction )
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
