// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Implements the deployment targets panel.
 */
class SProjectLauncherSimpleDeviceListView
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SProjectLauncherSimpleDeviceListView) { }
		SLATE_EVENT(FOnProfileRun, OnProfileRun)
		SLATE_ATTRIBUTE(bool, IsAdvanced)
	SLATE_END_ARGS()

public:

	/**
	 * Destructor.
	 */
	~SProjectLauncherSimpleDeviceListView( );

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param InModel The data model.
	 * @param InAdvanced Whether or not the elements should show advanced data.
	 */
	void Construct(const FArguments& InArgs, const FProjectLauncherModelRef& InModel);

protected:

	/**
	 * Refreshes the list of device proxies.
	 */
	void RefreshDeviceProxyList();

private:

	// Callback for getting the enabled state of a device proxy list row.
	bool HandleDeviceListRowIsEnabled(ITargetDeviceProxyPtr DeviceProxy) const;

	// Callback when the user clicks the Device Manager hyperlink.
	void HandleDeviceManagerHyperlinkNavigate() const;

	// Callback for getting the tool tip text of a device proxy list row.
	FText HandleDeviceListRowToolTipText(ITargetDeviceProxyPtr DeviceProxy) const;

	// Callback for generating a row in the device list view.
	TSharedRef<ITableRow> HandleDeviceProxyListViewGenerateRow(ITargetDeviceProxyPtr InItem, const TSharedRef<STableViewBase>& OwnerTable) const;

	// Callback for determining the visibility of the device list view.
	EVisibility HandleDeviceProxyListViewVisibility() const;

	// Callback for when a device proxy has been added to the device proxy manager.
	void HandleDeviceProxyManagerProxyAdded(const ITargetDeviceProxyRef& AddedProxy);

	// Callback for when a device proxy has been added to the device proxy manager.
	void HandleDeviceProxyManagerProxyRemoved(const ITargetDeviceProxyRef& RemovedProxy);

private:

	// Holds the list of available device proxies.
	TArray<ITargetDeviceProxyPtr> DeviceProxyList;

	// Holds the device proxy list view .
	TSharedPtr<SListView<ITargetDeviceProxyPtr> > DeviceProxyListView;

	// Holds a pointer to the data model.
	FProjectLauncherModelPtr Model;

	// Specifies whether advanced options are shown.
	TAttribute<bool> IsAdvanced;

	// Holds a delegate to be invoked when a profile is run.
	FOnProfileRun OnProfileRun;
};
