// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// This widget provides a fully-zoomed-out preview of a specified graph
class GRAPHEDITOR_API SGraphPreviewer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGraphPreviewer)
		: _ShowGraphStateOverlay(true)
	{}
		SLATE_ATTRIBUTE( FText, CornerOverlayText )
		/** Show overlay elements for the graph state such as the PIE and read-only borders and text */
		SLATE_ATTRIBUTE(bool, ShowGraphStateOverlay)
		SLATE_ARGUMENT( TSharedPtr<SWidget>, TitleBar )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraph* InGraphObj);
public:
	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	// End of SWidget interface
protected:
	/// The Graph we are currently viewing
	UEdGraph* EdGraphObj;

	// Do we need to refresh next tick?
	bool bNeedsRefresh;

	// The underlying graph panel
	TSharedPtr<SGraphPanel> GraphPanel;
};
