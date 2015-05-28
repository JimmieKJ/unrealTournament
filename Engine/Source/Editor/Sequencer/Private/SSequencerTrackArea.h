// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * The area where tracks( rows of sections ) are displayed
 */
class SSequencerTrackArea : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SSequencerTrackArea )
	{}
		/** The range of time being viewed */
		SLATE_ATTRIBUTE( TRange<float>, ViewRange )
		/** Percentage of horizontal space that the animation outliner takes up */
		SLATE_ATTRIBUTE( float, OutlinerFillPercent )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedRef<FSequencer> InSequencer );

	/**
	 * Updates the track area with new nodes
	 */
	virtual void Update( TSharedPtr<FSequencerNodeTree> InSequencerNodeTree );

	TSharedPtr<SSequencerCurveEditor> GetCurveEditor() { return CurveEditor; }

private:
	/**
	 * Generates a widget for the specified node and puts it into the scroll box
	 *
	 * @param Node	The node to generate a widget for
	 */
	void GenerateWidgetForNode( TSharedRef<FSequencerDisplayNode>& Node );
	
	/**
	 * Recursively generates widgets for each layout node
	 *
	 * @param Nodes to generate widgets for.  Will recurse into each node's children
	 */
	void GenerateLayoutNodeWidgetsRecursive( const TArray< TSharedRef<FSequencerDisplayNode> >& Nodes );

	/** Gets the visibility of the curve editor based on the the current settings. */
	EVisibility GetCurveEditorVisibility() const;
	/** Gets the visibility of the default section controls based on the current settings. */
	EVisibility GetSectionControlVisibility() const;

	/** Gets the fill value for the slot which contains the scrollbar.  This allows the scroll bar to move depending
	  * whether or not the curve editor is visible. */
	float GetScrollBarSlotFill() const;
	/** Gets the fill value for the spacer slot which is to the right of the scrollbar.  This allows the scroll bar 
	  * to move depending whether or not the curve editor is visible. */
	float GetScrollBarSpacerSlotFill() const;

private:
	/** Scrollable area to display widgets */
	TSharedPtr<SScrollBox> ScrollBox;
	/** The current view range */
	TAttribute< TRange<float> > ViewRange;
	/** The fill percentage of the animation outliner */
	TAttribute<float> OutlinerFillPercent;
	/** The main sequencer interface */
	TWeakPtr<FSequencer> Sequencer;
	/** The curve editor. */
	TSharedPtr<SSequencerCurveEditor> CurveEditor;
};
