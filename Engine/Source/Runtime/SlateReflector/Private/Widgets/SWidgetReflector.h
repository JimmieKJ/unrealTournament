// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SWidgetReflector.h: Declares the SWidgetReflector class.
=============================================================================*/

#pragma once


/** A Delegate for passing along a string of a source code location to access */
DECLARE_DELEGATE_RetVal_ThreeParams(bool, FAccessSourceCode, const FString& /*FileName*/, int32 /*InLineNumber*/, int32 /*InColumnNumber*/);


/**
 * Abstract base class for widget reflectors.
 */
class SWidgetReflector
	: public SCompoundWidget
	, public IWidgetReflector
{
	// The reflector uses a tree that observes FReflectorNodes.
	typedef STreeView<TSharedPtr<FReflectorNode>> SReflectorTree;

public:

	SLATE_BEGIN_ARGS(SWidgetReflector) { }
	SLATE_END_ARGS()

	/** Create widgets that comprise this WidgetReflector implementation */
	/**
	 * Creates and initializes a new widget reflector widget.
	 *
	 * @param InArgs The construction arguments.
	 */
	void Construct( const FArguments& InArgs );

public:

	// Begin SCompoundWidget overrides

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) OVERRIDE;

	// End SCompoundWidget overrides

public:

	// Begin IWidgetReflector interface

	virtual bool IsInPickingMode( ) const OVERRIDE
	{
		return bIsPicking;
	}

	virtual bool IsShowingFocus( ) const OVERRIDE
	{
		return bShowFocus;
	}

	virtual bool IsVisualizingLayoutUnderCursor( ) const OVERRIDE
	{
		return bIsPicking;
	}

	virtual void OnWidgetPicked( ) OVERRIDE
	{
		bIsPicking = false;
	}

	virtual bool ReflectorNeedsToDrawIn( TSharedRef<SWindow> ThisWindow ) const OVERRIDE;

	virtual void SetSourceAccessDelegate( FAccessSourceCode InDelegate ) OVERRIDE
	{
		SourceAccessDelegate = InDelegate;
	}

	virtual void SetWidgetsToVisualize( const FWidgetPath& InWidgetsToVisualize ) OVERRIDE;

	virtual int32 Visualize( const FWidgetPath& InWidgetsToVisualize, FSlateWindowElementList& OutDrawElements, int32 LayerId ) OVERRIDE;

	// End IWidgetReflector interface

protected:

	/**
	 * Generates a tool tip for the given reflector tree node.
	 *
	 * @param InReflectorNode The node to generate the tool tip for.
	 *
	 * @return The tool tip widget.
	 */
	TSharedRef<SToolTip> GenerateToolTipForReflectorNode( TSharedPtr<FReflectorNode> InReflectorNode );

	/**
	 * Mark the provided reflector nodes such that they stand out in the tree and are visible.
	 *
	 * @param WidgetPathToObserve The nodes to mark.
	 */
	void VisualizeAsTree( const TArray< TSharedPtr<FReflectorNode> >& WidgetPathToVisualize );

	/**
	 * Draw the widget path to the picked widget as the widgets' outlines.
	 *
	 * @param InWidgetsToVisualize A widget path whose widgets' outlines to draw.
	 * @param OutDrawElements A list of draw elements; we will add the output outlines into it.
	 * @param LayerId The maximum layer achieved in OutDrawElements so far.
	 *
	 * @return The maximum layer ID we achieved while painting.
	 */
	int32 VisualizePickAsRectangles( const FWidgetPath& InWidgetsToVisualize, FSlateWindowElementList& OutDrawElements, int32 LayerId );

	/**
	 * Draw an outline for the specified nodes.
	 *
	 * @param InNodesToDraw A widget path whose widgets' outlines to draw.
	 * @param WindowGeometry The geometry of the window in which to draw.
	 * @param OutDrawElements A list of draw elements; we will add the output outlines into it.
	 * @param LayerId the maximum layer achieved in OutDrawElements so far.
	 *
	 * @return The maximum layer ID we achieved while painting.
	 */
	int32 VisualizeSelectedNodesAsRectangles( const TArray<TSharedPtr<FReflectorNode>>& InNodesToDraw, const TSharedRef<SWindow>& VisualizeInWindow, FSlateWindowElementList& OutDrawElements, int32 LayerId );

private:

	// Callback for changing the application scale slider
	void HandleAppScaleSliderChanged( float NewValue )
	{
		FSlateApplication::Get().SetApplicationScale(NewValue);
	}

	// Callback for getting the value of the application scale slider.
	float HandleAppScaleSliderValue( ) const
	{
		return FSlateApplication::Get().GetApplicationScale();
	}

	// Callback for checked state changes of the focus check box.
	void HandleFocusCheckBoxCheckedStateChanged( ESlateCheckBoxState::Type NewValue );

	// Callback for getting the checked state of the focus check box.
	ESlateCheckBoxState::Type HandleFocusCheckBoxIsChecked( ) const
	{
		return bShowFocus ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
	}

	// Callback for getting the text of the frame rate text block.
	FString HandleFrameRateText( ) const;

	// Callback for clicking the pick button.
	FReply HandlePickButtonClicked( )
	{
		bIsPicking = !bIsPicking;

		if (bIsPicking)
		{
			bShowFocus = false;
		}

		return FReply::Handled();
	}

	// Callback for getting the color of the pick button text.
	FSlateColor HandlePickButtonColorAndOpacity( ) const
	{
		return bIsPicking
			? FCoreStyle::Get().GetSlateColor("SelectionColor")
			: FCoreStyle::Get().GetSlateColor("DefaultForeground");
	}

	// Callback for getting the text of the pick button.
	FString HandlePickButtonText() const;

	// Callback for generating a row in the reflector tree view.
	TSharedRef<ITableRow> HandleReflectorTreeGenerateRow( TSharedPtr<FReflectorNode> InReflectorNode, const TSharedRef<STableViewBase>& OwnerTable );

	// Callback for getting the child items of the given reflector tree node.
	void HandleReflectorTreeGetChildren( TSharedPtr<FReflectorNode> InWidgetGeometry, TArray<TSharedPtr<FReflectorNode>>& OutChildren )
	{
		OutChildren = InWidgetGeometry->ChildNodes;
	}

	// Callback for when the selection in the reflector tree has changed.
	void HandleReflectorTreeSelectionChanged( TSharedPtr< FReflectorNode >, ESelectInfo::Type /*SelectInfo*/ )
	{
		SelectedNodes = ReflectorTree->GetSelectedItems();
	}

private:

	TSharedPtr<SReflectorTree> ReflectorTree;

	TArray<TSharedPtr<FReflectorNode>> SelectedNodes;
	TArray<TSharedPtr<FReflectorNode>> ReflectorTreeRoot;
	TArray<TSharedPtr<FReflectorNode>> PickedPath;

	SSplitter::FSlot* WidgetInfoLocation;

	FAccessSourceCode SourceAccessDelegate;

	bool bShowFocus;
	bool bIsPicking;
};
