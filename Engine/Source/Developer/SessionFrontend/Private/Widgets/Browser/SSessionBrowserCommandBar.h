// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the session browser's command bar widget.
 */
class SSessionBrowserCommandBar
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSessionBrowserCommandBar) { }
	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 */
	void Construct( const FArguments& InArgs );
};
