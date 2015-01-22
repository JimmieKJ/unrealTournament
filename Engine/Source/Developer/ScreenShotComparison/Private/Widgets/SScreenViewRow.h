// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenViewRow.h: Declares the SScreenViewRow class.
=============================================================================*/

#pragma once

/**
 * Widget to display a particular view.
 */
class SScreenViewRow : public STableRow< TSharedPtr<IScreenShotData> >
{
public:

	SLATE_BEGIN_ARGS( SScreenViewRow )
		: _ScreenShotData()
		{}
		SLATE_ARGUMENT( TSharedPtr<IScreenShotData>, ScreenShotData )

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
	 * Generates the widget for the screen items.
	 *
	 * @param InScreenShotDataItem - Item to edit.
	 * @param OwnerTable - The owner table,
	 * @return The widget.
	 */
	TSharedRef<ITableRow> OnGenerateWidgetForScreenView( TSharedPtr<IScreenShotData> InScreenShotDataItem, const TSharedRef<STableViewBase>& OwnerTable );

private:

	// Holds the screen shot list view.
	TSharedPtr< SListView< TSharedPtr<IScreenShotData> > > ScreenShotDataView;
};

