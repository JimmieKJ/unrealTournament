// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements the device details widget.
 */
class SDeviceProcesses
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDeviceProcesses) { }
	SLATE_END_ARGS()

public:

	/**
	 * Destructor.
	 */
	~SDeviceProcesses( );

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InModel The view model to use.
	 */
	void Construct( const FArguments& InArgs, const FDeviceManagerModelRef& InModel );

protected:

	/**
	 * Reloads the list of processes.
	 */
	void ReloadProcessList( bool FullyReload );

private:
	// Periodically refreshes the process list
	EActiveTimerReturnType UpdateProcessList( double InCurrentTime, float InDeltaTime );

	// Callback for getting the text of the message overlay.
	FText HandleMessageOverlayText( ) const;

	// Callback for getting the visibility of the message overlay.
	EVisibility HandleMessageOverlayVisibility( ) const;

	// Callback for handling device service selection changes.
	void HandleModelSelectedDeviceServiceChanged( );

	// Callback for changing 'Show process tree' check box state.
	void HandleProcessTreeCheckBoxCheckStateChanged( ECheckBoxState NewState );

	// Callback for determining checked state of the 'Show process tree' check box.
	ECheckBoxState HandleProcessTreeCheckBoxIsChecked( ) const;

	// Callback for generating a row widget in the process tree view.
	TSharedRef<ITableRow> HandleProcessTreeViewGenerateRow( FDeviceProcessesProcessTreeNodePtr Item, const TSharedRef<STableViewBase>& OwnerTable );

	// Callback for getting the children of a node in the process tree view.
	void HandleProcessTreeViewGetChildren( FDeviceProcessesProcessTreeNodePtr Item, TArray<FDeviceProcessesProcessTreeNodePtr>& OutChildren );

	// Callback for getting the enabled state of the processes panel.
	bool HandleProcessesBoxIsEnabled( ) const;

	// Callback for getting the enabled state of the 'Terminate Process' button.
	bool HandleTerminateProcessButtonIsEnabled( ) const;

	// Callback for clicking the 'Terminate Process' button.
	FReply HandleTerminateProcessButtonClicked( );

private:
	
	// Holds the time at which the process list was last refreshed.
	FDateTime LastProcessListRefreshTime;

	// Holds a pointer the device manager's view model.
	FDeviceManagerModelPtr Model;

	// Holds the filtered list of processes running on the device.
	TArray<FDeviceProcessesProcessTreeNodePtr> ProcessList;

	// Holds the collection of process nodes.
	TMap<uint32, FDeviceProcessesProcessTreeNodePtr> ProcessMap;

	// Holds the application tree view.
	TSharedPtr<STreeView<FDeviceProcessesProcessTreeNodePtr> > ProcessTreeView;

	// Holds the collection of processes running on the device.
	TArray<FTargetDeviceProcessInfo> RunningProcesses;

	// Holds a flag indicating whether the process list should be shown as a tree instead of a list.
	bool ShowProcessTree;
};
