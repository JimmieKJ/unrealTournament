// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenShotBrowser.h: Declares the SScreenShotBrowser class.
=============================================================================*/

#pragma once


/**
 * Implements a Slate widget for browsing active game sessions.
 */

class SScreenShotBrowser
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SScreenShotBrowser) { }
	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget.
	 *
	 * @param InArgs - The declaration data for this widget.
 	 * @param InScreenShotManager - The screen shot manager containing the screen shot data.
	 */
	void Construct( const FArguments& InArgs, IScreenShotManagerRef InScreenShotManager );

public:

	/**
	 * Generates the widget for the screen items.
	 *
	 * @param InItem - Item to edit.
	 * @param OwnerTable - The owner table
	 * @return The widget.
	 */
	TSharedRef<ITableRow> OnGenerateWidgetForScreenView( TSharedPtr<IScreenShotData> InItem, const TSharedRef<STableViewBase>& OwnerTable );

private:

	/**
	 * Request widgets get regenerated when the data is updated
	 */
	void HandleScreenShotDataChanged();

	/**
	 * Regenerate the widgets when the filter changes
	 */
	void ReGenerateTree();

private:

	// The manager containing the screen shots
	IScreenShotManagerPtr ScreenShotManager;

	// Delegate to call when screen shot data changes 
	FOnScreenFilterChanged ScreenShotDelegate;

	// Holder for the screen shot widgets
	TSharedPtr< SHorizontalBox > TreeBoxHolder;
};
