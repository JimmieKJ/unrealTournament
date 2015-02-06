// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SButton.h"


/**
 * Expander arrow and indentation component that can be placed in a TableRow
 * of a TreeView. Intended for use by TMultiColumnRow in TreeViews.
 */
class SLATE_API SExpanderArrow : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SExpanderArrow )
		: _IndentAmount(10)
		, _BaseIndentLevel(0)
	{ }
		SLATE_ATTRIBUTE(float, IndentAmount)
		SLATE_ATTRIBUTE(int32, BaseIndentLevel)
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedPtr<class ITableRow>& TableRow );

protected:

	/** Invoked when the expanded button is clicked (toggle item expansion) */
	FReply OnArrowClicked();

	/** @return Visible when has children; invisible otherwise */
	EVisibility GetExpanderVisibility() const;

	/** @return the margin corresponding to how far this item is indented */
	FMargin GetExpanderPadding() const;

	/** @return the name of an image that should be shown as the expander arrow */
	const FSlateBrush* GetExpanderImage() const;

	TWeakPtr<class ITableRow> OwnerRowPtr;

	/** A reference to the expander button */
	TSharedPtr<SButton> ExpanderArrow;

	/** The amount of space to indent at each level */
	TAttribute<float> IndentAmount;

	/** The level in the tree that begins the indention amount */
	TAttribute<int32> BaseIndentLevel;
};
