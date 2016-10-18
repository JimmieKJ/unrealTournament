// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the profile page for the session launcher wizard.
 */
class SProjectLauncherPackagePage
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SProjectLauncherPackagePage) { }
	SLATE_END_ARGS()

public:

	/**
	 * Destructor.
	 */
	~SProjectLauncherPackagePage( );

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param InModel The data model.
	 */
	void Construct( const FArguments& InArgs, const FProjectLauncherModelRef& InModel );

private:

	// Callback for getting the content text of the 'Cook Mode' combo button.
	FText HandlePackagingModeComboButtonContentText( ) const;

	// Callback for clicking an item in the 'Cook Mode' menu.
	void HandlePackagingModeMenuEntryClicked( ELauncherProfilePackagingModes::Type PackagingMode );

	// Callback for getting the visibility of the packaging settings area.
	EVisibility HandlePackagingSettingsAreaVisibility( ) const;

	// Callback for changing the selected profile in the profile manager.
	void HandleProfileManagerProfileSelected( const ILauncherProfilePtr& SelectedProfile, const ILauncherProfilePtr& PreviousProfile );

private:

	// Holds a pointer to the data model.
	FProjectLauncherModelPtr Model;

	TSharedPtr<SProjectLauncherPackagingSettings> ProjectLauncherPackagingSettings;
};
