// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __SGraphEditorActionMenu_BehaviorTree_h__
#define __SGraphEditorActionMenu_BehaviorTree_h__

#include "SGraphActionMenu.h"

/////////////////////////////////////////////////////////////////////////////////////////////////

class SGraphEditorActionMenu_BehaviorTree : public SBorder
{
public:
	SLATE_BEGIN_ARGS( SGraphEditorActionMenu_BehaviorTree )
		: _GraphObj( static_cast<UEdGraph*>(NULL) )
		, _GraphNode(NULL)
		, _NewNodePosition( FVector2D::ZeroVector )
		, _AutoExpandActionMenu( false )
		, _SubNodeType(ESubNode::Decorator)
		{}

		SLATE_ARGUMENT( UEdGraph*, GraphObj )
		SLATE_ARGUMENT( UBehaviorTreeGraphNode*, GraphNode)
		SLATE_ARGUMENT( FVector2D, NewNodePosition )
		SLATE_ARGUMENT( TArray<UEdGraphPin*>, DraggedFromPins )
		SLATE_ARGUMENT( SGraphEditor::FActionMenuClosed, OnClosedCallback )
		SLATE_ARGUMENT( bool, AutoExpandActionMenu )
		SLATE_ARGUMENT( ESubNode::Type, SubNodeType)
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	~SGraphEditorActionMenu_BehaviorTree();

	TSharedRef<SEditableTextBox> GetFilterTextBox();

protected:
	UEdGraph* GraphObj;
	UBehaviorTreeGraphNode* GraphNode;
	TArray<UEdGraphPin*> DraggedFromPins;
	FVector2D NewNodePosition;
	bool AutoExpandActionMenu;
	ESubNode::Type SubNodeType;

	SGraphEditor::FActionMenuClosed OnClosedCallback;

	TSharedPtr<SGraphActionMenu> GraphActionMenu;

	void OnActionSelected( const TArray< TSharedPtr<FEdGraphSchemaAction> >& SelectedAction );

	/** Callback used to populate all actions list in SGraphActionMenu */
	void CollectAllActions(FGraphActionListBuilderBase& OutAllActions);
};

#endif // __SGraphEditorActionMenu_h__
