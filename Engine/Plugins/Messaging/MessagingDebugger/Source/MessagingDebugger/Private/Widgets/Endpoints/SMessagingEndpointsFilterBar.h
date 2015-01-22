// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the endpoints list filter bar widget.
 */
class SMessagingEndpointsFilterBar
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMessagingEndpointsFilterBar) { }
	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InFilter The filter model.
	 */
	void Construct( const FArguments& InArgs, FMessagingDebuggerEndpointFilterRef InFilter );

private:

	/** Handles changing the filter string text box text. */
	void HandleFilterStringTextChanged( const FText& NewText )
	{
		Filter->SetFilterString(NewText.ToString());
	}

private:

	/** Holds a pointer to the filter model. */
	FMessagingDebuggerEndpointFilterPtr Filter;
};
