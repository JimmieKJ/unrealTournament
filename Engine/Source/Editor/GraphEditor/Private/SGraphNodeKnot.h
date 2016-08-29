// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphNodeDefault.h"
#include "K2Node_Knot.h"

class SCommentBubble;

class SGraphNodeKnot : public SGraphNodeDefault
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeKnot) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, class UK2Node_Knot* InKnot);

	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	virtual const FSlateBrush* GetShadowBrush(bool bSelected) const override;
	virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	virtual void RequestRenameOnSpawn() override { }
	// End of SGraphNode interface

	// SWidget interface
	virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override;
	// End of SWidget interface

protected:
	/** Returns Offset to center comment on the node's only pin */
	FVector2D GetCommentOffset() const;
protected:

	/** Toggles the hovered visibility state */
	virtual void OnCommentBubbleToggled(bool bInCommentBubbleVisible) override;

	/** If bHoveredCommentVisibility is true, hides the comment bubble after a change is committed */
	virtual void OnCommentTextCommitted(const FText& NewComment, ETextCommit::Type CommitInfo) override;

	/** The hovered visibility state. If false, comment bubble will only appear on hover. */
	bool bAlwaysShowCommentBubble;

	/** SharedPtr to comment bubble */
	TSharedPtr<SCommentBubble> CommentBubble;
};
