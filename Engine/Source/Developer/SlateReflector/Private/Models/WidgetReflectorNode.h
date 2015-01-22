// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
  * The WidgetReflector captures layout information and widget hierarchy structure into a tree of FReflectorNode.
  * The node also contains useful visualization and debug info.
  */
struct FReflectorNode
{
	/** The widget which this node describes */
	TWeakPtr<SWidget> Widget;

	/** The geometry of the widget */
	FGeometry Geometry;

	/** ReflectorNodes for the Widget's children. */
	TArray<TSharedPtr<FReflectorNode>> ChildNodes;	

	/** A tint that is applied to text in order to provide visual hints */
	FLinearColor Tint;

	/** Should we visualize this node */
	bool bVisualizeThisNode;

public:

	/**
	 * FReflectorNodes must be constructed as shared pointers.
	 *
	 * @param InWidgetGeometry Optional widget and associated geometry which this node should represent
	 */
	static TSharedRef<FReflectorNode> New( const FArrangedWidget& InWidgetGeometry = FArrangedWidget(SNullWidget::NullWidget, FGeometry()) )
	{
		return MakeShareable( new FReflectorNode(InWidgetGeometry) );
	}

	/**
	 * Capture all the children of the supplied widget along with their layout results
	 * Note that we include both visible and invisible children!
	 * 
	 * @param InWidgetGeometry Widget and geometry whose children to capture in the snapshot.
	 */
	static TSharedRef<FReflectorNode> NewTreeFrom( const FArrangedWidget& InWidgetGeometry )
	{
		TSharedRef<FReflectorNode> NewNode = MakeShareable( new FReflectorNode(InWidgetGeometry) );

		FArrangedChildren ArrangedChildren(EVisibility::All);
		NewNode->Widget.Pin()->ArrangeChildren( NewNode->Geometry, ArrangedChildren );
		
		for (int32 WidgetIndex=0; WidgetIndex < ArrangedChildren.Num(); ++WidgetIndex)
		{
			// Note that we include both visible and invisible children!
			NewNode->ChildNodes.Add( FReflectorNode::NewTreeFrom( ArrangedChildren[WidgetIndex] )  );
		}

		return NewNode;
	}

	/**
	 * Locate all the widgets from a widget path in a list of ReflectorNodes and their children.
	 *
	 * @param CandidateNodes A list of FReflectorNodes that represent widgets.
	 * @param WidgetPathToFind We want to find all reflector nodes corresponding to widgets in this path
	 * @param SearchResult An array that gets results put in it
	 * @param NodeIndexToFind Index of the widget in the path that we are currently looking for; we are done when we've found all of them
	 */
	static void FindWidgetPath( const TArray<TSharedPtr<FReflectorNode>>& CandidateNodes, const FWidgetPath& WidgetPathToFind, TArray< TSharedPtr<FReflectorNode> >& SearchResult, int32 NodeIndexToFind = 0 )
	{
		if (NodeIndexToFind < WidgetPathToFind.Widgets.Num())
		{
			const FArrangedWidget& WidgetToFind = WidgetPathToFind.Widgets[NodeIndexToFind];

			for (int32 NodeIndex=0; NodeIndex < CandidateNodes.Num(); ++NodeIndex)
			{
				if (CandidateNodes[NodeIndex]->Widget.Pin() == WidgetPathToFind.Widgets[NodeIndexToFind].Widget)
				{
					SearchResult.Add(CandidateNodes[NodeIndex]);
					FindWidgetPath(CandidateNodes[NodeIndex]->ChildNodes, WidgetPathToFind, SearchResult, NodeIndexToFind + 1);
				}
			}
		}
	}

protected:

	/**
	 * FReflectorNode must be constructor through static methods.
	 *
	 * @param InWidgetGeometry Widget and associated geometry that this node will represent
	 */
	FReflectorNode( const FArrangedWidget& InWidgetGeometry )
		: Widget( InWidgetGeometry.Widget )
		, Geometry( InWidgetGeometry.Geometry )
		, Tint(FLinearColor(1.0f, 1.0f, 1.0f))
		, bVisualizeThisNode(true)
	{ }
 };
