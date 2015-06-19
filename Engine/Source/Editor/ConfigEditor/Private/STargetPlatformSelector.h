// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#define LOCTEXT_NAMESPACE "ConfigEditor"

/*-----------------------------------------------------------------------------
   STargetPlatformSelector
-----------------------------------------------------------------------------*/
class STargetPlatformSelector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STargetPlatformSelector){}
		SLATE_EVENT(FSimpleDelegate, OnTargetPlatformChanged)
	SLATE_END_ARGS()

	// Begin SCompoundWidget|SWidget interface
	void Construct(const FArguments& InArgs);
	// End SCompoundWidget|SWidget interface

	/**
	 * Access to the selected target platform in this widget.
	 *
	 * @return - The selected platform from the target platform ComboBox.
	 */
	TSharedPtr<FString> GetSelectedTargetPlatform();

private:
	 
	/**
	 * Callback when the user has made a change to the target platform.
	 */
	void HandleTargetPlatformChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);

	/**
	 * Build a list of available target platforms.
	 */
	void CollateAvailableTargetPlatformEntries();

private:

	// The currently selected target platform in this widget.
	TSharedPtr<FString> SelectedTargetPlatform;

	// The combo widget used to make a change to selected target platform.
	TSharedPtr<STextComboBox> AvailableTargetPlatformComboBox;

	// The list of available target platforms for the combo widget.
	TArray< TSharedPtr<FString> > TargetPlatformOptionsSource;

	// Delegate called to let the listener know there has been a change to the target platform.
	FSimpleDelegate OnTargetPlatformChanged;
};

#undef LOCTEXT_NAMESPACE // "ConfigEditor"