// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenPlatformRow.h: Declares the SScreenPlatformRow class.
=============================================================================*/

#pragma once

/**
 * A row viewer for the screen shot browser.
 */
class SScreenPlatformRow : public SMultiColumnTableRow< TSharedPtr<IScreenShotData> >
{
public:

	SLATE_BEGIN_ARGS( SScreenPlatformRow )
		: _ScreenShotDataItem()
		{}
		SLATE_ARGUMENT( float, ColumnWidth )
		SLATE_ARGUMENT( TSharedPtr<IScreenShotData>, ScreenShotDataItem )
		SLATE_ARGUMENT(TSharedPtr<STableViewBase>, OwnerTableView)

	SLATE_END_ARGS()

	/**
	 * Construct the widget.
	 *
	 * @param InArgs   A declaration from which to construct the widget.
 	 * @param InOwnerTableView   The owning table data.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView );

public:
	/**
	 * Generates the widget for the specified column.
	 *
	 * @param ColumnName - The name of the column to generate the widget for.
	 *
	 * @return The widget.
	 */
	TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName );

private:

	// Holds the data item that we visualize/edit.
	TSharedPtr<IScreenShotData> ScreenShotDataItem;
};

