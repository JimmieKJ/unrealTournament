// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Delegate type for selection changes in the device browser.
 *
 * The first parameter is the newly selected device service (or NULL if none was selected).
 */
DECLARE_DELEGATE_OneParam(FOnDeviceBrowserSelectionChanged, const ITargetDeviceServicePtr&)


/**
 * Implements the device browser widget.
 */
class SDeviceBrowser
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDeviceBrowser) { }
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InModel The view model to use.
	 * @param InDeviceServiceManager The target device service manager to use.
	 * @param InUICommandList The UI command list to use.
	 */
	void Construct( const FArguments& InArgs, const FDeviceManagerModelRef& InModel, const ITargetDeviceServiceManagerRef& InDeviceServiceManager, const TSharedPtr<FUICommandList>& InUICommandList );

public:

	// SCompoundWidget overrides

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

protected:

	/**
	 * Reloads the list of target device services.
	 *
	 * @param FullyReload Whether to fully reload the service entries, or only re-apply filtering.
	 */
	void ReloadDeviceServiceList( bool FullyReload );

private:

	// Callback for generating a context menu in the session list view.
	TSharedPtr<SWidget> HandleDeviceServiceListViewContextMenuOpening( );

	// Callback for generating a row widget in the device service list view.
	TSharedRef<ITableRow> HandleDeviceServiceListViewGenerateRow( ITargetDeviceServicePtr DeviceService, const TSharedRef<STableViewBase>& OwnerTable );

	// Callback for getting the highlight text in session rows.
	FText HandleDeviceServiceListViewHighlightText( ) const;

	// Callback for selecting items in the devices list view.
	void HandleDeviceServiceListViewSelectionChanged( ITargetDeviceServicePtr Selection, ESelectInfo::Type SelectInfo );

	// Callback for added target device services.
	void HandleDeviceServiceManagerServiceAdded( const ITargetDeviceServiceRef& AddedService );

	// Callback for added target device services.
	void HandleDeviceServiceManagerServiceRemoved( const ITargetDeviceServiceRef& RemovedService );

	// Callback for changing the filter settings.
	void HandleFilterChanged( );

private:

	// Holds the list of all available target device services.
	TArray<ITargetDeviceServicePtr> AvailableDeviceServices;

	// Holds the list of filtered target device services for display.
	TArray<ITargetDeviceServicePtr> DeviceServiceList;

	// Holds the list view for filtered target device services.
	TSharedPtr<SListView<ITargetDeviceServicePtr> > DeviceServiceListView;

	// Holds a pointer to the target device service manager.
	ITargetDeviceServiceManagerPtr DeviceServiceManager;

	// Holds the filter model.
	FDeviceBrowserFilterPtr Filter;

	// Holds a pointer the device manager's view model.
	FDeviceManagerModelPtr Model;

	// Holds a flag indicating whether the list of target device services needs to be refreshed.
	bool NeedsServiceListRefresh;

	// Holds a pointer to the command list for controlling the device.
	TSharedPtr<FUICommandList> UICommandList;
};
