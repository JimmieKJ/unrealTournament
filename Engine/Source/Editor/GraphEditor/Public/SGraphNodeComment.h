// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SGraphNodeComment : public SGraphNodeResizable
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeComment){}
	SLATE_END_ARGS()

	// Begin SWidget Interface
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	// End SWidget Interface

	// Begin SNodePanel::SNode interface
	virtual const FSlateBrush* GetShadowBrush(bool bSelected) const override;
	virtual void GetOverlayBrushes(bool bSelected, const FVector2D WidgetSize, TArray<FOverlayBrushInfo>& Brushes) const override;
	virtual bool ShouldAllowCulling() const override { return false; }
	virtual int32 GetSortDepth() const override;
	virtual void EndUserInteraction() const override;
	virtual FString GetNodeComment() const override;
	// End SNodePanel::SNode interface

	// Begin SPanel Interface
	virtual FVector2D ComputeDesiredSize(float) const override;
	// End SPanel interface

	// Begin SGraphNode Interface
	virtual bool IsNameReadOnly() const override;
	virtual FSlateColor GetCommentColor() const override { return GetCommentBodyColor(); }
	// End SGraphNode Interface

	void Construct( const FArguments& InArgs, UEdGraphNode* InNode );

	/** return if the node can be selected, by pointing given location */
	virtual bool CanBeSelected( const FVector2D& MousePositionInNode ) const override;

	/** return size of the title bar */
	virtual FVector2D GetDesiredSizeForMarquee() const override;

	/** return rect of the title bar */
	virtual FSlateRect GetTitleRect() const override;

protected:
	// SGraphNode Interface
	virtual void UpdateGraphNode() override;
	virtual void PopulateMetaTag(class FGraphNodeMetaData* TagMeta) const override;

	/**
	 * Helper method to update selection state of comment and any nodes 'contained' within it
	 * @param bSelected	If true comment is being selected, false otherwise
	 * @param bUpdateNodesUnderComment If true then force the rebuild of the list of nodes under the comment
	 */
	void HandleSelection(bool bIsSelected, bool bUpdateNodesUnderComment = false) const;

	/** called when user is moving the comment node */
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter ) override;

	// SGraphNodeResizable Interface
	virtual float GetTitleBarHeight() const override;
	virtual FSlateRect GetHitTestingBorder( float InverseZoomFactor ) const override;
	virtual FVector2D GetNodeMaximumSize() const override;
	// SGraphNodeResizable Interface

private:

	/** @return the color to tint the comment body */
	FSlateColor GetCommentBodyColor() const;

	/** @return the color to tint the title bar */
	FSlateColor GetCommentTitleBarColor() const;

	/** @return the color to tint the comment bubble */
	FSlateColor GetCommentBubbleColor() const;

	/** Returns the width to wrap the text of the comment at */
	float GetWrapAt() const;

private:

	/** The current selection state of the comment */
	mutable bool bIsSelected;

	/** the title bar, needed to obtain it's height */
	TSharedPtr<SBorder> TitleBar;

	/** cached comment title */
	FString CachedCommentTitle; 

	/** cached comment title */
	int32 CachedWidth;
};
