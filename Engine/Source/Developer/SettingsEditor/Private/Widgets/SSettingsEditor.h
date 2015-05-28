// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NotifyHook.h"
#include "SNotificationList.h"


// forward declarations
class FEditPropertyChain;
struct FPropertyChangedEvent;
class IDetailsView;
class SSettingsEditorCheckoutNotice;
class UObject;


/**
 * Implements an editor widget for settings.
 */
class SSettingsEditor
	: public SCompoundWidget
	, public FNotifyHook
{
public:

	SLATE_BEGIN_ARGS(SSettingsEditor) { }
		SLATE_EVENT( FSimpleDelegate, OnApplicationRestartRequired )
	SLATE_END_ARGS()

public:

	/** Destructor. */
	~SSettingsEditor();

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param InModel The view model.
	 */
	void Construct( const FArguments& InArgs, const ISettingsEditorModelRef& InModel );

public:

	// FNotifyHook interface

	virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged ) override;

protected:

	/**
	 * Checks out the default configuration file for the currently selected settings object.
	 *
	 * @return true if the check-out succeeded, false otherwise.
	 */
	bool CheckOutDefaultConfigFile();

	/**
	 * Gets the absolute path to the Default.ini for the specified object.
	 *
	 * @param SettingsObject The object to get the path for.
	 * @return The path to the file.
	 */
	FString GetDefaultConfigFilePath( const TWeakObjectPtr<UObject>& SettingsObject ) const;

	/**
	 * Gets the settings object of the selected section, if any.
	 *
	 * @return The settings object.
	 */
	TWeakObjectPtr<UObject> GetSelectedSettingsObject() const;

	/**
	 * Checks whether the default config file needs to be checked out for editing.
	 *
	 * @return true if the file needs to be checked out, false otherwise.
	 */
	bool IsDefaultConfigCheckOutNeeded() const;

	/**
	 * Creates a widget for the given settings category.
	 *
	 * @param Category The category to create the widget for.
	 * @return The created widget.
	 */
	TSharedRef<SWidget> MakeCategoryWidget( const ISettingsCategoryRef& Category );

	/**
	 * Makes the default configuration file for the currently selected settings object writable.
	 *
	 * @return true if it was made writable, false otherwise.
	 */
	bool MakeDefaultConfigFileWritable();

	/**
	 * Reports a preference changed event to the analytics system.
	 *
	 * @param SelectedSection The currently selected settings section.
	 * @param PropertyChangedEvent The event for the changed property.
	 */
	void RecordPreferenceChangedAnalytics( ISettingsSectionPtr SelectedSection, const FPropertyChangedEvent& PropertyChangedEvent ) const;

	/** Reloads the settings categories. */
	void ReloadCategories();

	/**
	 * Shows a notification pop-up.
	 *
	 * @param Text The notification text.
	 * @param CompletionState The notification's completion state, i.e. success or failure.
	 */
	void ShowNotification( const FText& Text, SNotificationItem::ECompletionState CompletionState ) const;

private:

	/** Callback for clicking the Back link. */
	FReply HandleBackButtonClicked();

	/** Returns the config file name currently being edited. */
	FString HandleCheckoutNoticeConfigFilePath() const;

	/** Reloads the configuration object. */
	void HandleCheckoutNoticeFileProbablyModifiedExternally();

	/** Callback for determining the visibility of the 'Locked' notice. */
	EVisibility HandleCheckoutNoticeVisibility() const;

	/** Callback for when the user's culture has changed. */
	void HandleCultureChanged();

	/** Callback for clicking the 'Reset to Defaults' button. */
	FReply HandleExportButtonClicked();

	/** Callback for getting the enabled state of the 'Export' button. */
	bool HandleExportButtonEnabled() const;

	/** Callback for clicking the 'Reset to Defaults' button. */
	FReply HandleImportButtonClicked();

	/** Callback for getting the enabled state of the 'Import' button. */
	bool HandleImportButtonEnabled() const;

	/** Callback for changing the selected settings section. */
	void HandleModelSelectionChanged();

	/** Callback for clicking the 'Reset to Defaults' button. */
	FReply HandleResetDefaultsButtonClicked();

	/** Callback for getting the enabled state of the 'Reset to Defaults' button. */
	bool HandleResetToDefaultsButtonEnabled() const;

	/** Callback for navigating a settings section link. */
	void HandleSectionLinkNavigate( ISettingsSectionPtr Section );

	/** Callback for getting the visibility of a section link image. */
	EVisibility HandleSectionLinkImageVisibility( ISettingsSectionPtr Section ) const;

	/** Callback for clicking the 'Set as Defaults' button. */
	FReply HandleSetAsDefaultButtonClicked();

	/** Callback for getting the enabled state of the 'Set as Defaults' button. */
	bool HandleSetAsDefaultButtonEnabled() const;

	/** Callback for getting the visibility of options that are only visible when editing something that came from a non-default object. */
	EVisibility HandleSetAsDefaultButtonVisibility() const;

	/** Callback for getting the section description text. */
	FText HandleSettingsBoxDescriptionText() const;

	/** Callback for getting the section text for the settings box. */
	FText HandleSettingsBoxTitleText() const;

	/** Callback for determining the visibility of the settings box. */
	EVisibility HandleSettingsBoxVisibility() const;

	/** Callback for the modification of categories in the settings container. */
	void HandleSettingsContainerCategoryModified( const FName& CategoryName );

	/** Callback for checking whether the settings view is enabled. */
	bool HandleSettingsViewEnabled() const;

	/** Callback for determining the visibility of the settings view. */
	EVisibility HandleSettingsViewVisibility() const;

	/** Callback when the timer has been ticked to refresh the categories latently. */
	EActiveTimerReturnType UpdateCategoriesCallback(double InCurrentTime, float InDeltaTime);

private:

	/** Holds the vertical box for settings categories. */
	TSharedPtr<SVerticalBox> CategoriesBox;

	/** Holds the overlay slot for custom widgets. */
	SOverlay::FOverlaySlot* CustomWidgetSlot;

	/** Watcher widget for the default config file (checks file status / SCC state). */
	TSharedPtr<SSettingsEditorCheckoutNotice> FileWatcherWidget;

	/** Holds the path to the directory that the last settings were exported to. */
	FString LastExportDir;

	/** Holds a pointer to the view model. */
	ISettingsEditorModelPtr Model;

	/** Holds the settings container. */
	ISettingsContainerPtr SettingsContainer;

	/** Holds the details view. */
	TSharedPtr<IDetailsView> SettingsView;

	/** Delegate called when this settings editor requests that the user be notified that the application needs to be restarted for some setting changes to take effect */
	FSimpleDelegate OnApplicationRestartRequiredDelegate;

	/** Is the active timer registered to refresh categories after the settings changed. */
	bool bIsActiveTimerRegistered;
};
