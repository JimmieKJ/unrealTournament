// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the device details widget.
 */
class SDeviceApps
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDeviceApps) { }
	SLATE_END_ARGS()

public:

	/**
	 * Destructor.
	 */
	~SDeviceApps( );

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InModel The view model to use.
	 */
	void Construct( const FArguments& InArgs, const FDeviceManagerModelRef& InModel );

private:

	// Callback for generating a row widget in the application list view.
	TSharedRef<ITableRow> HandleAppListViewGenerateRow( TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable );

	// Callback for selecting items in the devices list view.
	void HandleAppListViewSelectionChanged( TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo );

	// Callback for getting the enabled state of the processes panel.
	bool HandleAppsBoxIsEnabled( ) const;

	// Callback for handling device service selection changes.
	void HandleModelSelectedDeviceServiceChanged( );

	// Callback for getting the visibility of the 'Select a device' message.
	EVisibility HandleSelectDeviceOverlayVisibility( ) const;

private:

	// Holds the list of applications deployed to the device.
	TArray<TSharedPtr<FString> > AppList;

	// Holds the application list view.
	TSharedPtr<SListView<TSharedPtr<FString>>> AppListView;

	// Holds a pointer the device manager's view model.
	FDeviceManagerModelPtr Model;
};
