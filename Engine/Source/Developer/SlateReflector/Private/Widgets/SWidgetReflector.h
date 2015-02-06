// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IWidgetReflector.h"


// forward declarations
struct FLoggedEvent;


/**
 * Abstract base class for widget reflectors.
 */
class SLATEREFLECTOR_API SWidgetReflector
	: public SCompoundWidget
	, public IWidgetReflector
{
	// The reflector uses a tree that observes FReflectorNodes.
	typedef STreeView<TSharedPtr<FReflectorNode>> SReflectorTree;

public:

	SLATE_BEGIN_ARGS(SWidgetReflector) { }
	SLATE_END_ARGS()

	/**
	 * Creates and initializes a new widget reflector widget.
	 *
	 * @param InArgs The construction arguments.
	 */
	void Construct( const FArguments& InArgs );

public:

	// SCompoundWidget overrides

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

public:

	// IWidgetReflector interface

	virtual void OnEventProcessed( const FInputEvent& Event, const FReplyBase& InReply ) override;

	virtual bool IsInPickingMode() const override
	{
		return bIsPicking;
	}

	virtual bool IsShowingFocus() const override
	{
		return bShowFocus;
	}

	virtual bool IsVisualizingLayoutUnderCursor() const override
	{
		return bIsPicking;
	}

	virtual void OnWidgetPicked() override
	{
		bIsPicking = false;
	}

	virtual bool ReflectorNeedsToDrawIn( TSharedRef<SWindow> ThisWindow ) const override;

	virtual void SetSourceAccessDelegate( FAccessSourceCode InDelegate ) override
	{
		SourceAccessDelegate = InDelegate;
	}

	virtual void SetAssetAccessDelegate(FAccessAsset InDelegate) override
	{
		AsseetAccessDelegate = InDelegate;
	}

	virtual void SetWidgetsToVisualize( const FWidgetPath& InWidgetsToVisualize ) override;
	virtual int32 Visualize( const FWidgetPath& InWidgetsToVisualize, FSlateWindowElementList& OutDrawElements, int32 LayerId ) override;

protected:

	/**
	 * Generates a tool tip for the given reflector tree node.
	 *
	 * @param InReflectorNode The node to generate the tool tip for.
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
	 * @return The maximum layer ID we achieved while painting.
	 */
	int32 VisualizeSelectedNodesAsRectangles( const TArray<TSharedPtr<FReflectorNode>>& InNodesToDraw, const TSharedRef<SWindow>& VisualizeInWindow, FSlateWindowElementList& OutDrawElements, int32 LayerId );

private:

	/** Callback for changing the application scale slider. */
	void HandleAppScaleSliderChanged( float NewValue )
	{
		FSlateApplication::Get().SetApplicationScale(NewValue);
	}

	FReply HandleDisplayTextureAtlases();

	FReply HandleDisplayFontAtlases();

	/** Callback for getting the value of the application scale slider. */
	float HandleAppScaleSliderValue() const
	{
		return FSlateApplication::Get().GetApplicationScale();
	}

	/** Callback for checked state changes of the focus check box. */
	void HandleFocusCheckBoxCheckedStateChanged( ECheckBoxState NewValue );

	/** Callback for getting the checked state of the focus check box. */
	ECheckBoxState HandleFocusCheckBoxIsChecked() const
	{
		return bShowFocus ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	/** Callback for getting the text of the frame rate text block. */
	FString HandleFrameRateText() const;

	/** Callback for clicking the pick button. */
	FReply HandlePickButtonClicked()
	{
		bIsPicking = !bIsPicking;

		if (bIsPicking)
		{
			bShowFocus = false;
		}

		return FReply::Handled();
	}

	/** Callback for getting the color of the pick button text. */
	FSlateColor HandlePickButtonColorAndOpacity() const
	{
		static const FName SelectionColor("SelectionColor");
		static const FName DefaultForeground("DefaultForeground");

		return bIsPicking
			? FCoreStyle::Get().GetSlateColor(SelectionColor)
			: FCoreStyle::Get().GetSlateColor(DefaultForeground);
	}

	/** Callback for getting the text of the pick button. */
	FString HandlePickButtonText() const;

	/** Callback for generating a row in the reflector tree view. */
	TSharedRef<ITableRow> HandleReflectorTreeGenerateRow( TSharedPtr<FReflectorNode> InReflectorNode, const TSharedRef<STableViewBase>& OwnerTable );

	/** Callback for getting the child items of the given reflector tree node. */
	void HandleReflectorTreeGetChildren( TSharedPtr<FReflectorNode> InWidgetGeometry, TArray<TSharedPtr<FReflectorNode>>& OutChildren );

	/** Callback for when the selection in the reflector tree has changed. */
	void HandleReflectorTreeSelectionChanged( TSharedPtr< FReflectorNode >, ESelectInfo::Type /*SelectInfo*/ )
	{
		SelectedNodes = ReflectorTree->GetSelectedItems();
	}

	TSharedRef<ITableRow> GenerateEventLogRow( TSharedRef<FLoggedEvent> InReflectorNode, const TSharedRef<STableViewBase>& OwnerTable );

private:

	TArray< TSharedRef<FLoggedEvent> > LoggedEvents;
	TSharedPtr< SListView< TSharedRef< FLoggedEvent > > > EventListView;

	TSharedPtr<SReflectorTree> ReflectorTree;

	TArray<TSharedPtr<FReflectorNode>> SelectedNodes;
	TArray<TSharedPtr<FReflectorNode>> ReflectorTreeRoot;
	TArray<TSharedPtr<FReflectorNode>> PickedPath;

	SSplitter::FSlot* WidgetInfoLocation;

	FAccessSourceCode SourceAccessDelegate;
	FAccessAsset AsseetAccessDelegate;

	bool bShowFocus;
	bool bIsPicking;

private:
	// STATS
	TSharedPtr<SBorder> StatsBorder;
	TArray< TSharedRef<class FStatItem> > StatsItems;
	TSharedPtr< SListView< TSharedRef< FStatItem > > > StatsList;

	TSharedRef<SWidget> MakeStatViewer();
	void UpdateStats();
	TSharedRef<ITableRow> GenerateStatRow( TSharedRef<FStatItem> StatItem, const TSharedRef<STableViewBase>& OwnerTable );
	FReply CopyStatsToClipboard();
};
