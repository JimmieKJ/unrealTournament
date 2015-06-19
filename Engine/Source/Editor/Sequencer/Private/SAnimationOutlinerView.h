// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * A widget for displaying a sequencer tree node in the animation outliner
 */
class SAnimationOutlinerTreeNode : public SCompoundWidget
{
public:
	/** A delegate called when the widget is selected */
	DECLARE_DELEGATE_OneParam( FOnNodeSelectionChanged, TSharedPtr<class FSequencerDisplayNode> )

	SLATE_BEGIN_ARGS( SAnimationOutlinerTreeNode ) {}
		/** Called when the widget is selected */	
		SLATE_EVENT( FOnNodeSelectionChanged, OnSelectionChanged )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedRef<FSequencerDisplayNode> Node );

	/**
	 * @return The display node used by this widget                                                              
	 */
	const TSharedPtr< const FSequencerDisplayNode> GetDisplayNode() const { return DisplayNode; }
private:
	/** SWidget Interface */
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	/**
	 * Called when the expander arrow is clicked
	 */
	FReply OnExpanderClicked();

	/**
	 * @return The border image to show in the tree node
	 */
	const FSlateBrush* GetNodeBorderImage() const;

	/**
	 * @return The expander image to show
	 */
	const FSlateBrush* OnGetExpanderImage() const;

	/**
	 * @return The visibility of this node                                                              
	 */
	EVisibility GetNodeVisibility() const;
	
	/**
	 * @return The expander visibility of this node                                                              
	 */
	EVisibility GetExpanderVisibility() const;

	/**
	 * @return The display name for this node.
	 */
	FText GetDisplayName() const;

private:
	/** Layout node the widget is visualizing */
	TSharedPtr<FSequencerDisplayNode> DisplayNode;
	/** Delegate to call when the node is selected */
	FOnNodeSelectionChanged OnSelectionChanged;
	/** Brush to display a border around the widget when it is selected */
	const FSlateBrush* SelectedBrush;
	/** Brush to display a border around the widget when it is selected but inactive */
	const FSlateBrush* SelectedBrushInactive;
	/** Brush to use if the node is not selected */
	const FSlateBrush* NotSelectedBrush;
	/** Brush to use for the expander image when this node is expanded */
	const FSlateBrush* ExpandedBrush;
	/** BBrush to use for the expamnder image when this node is collapsed */
	const FSlateBrush* CollapsedBrush;
};

typedef TSequencerTrackViewPanel<SAnimationOutlinerTreeNode> SAnimationOutlinerViewBase;

/**
 * AnimationOutliner
 */
class SAnimationOutlinerView : public SAnimationOutlinerViewBase
{

public:
	SLATE_BEGIN_ARGS( SAnimationOutlinerView ) {}
	SLATE_END_ARGS()

	/** Construct this widget.  Called by the SNew() Slate macro. */
	void Construct( const FArguments& InArgs, TSharedRef<FSequencerDisplayNode> RootNode, TSharedRef<FSequencer> InSequencer );

	/** SAnimationOutlinerView destructor */
	virtual ~SAnimationOutlinerView();
private:
	/** @return The visibility of our root node */
	EVisibility GetNodeVisibility() const;

	/** Generates an internal widget for the specified LayoutNode */
	void GenerateWidgetForNode( TSharedRef<FSequencerDisplayNode>& InLayoutNode );

	/** SPanel Interface */
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;

	/**
	 * Called when a node is selected in the outliner
	 *
	 * @param AffectedNode	The node that was selected
	 */
	void OnSelectionChanged( TSharedPtr<FSequencerDisplayNode> AffectedNode );

private:
	/** Internal sequencer interface */
	TWeakPtr<FSequencer> Sequencer;
};