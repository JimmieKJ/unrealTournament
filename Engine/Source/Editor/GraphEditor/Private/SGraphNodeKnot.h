// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

	/** Returns a custom hover state for the comment bubble */
	bool IsKnotHovered() const;

	/** Returns Offset to center comment on the node's only pin */
	FVector2D GetCommentOffset() const;

	/** Called to override the comment bubble's current visibility */
	ECheckBoxState GetBubbleCheckState() const;

protected:

	/** The hovered visibility state */
	bool bHoveredCommentVisibility;
	/** SharedPtr to comment bubble */
	TSharedPtr<SCommentBubble> CommentBubble;

};
