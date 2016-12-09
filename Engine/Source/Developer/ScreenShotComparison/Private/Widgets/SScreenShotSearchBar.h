// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenShotSearchBar.h: Declares the SScreenShotSearchBar class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Interfaces/IScreenShotManager.h"

class FScreenShotComparisonFilter;

class SScreenShotSearchBar : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SScreenShotSearchBar )
	{}

	SLATE_END_ARGS()

	/**
	 * Construct this widget.
	 *
	 * @param InArgs - The declaration data for this widget.
	 * @param InScreenShotManager - The screen shot manager.
	 */
	void Construct( const FArguments& InArgs, IScreenShotManagerPtr InScreenShotManager );

protected:

	/**
	 * Get the string for the platform box.
	 *
	 * @return The platform string.
	 */
	FText GetPlatformString() const;

	/**
	 * Set the view filter based on the input from the search box
	 *
	 * @param InFilterText - The filter text.
	 */
	void HandleFilterTextChanged( const FText& InFilterText );

	/**
	 * Set the filter when a platform is selected.
	 *
	 * @param PlatformName - The platform name.
	 * @param SelectInfo - The selection type.
	 */
	void HandlePlatformListSelected( TSharedPtr<FString> PlatformName, ESelectInfo::Type SelectInfo );

	/**
	 * Refresh the tree view.
	 */
	FReply RefreshView();

	/**
	 * Create a new row for each menu item
	 *
	 * @param PlatformName - The platform name.
	 * @param OwnerTable - The owner table.
	 * @return - The new widget
	 */
	TSharedRef<ITableRow> HandlePlatformListViewGenerateRow( TSharedPtr<FString> PlatformName, const TSharedRef<STableViewBase>& OwnerTable );

private:

	// Holds the display string for the selected platform
	FString PlatformDisplayString;

	// Hold the screen shot manager that contains the screen shot data.
	IScreenShotManagerPtr ScreenShotManager;

	// The screen shot filter collection - contains the screen shot filters.
	TSharedPtr< ScreenShotFilterCollection > ScreenShotFilters;

	// The screen shot comparison general filter.
	TSharedPtr< FScreenShotComparisonFilter > ScreenShotComparisonFilter;

	// The search box used to set the view filter.
	TSharedPtr< SSearchBox > SearchBox;

	/** The list view for showing all log messages. Should be replaced by a full text editor */
	TSharedPtr< SListView< TSharedPtr<FString> > > PlatformListView;
};

