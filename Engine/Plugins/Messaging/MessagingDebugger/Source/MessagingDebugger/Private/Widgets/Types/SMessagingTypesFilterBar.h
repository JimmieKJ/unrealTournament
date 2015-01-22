// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the message type list filter bar widget.
 */
class SMessagingTypesFilterBar
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMessagingTypesFilterBar) { }

		/** Called when the filter settings have changed. */
		SLATE_EVENT(FSimpleDelegate, OnFilterChanged)

	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InFilter The filter model.
	 */
	void Construct( const FArguments& InArgs, FMessagingDebuggerTypeFilterRef InFilter );

private:

	/** Handles changing the filter string text box text. */
	void HandleFilterStringTextChanged( const FText& NewText )
	{
		Filter->SetFilterString(NewText.ToString());
	}

private:

	/** Holds the filter model. */
	FMessagingDebuggerTypeFilterPtr Filter;
};
