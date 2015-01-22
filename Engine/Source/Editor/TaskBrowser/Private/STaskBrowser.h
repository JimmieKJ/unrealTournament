// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TaskDataManager.h"


//////////////////////////////////////////////////////////////////////////
// EField
enum EField
{
	/** Invalid task index */
	Invalid = 0,

	/** The task number that usually identifies this task in the actual database */
	Number,

	/** Priority of the task */
	Priority,

	/** Name of the task (quick summary) */
	Summary,

	/** Status of this task  */
	Status,

	/** Who created the task */
	CreatedBy,

	/** Who the task is currently assigned to */
	AssignedTo,

	// ...

	/** The total number of column IDs.  Must be the last entry in the enum! */
	NumColumnIDs
};


//////////////////////////////////////////////////////////////////////////
// FTaskOverview
class FTaskOverview : public FTaskDatabaseEntry
{
public:
	/** Default constructors */
	FTaskOverview( const FTaskDatabaseEntry& InEntry );

	/** Helper func to retrieve the field entry */
	FString GetFieldEntry( const FName InName ) const;

	/** Helper func for retrieving the names of the fields */
	static FName GetFieldName( const EField InField );

	/** Helper func to fill our FieldNames map if it isn't already */
	static void PopulateFieldNames();

private:
	/** Helper map of field names and their enums */
	static TMap< EField, FName > FieldNames;
};


//////////////////////////////////////////////////////////////////////////
// FTaskBrowserSettings
class FTaskBrowserSettings
{
public:

	/** -=-=-=-=-=-=- Settings dialog -=-=-=-=-=-=-=- */

	/** Server name */
	FString ServerName;

	/** Server port */
	int32 ServerPort;

	/** Login user name */
	FString UserName;

	/** Login password */
	FString Password;

	/** Project name */
	FString ProjectName;

	/** True if we should automatically connect to the server at startup */
	bool bAutoConnectAtStartup;

	/** True if we should use single sign on */
	bool bUseSingleSignOn;

	/** -=-=-=-=-=-=- Stored interface settings -=-=-=-=-=-=-=- */

	/** Default database filter name (not localized) */
	FString DBFilterName;

	/** Display filter for 'open tasks' */
	bool bFilterOnlyOpen;

	/** Display filter for 'assigned to me' */
	bool bFilterAssignedToMe;

	/** Display filter for 'created by me' */
	bool bFilterCreatedByMe;

	/** Display filter for 'current map' */
	bool bFilterCurrentMap;

	/** The column name to sort the task list by */
	int32 TaskListSortColumn;

	/** Denotes task list ascending/descending sort order */
	bool bTaskListSortAscending;

public:

	/** FTaskBrowserSettings constructor */
	FTaskBrowserSettings();

	/** Loads settings from the configuration file */
	void LoadSettings();

	/** Saves settings to the configuration file */
	void SaveSettings();

	/** Returns true if the current connection settings are valid */
	bool AreConnectionSettingsValid() const;
};


//////////////////////////////////////////////////////////////////////////
// STaskBrowser

class STaskBrowser : public SCompoundWidget, public FTaskDataGUIInterface
{
public:
	SLATE_BEGIN_ARGS( STaskBrowser )
	{
	}

	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 *
	 * @param	InArgs			A declaration from which to construct the widget
	 */
	void Construct(const FArguments& InArgs);

	/** Gets the widget contents of the app */
	virtual TSharedRef<SWidget> GetContent();

	virtual ~STaskBrowser();

	FReply OnTaskColumnClicked( const EField InField );

private:

	/** Return whether or not the Mark Complete button should be active */
	bool GetMarkCompleteEnabled() const;

	/** Handle when the Mark Complete button is pressed */
	FReply OnMarkCompleteClicked();

	/** Handle when the Refresh View button is pressed */
	FReply OnRefreshViewClicked();

	/** Handle when the Database Filter dropdown has changed */
	void OnDatabaseFilterSelect( TSharedPtr<FString> InFilter, ESelectInfo::Type SelectInfo );

	/** Handle when the Open Only flag is toggled */
	void OnOpenOnlyChanged( const ECheckBoxState NewCheckedState );

	/** Handle when the Assigned To Me flag is toggled */
	void OnAssignedToMeChanged( const ECheckBoxState NewCheckedState );

	/** Handle when the Created By Me flag is toggled */
	void OnCreatedByMeChanged( const ECheckBoxState NewCheckedState );

	/** Handle when the Current Map flag is toggled */
	void OnCurrentMapChanged( const ECheckBoxState NewCheckedState );

	/** Create a widget to display a row of our table correctly */
	TSharedRef<ITableRow> OnTaskGenerateRow( TSharedPtr<FTaskOverview> InTaskOverview, const TSharedRef<STableViewBase>& OwnerTable );

	/** Handle when the task in the table has changed */
	void OnTaskSelectionChanged( TSharedPtr<FTaskOverview> InTaskOverview, ESelectInfo::Type SelectInfo );

	/** Handle when the task in the table has been opened */
	void OnTaskDoubleClicked( TSharedPtr<FTaskOverview> InTaskOverview );

	/** Return whether or not the Connect button should be active */
	bool GetConnectEnabled() const;

	/** Handle when the Refresh View button is pressed */
	FReply OnConnectClicked();

	/** Return the connect button text based on the state*/
	FText GetConnectText() const;

	/** Handle when the Refresh View button is pressed */
	FReply OnSettingsClicked();

	/** Return the status text based on the state*/
	FText GetStatusText() const;

	/** Spawn our task complete modal dialog */
	void CompleteTaskDialog( TArray< uint32 >& TaskNumbersToFix );

	/** Spawn our settings modal dialog */
	void SettingsDialog( void );

	/** Generates a list of currently selected task numbers */
	void QuerySelectedTaskNumbers( TArray< uint32 >& OutSelectedTaskNumbers, const bool bOnlyOpenTasks ) const;

	/** Returns the number of selected task numbers */
	int32 GetNumSelectedTasks( const bool bOnlyOpenTasks ) const;

	/** Sort the TaskList's entries */
	bool TaskListItemSort( const TSharedPtr<FTaskOverview> TaskEntryA, const TSharedPtr<FTaskOverview> TaskEntryB ) const;
	void TaskListItemSort();

	/** Refresh all or part of the user interface */
	void RefreshGUI( const ETaskBrowserGUIRefreshOptions::Type Options );

	/** FTaskDataGUIInterface Callback: Called when the GUI should be refreshed */
	virtual void Callback_RefreshGUI( const ETaskBrowserGUIRefreshOptions::Type Options );

	/** Task data manager object */
	TSharedPtr<FTaskDataManager> TaskDataManager;

	/** The column currently being used for sorting */
	EField TaskListSortColumn;

	/** Denotes ascending/descending sort order */
	bool TaskListSortAscending;

	/** The default column width */
	float TaskListColumnWidth;

	/** The number of selected open tasks */
	int32 TaskListNumSelectedAndOpen;

	/** The Mark Complete button */
	TSharedPtr<SButton> MarkComplete;

	/** The Refresh View button */
	TSharedPtr<SButton> RefreshView;

	/** The Database Filter dropdown */
	TSharedPtr<STextComboBox> DatabaseFilter;

	/** The Database Filter options */
	TArray< TSharedPtr<FString> > DatabaseOptions;

	/** The Open Only checkbox */
	TSharedPtr<SCheckBox> OpenOnly;

	/** The Assigned To Me checkbox */
	TSharedPtr<SCheckBox> AssignedToMe;

	/** The Created By Me checkbox */
	TSharedPtr<SCheckBox> CreatedByMe;

	/** The Current Map checkbox */
	TSharedPtr<SCheckBox> CurrentMap;

	/** The Task List table */
	TSharedPtr<SListView< TSharedPtr<FTaskOverview> >> TaskList;

	/** The Task List table headers */
	TSharedPtr<SHeaderRow> TaskHeaders;

	/** The array of all the tasks we can display in the table */
	TArray< TSharedPtr<FTaskOverview> > TaskEntries;

	/** The Summary of the active task */
	TSharedPtr<SEditableTextBox> Summary;

	/** The Description of the active task */
	TSharedPtr<SEditableTextBox> Description;

	/** The Connect button */
	TSharedPtr<SButton> Connect;

	/** The Settings button */
	TSharedPtr<SButton> Settings;
};