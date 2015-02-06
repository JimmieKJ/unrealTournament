// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements a widget for manually locating target devices.
 */
class SDeviceBrowserDeviceAdder
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDeviceBrowserDeviceAdder) { }
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InDeviceManager The target device manager to use.
	 */
	void Construct( const FArguments& InArgs, const ITargetDeviceServiceManagerRef& InDeviceServiceManager );

protected:

	/**
	 * Determines the visibility of the AddUnlistedButton.
	 */
	void DetermineAddUnlistedButtonVisibility( );

	/**
	 * Refreshes the list of known platforms.
	 */
	void RefreshPlatformList( );

private:

	// Callback for clicking of the Add button.
	FReply HandleAddButtonClicked( );

	// Callback for determining the enabled state of the 'Add' button.
	bool HandleAddButtonIsEnabled( ) const;

	// Callback for determining the visibility of the credentials box.
	EVisibility HandleCredentialsBoxVisibility( ) const;

	// Callback for changes in the device name text box.
	void HandleDeviceNameTextBoxTextChanged ( const FString& Text );

	// Callback for getting the name of the selected platform.
	FText HandlePlatformComboBoxContentText( ) const;

	// Callback for generating widgets for the platforms combo box.
	TSharedRef<SWidget> HandlePlatformComboBoxGenerateWidget( TSharedPtr<FString> StringItem );

	// Callback for handling platform selection changes.
	void HandlePlatformComboBoxSelectionChanged( TSharedPtr<FString> StringItem, ESelectInfo::Type SelectInfo );

private:

	// Holds the button for adding an unlisted device.
	TSharedPtr<SButton> AddButton;

	// Holds the device identifier text box.
	TSharedPtr<SEditableTextBox> DeviceIdTextBox;

	// Holds a pointer to the target device service manager.
	ITargetDeviceServiceManagerPtr DeviceServiceManager;

	// Holds the device name text box.
	TSharedPtr<SEditableTextBox> DeviceNameTextBox;

	// Holds the user name text box.
	TSharedPtr<SEditableTextBox> UserNameTextBox;

	// Holds the user password text box.
	TSharedPtr<SEditableTextBox> UserPasswordTextBox;

	// Holds the turnable overlay with user data
	TSharedPtr<SOverlay> UserDataOverlay;

	// Holds the platforms combo box.
	TSharedPtr<SComboBox<TSharedPtr<FString>>> PlatformComboBox;

	// Holds the list of known platforms.
	TArray<TSharedPtr<FString>> PlatformList;
};
