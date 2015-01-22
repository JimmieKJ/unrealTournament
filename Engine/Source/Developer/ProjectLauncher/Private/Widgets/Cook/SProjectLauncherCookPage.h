// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the profile page for the session launcher wizard.
 */
class SProjectLauncherCookPage
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SProjectLauncherCookPage) { }
	SLATE_END_ARGS()

public:

	/**
	 * Destructor.
	 */
	~SProjectLauncherCookPage( );

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param InModel The data model.
	 */
	void Construct(	const FArguments& InArgs, const FProjectLauncherModelRef& InModel );

private:

	// Callback for getting the visibility of the cook-by-the-book settings area.
	EVisibility HandleCookByTheBookSettingsVisibility( ) const;

	// Callback for getting the content text of the 'Cook Mode' combo button.
	FText HandleCookModeComboButtonContentText( ) const;

	// Callback for clicking an item in the 'Cook Mode' menu.
	void HandleCookModeMenuEntryClicked( ELauncherProfileCookModes::Type CookMode );

	// Callback for getting the visibility of the cook-on-the-fly settings area.
	EVisibility HandleCookOnTheFlySettingsVisibility( ) const;

	// Callback for changing the list of profiles in the profile manager.
	void HandleProfileManagerProfileListChanged( );

	// Callback for changing the selected profile in the profile manager.
	void HandleProfileManagerProfileSelected( const ILauncherProfilePtr& SelectedProfile, const ILauncherProfilePtr& PreviousProfile );

private:

	// Holds the cooker options text box.
	TSharedPtr<SEditableTextBox> CookerOptionsTextBox;

	// Holds the list of available cook modes.
	TArray<TSharedPtr<FString> > CookModeList;

	// Holds a pointer to the data model.
	FProjectLauncherModelPtr Model;
};
