// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Type definition for shared pointers to instances of FDeviceManagerModel. */
typedef TSharedPtr<class FDeviceManagerModel> FDeviceManagerModelPtr;

/** Type definition for shared references to instances of FDeviceManagerModel. */
typedef TSharedRef<class FDeviceManagerModel> FDeviceManagerModelRef;


/**
 * Implements a view model for the messaging debugger.
 */
class FDeviceManagerModel
{
public:

	/**
	 * Gets the device service that is currently selected in the device browser.
	 *
	 * @return The selected device service.
	 */
	ITargetDeviceServicePtr GetSelectedDeviceService( ) const
	{
		return SelectedDeviceService;
	}

	/**
	 * Selects the specified device service (or none if nullptr).
	 *
	 * @param DeviceService The device service to select.
	 */
	void SelectDeviceService( const ITargetDeviceServicePtr& DeviceService )
	{
		if (SelectedDeviceService != DeviceService)
		{
			SelectedDeviceService = DeviceService;
			SelectedDeviceServiceChangedEvent.Broadcast();
		}
	}

public:

	/**
	 * Gets an event delegate that is invoked when the selected device service has changed.
	 *
	 * @return The event delegate.
	 */
	DECLARE_EVENT(FDeviceManagerModel, FOnSelectedDeviceServiceChanged);
	FOnSelectedDeviceServiceChanged& OnSelectedDeviceServiceChanged( )
	{
		return SelectedDeviceServiceChangedEvent;
	}

private:

	// Holds the currently selected target device service.
	ITargetDeviceServicePtr SelectedDeviceService;

private:

	// Holds an event delegate that is invoked when the selected message has changed.
	FOnSelectedDeviceServiceChanged SelectedDeviceServiceChangedEvent;
};
