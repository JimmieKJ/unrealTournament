// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class GRAPHEDITOR_API SGraphNodeK2Base : public SGraphNode
{
public:

	// SGraphNode interface
	virtual void UpdateGraphNode() override;

	// SNodePanel::SNode interface
	virtual bool RequiresSecondPassLayout() const override;
	virtual void GetOverlayBrushes(bool bSelected, const FVector2D WidgetSize, TArray<FOverlayBrushInfo>& Brushes) const override;
	virtual void GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const override;
	virtual const FSlateBrush* GetShadowBrush(bool bSelected) const override;
	void PerformSecondPassLayout(const TMap< UObject*, TSharedRef<SNode> >& NodeToWidgetLookup) const override;

protected :
	// Begin SGraphNode interface
	virtual TSharedPtr<SToolTip> GetComplexTooltip() override;
	// End SGraphNode interface

	/** Set up node in 'standard' mode */
	void UpdateStandardNode();
	/** Set up node in 'compact' mode */
	void UpdateCompactNode();

	/** Get title in compact mode */
	FText GetNodeCompactTitle() const;

	/** Retrieves text to tack on to the top of the tooltip (above the standard text) */
	FText GetToolTipHeading() const;

protected:
	static const FLinearColor BreakpointHitColor;
	static const FLinearColor LatentBubbleColor;
	static const FLinearColor TimelineBubbleColor;
	static const FLinearColor PinnedWatchColor;
};
