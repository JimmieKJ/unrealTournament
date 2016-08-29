// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the packaging settings panel.
 */
class SProjectLauncherPackagingSettings
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SProjectLauncherPackagingSettings) { }
	SLATE_END_ARGS()

public:

	/**
	 * Destructor.
	 */
	~SProjectLauncherPackagingSettings( );

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param InModel The data model.
	 */
	void Construct( const FArguments& InArgs, const FProjectLauncherModelRef& InModel );

	// Changes the directory path to the correct packaging mode version
	void UpdateDirectoryPathText();

private:


	void HandleForDistributionCheckBoxCheckStateChanged(ECheckBoxState NewState);
	ECheckBoxState HandleForDistributionCheckBoxIsChecked() const;

	// Callback for getting the content text of the 'Directory' label.
	FText HandleDirectoryTitleText() const;
	FText HandleDirectoryPathText() const;

	// Callback for changing the selected profile in the profile manager.
	void HandleProfileManagerProfileSelected( const ILauncherProfilePtr& SelectedProfile, const ILauncherProfilePtr& PreviousProfile );

	FReply HandleBrowseButtonClicked();

	/** Handles entering in a command */
	bool IsEditable() const;

	void OnTextCommitted(const FText& InText, ETextCommit::Type CommitInfo);

	void OnTextChanged(const FText& InText);

private:

	// Holds a pointer to the data model.
	FProjectLauncherModelPtr Model;

	// Holds the repository path text box.
	TSharedPtr<SEditableTextBox> DirectoryPathTextBox;
};
