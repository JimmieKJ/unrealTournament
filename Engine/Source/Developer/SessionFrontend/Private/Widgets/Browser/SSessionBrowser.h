// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements a Slate widget for browsing active game sessions.
 */
class SSessionBrowser
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSessionBrowser) { }
	SLATE_END_ARGS()

public:

	/** Destructor. */
	~SSessionBrowser();

public:

	/**
	 * Construct this widget.
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InSessionManager The session manager to use.
	 */
	void Construct( const FArguments& InArgs, ISessionManagerRef InSessionManager );

protected:

	/** Filters the session list. */
	void FilterSessions();

	/**
	 * Gets the display name of the specified session.
	 *
	 * @param SessionInfo The session to get the name for.
	 * @param Session name.
	 */
	FText GetSessionName( const ISessionInfoPtr& SessionInfo ) const;

	/** Reloads the sessions list. */
	void ReloadSessions();

private:

	/** Callback for generating a row in the session combo box. */
	TSharedRef<SWidget> HandleSessionComboBoxGenerateWidget( ISessionInfoPtr SessionInfo ) const;

	/** Callback for selection changes in the session combo box. */
	void HandleSessionComboBoxSelectionChanged( ISessionInfoPtr SelectedItem, ESelectInfo::Type SelectInfo );

	/** Callback for getting the text in the session combo box. */
	FText HandleSessionComboBoxText() const;

	/** Callback for getting the text of the session details area. */
	FText HandleSessionDetailsText() const;

	/** Callback for changing the selected session in the session manager. */
	void HandleSessionManagerSelectedSessionChanged( const ISessionInfoPtr& SelectedSession );

	/** Callback for updating the session list in the session manager. */
	void HandleSessionManagerSessionsUpdated();

	/** Callback for clicking the 'Terminate Session' button. */
	FReply HandleTerminateSessionButtonClicked();

	/** Callback for getting the enabled state of the 'Terminate Session' button. */
	bool HandleTerminateSessionButtonIsEnabled() const;

private:

	/** Holds an unfiltered list of available sessions. */
	TArray<ISessionInfoPtr> AvailableSessions;

	/** Holds the command bar. */
	TSharedPtr<SSessionBrowserCommandBar> CommandBar;

	/** Holds the instance list view. */
	TSharedPtr<SSessionInstanceList> InstanceListView;

	/** Holds the session combo box. */
	TSharedPtr<SComboBox<ISessionInfoPtr>> SessionComboBox;

	/** Holds the filtered list of sessions to be displayed. */
	TArray<ISessionInfoPtr> SessionList;

	/** Holds a reference to the session manager. */
	ISessionManagerPtr SessionManager;

private:

	/** Callback for creating a row widget in the session list view. */
	TSharedRef<ITableRow> HandleSessionListViewGenerateRow( ISessionInfoPtr Item, const TSharedRef<STableViewBase>& OwnerTable );

	/** Handles changing the selection in the session tree. */
	void HandleSessionListViewSelectionChanged( ISessionInfoPtr Item, ESelectInfo::Type SelectInfo );

	/** Holds the session list view. */
	TSharedPtr<SListView<ISessionInfoPtr>> SessionListView;
};
