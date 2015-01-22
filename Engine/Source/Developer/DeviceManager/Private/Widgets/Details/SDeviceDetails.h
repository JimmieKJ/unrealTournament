// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the device details widget.
 */
class SDeviceDetails
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDeviceDetails) { }
	SLATE_END_ARGS()

public:

	/**
	 * Destructor.
	 */
	~SDeviceDetails( );

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InModel The view model to use.
	 */
	void Construct( const FArguments& InArgs, const FDeviceManagerModelRef& InModel );

private:

	// Callback for getting the visibility of the details box.
	EVisibility HandleDetailsBoxVisibility( ) const;

	// Callback for generating a row widget for the feature list view.
	TSharedRef<ITableRow> HandleFeatureListGenerateRow( FDeviceDetailsFeaturePtr Feature, const TSharedRef<STableViewBase>& OwnerTable );

	// Callback for handling device service selection changes.
	void HandleModelSelectedDeviceServiceChanged( );

	// Callback for getting the visibility of the 'Select a device' message.
	EVisibility HandleSelectDeviceOverlayVisibility( ) const;

private:

	// Holds the list of device features.
	TArray<FDeviceDetailsFeaturePtr> FeatureList;

	// Holds the device's feature list view.
	TSharedPtr<SListView<FDeviceDetailsFeaturePtr> > FeatureListView;

	// Holds a pointer the device manager's view model.
	FDeviceManagerModelPtr Model;

	// Holds the quick information widget.
	TSharedPtr<SDeviceQuickInfo> QuickInfo;
};
