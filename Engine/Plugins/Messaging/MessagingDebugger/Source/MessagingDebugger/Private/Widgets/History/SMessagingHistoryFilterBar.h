// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#define LOCTEXT_NAMESPACE "SMessagingHistoryFilterBar"


/**
 * Implements the messaging history command bar widget.
 */
class SMessagingHistoryFilterBar
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMessagingHistoryFilterBar) { }
	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InFilter The filter model.
	 */
	void Construct( const FArguments& InArgs, FMessagingDebuggerMessageFilterRef InFilter )
	{
		ChildSlot
		[
			SNullWidget::NullWidget
		];
	}

private:

	/** Holds the filter model. */
	FMessagingDebuggerMessageFilterPtr Filter;

};


#undef LOCTEXT_NAMESPACE
