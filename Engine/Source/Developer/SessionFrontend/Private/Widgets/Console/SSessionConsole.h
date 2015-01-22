// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the session console panel.
 *
 * This panel receives console log messages from a remote engine session and can also send
 * console commands to it.
 */
class SSessionConsole
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSessionConsole) { }
	SLATE_END_ARGS()

public:

	/** Destructor. */
	~SSessionConsole();

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InSessionManager The session manager to use.
	 */
	void Construct( const FArguments& InArgs, ISessionManagerRef InSessionManager );

protected:

	/** Binds the device commands on our toolbar. */
	void BindCommands();

	/** Clears the log list view. */
	void ClearLog();

	/** Copies the selected log messages to the clipboard. */
	void CopyLog();

	/**
	 * Reloads the log messages for the currently selected engine instances.
	 *
	 * @param FullyReload - Whether to fully reload the log entries or only re-apply filtering.
	 */
	void ReloadLog( bool FullyReload );

	/** Saves all log messages to a file. */
	void SaveLog();

	/**
	 * Sends the command entered into the input field.
	 *
	 * @param CommandString The command string to send.
	 */
	void SendCommand( const FString& CommandString );

protected:

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;

private:

	/** Callback for executing the 'Clear' action. */
	void HandleClearActionExecute();

	/** Callback for determining the 'Clear' action can execute. */
	bool HandleClearActionCanExecute();

	/** Callback for executing the 'Copy' action. */
	void HandleCopyActionExecute();

	/** Callback for determining the 'Copy' action can execute. */
	bool HandleCopyActionCanExecute();

	/** Callback for executing the 'Save' action. */
	void HandleSaveActionExecute();

	/** Callback for determining the 'Save' action can execute. */
	bool HandleSaveActionCanExecute();

	/** Callback for promoting console command to shortcuts. */
	void HandleCommandBarPromoteToShortcutClicked( const FString& CommandString );

	/** Callback for submitting console commands. */
	void HandleCommandSubmitted( const FString& CommandString );

	/** Callback for changing the filter settings. */
	void HandleFilterChanged();

	/** Callback for scrolling a log item into view. */
	void HandleLogListItemScrolledIntoView( FSessionLogMessagePtr Item, const TSharedPtr<ITableRow>& TableRow );

	/** Callback for generating a row widget for the log list view. */
	TSharedRef<ITableRow> HandleLogListGenerateRow( FSessionLogMessagePtr Message, const TSharedRef<STableViewBase>& OwnerTable );

	/** Callback for getting the highlight string for log messages. */
	FText HandleLogListGetHighlightText() const;

	/** Callback for selecting log messages. */
	void HandleLogListSelectionChanged( FSessionLogMessagePtr InItem, ESelectInfo::Type SelectInfo );

	/** Callback for getting the enabled state of the console box. */
	bool HandleMainContentIsEnabled() const;

	/** Callback for determining the visibility of the 'Select a session' overlay. */
	EVisibility HandleSelectSessionOverlayVisibility() const;

	/** Callback for changing the engine instance selection. */
	void HandleSessionManagerInstanceSelectionChanged();

	/** Callback for received log entries. */
	void HandleSessionManagerLogReceived( const ISessionInfoRef& Session, const ISessionInstanceInfoRef& Instance, const FSessionLogMessageRef& Message );

	/** Callback for changing the selected session. */
	void HandleSessionManagerSelectedSessionChanged( const ISessionInfoPtr& SelectedSession );

private:

	/** Holds an unfiltered list of available log messages. */
	TArray<FSessionLogMessagePtr> AvailableLogs;

	/** Holds the command bar. */
	TSharedPtr<SSessionConsoleCommandBar> CommandBar;

	/** Holds the filter bar. */
	TSharedPtr<SSessionConsoleFilterBar> FilterBar;

	/** Holds the find bar. */
	TSharedPtr<SSearchBox> FindBar;

	/** Holds the highlight text. */
	FString HighlightText;

	/** Holds the directory where the log file was last saved to. */
	FString LastLogFileSaveDirectory;

	/** Holds the log list view. */
 	TSharedPtr<SListView<FSessionLogMessagePtr>> LogListView;

 	/** Holds the filtered list of log messages. */
 	TArray<FSessionLogMessagePtr> LogMessages;

	/** Holds the session manager. */
	ISessionManagerPtr SessionManager;

	/** Holds the shortcut window. */
	TSharedPtr<SSessionConsoleShortcutWindow> ShortcutWindow;

	/** Holds a flag indicating whether the log list should auto-scroll to the last item. */
	bool ShouldScrollToLast;

	/** The command list for controlling the device */
	TSharedPtr<FUICommandList> UICommandList;
};
