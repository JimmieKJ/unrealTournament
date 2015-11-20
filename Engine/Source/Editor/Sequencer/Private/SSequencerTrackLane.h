// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SSequencerTreeView;
class FSequencerDisplayNode;

/**
 * A wrapper widget responsible for positioning a track lane within the section area
 */
class SSequencerTrackLane : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSequencerTrackLane){}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	/** Construct this widget */
	void Construct(const FArguments& InArgs, const TSharedRef<FSequencerDisplayNode>& InDisplayNode, const TSharedRef<SSequencerTreeView>& InTreeView);
	
	/** Paint this widget */
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/** Get the desired physical vertical position of this track lane */
	float GetPhysicalPosition() const;
	
private:
	/** The authoritative display node that created us */
	TSharedPtr<FSequencerDisplayNode> DisplayNode;
	/** Pointer back to the tree view for virtual <-> physical space conversions. Important: weak ptr to avoid circular references */
	TWeakPtr<SSequencerTreeView> TreeView;
};