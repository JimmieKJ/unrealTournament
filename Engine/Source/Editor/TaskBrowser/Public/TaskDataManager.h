// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TaskDatabaseDefs.h"


/** GUI refresh options for the task browser */
namespace ETaskBrowserGUIRefreshOptions
{
	enum Type
	{
		// Completely rebuild the filter list contents (and force a sort)
		RebuildFilterList = 1 << 0,

		// Completely rebuild the task list contents (and force a sort)
		RebuildTaskList = 1 << 1,

		// Update the window column sizes
		ResetColumnSizes = 1 << 2,

		// Sort the task list
		SortTaskList = 1 << 3,

		// Update the task description text and name label
		UpdateTaskDescription = 1 << 4,
	};
}


/**
 * Classes derived from this implement a GUI front end for the task data manager
 */
class FTaskDataGUIInterface
{
public:

	/**
	 * Called by the TaskDataManager when it wants to refresh the user interface
	 *
	 * @param	Options		Bitfield that contains the types of GUI elements to refresh
	 */
	virtual void Callback_RefreshGUI( const ETaskBrowserGUIRefreshOptions::Type Options ) = 0;
};


/** Status field for the task data manager */
namespace ETaskDataManagerStatus
{
	enum Type
	{
		/** Unknown status */
		Unknown,

		/** Failed to initialize (no task database providers?) */
		FailedToInit,
		
		/** Ready to attempt a connection (waiting for user to initiate it) */
		ReadyToConnect,

		/** Connecting to server and logging in */
		Connecting,

		/** Connected successfully */
		Connected,

		/** Connection failed */
		ConnectionFailed,

		/** Disconnecting */
		Disconnecting,
		
		// ...

		/** Querying filters */
		QueryingFilters,

		/** Querying tasks */
		QueryingTasks,

		/** Querying task details */
		QueryingTaskDetails,

		/** Marking task as fixed */
		MarkingTaskComplete,
	};
}


/**
 * Connection settings and credentials for logging into the task database
 */
class FTaskDataManagerConnectionSettings
{
public:

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

public:

	/** FTaskDataManagerConnectionSettings constructor */
	FTaskDataManagerConnectionSettings()
		: ServerName(),
		  ServerPort( 80 ),
		  UserName(),
		  Password(),
		  ProjectName()
	{ }
};


/**
 * Data management layer between task GUI system and database connection
 */
class FTaskDataManager
	: public FTaskDatabaseListener,		// Listens for task database responses
	  public FTickableEditorObject			// Tickable, so we can update the task database
{
public:

	/**
	 * FTaskDataManager constructor
	 *
	 * @param	InGUICallbackObject		Object that we'll call back to for GUI interop
	 */
	FTaskDataManager( FTaskDataGUIInterface* InGUICallbackObject );

	/**
	 * Virtual destructor.
	 */
	virtual ~FTaskDataManager();

public:

	/**
	 * Used to determine whether an object is ready to be ticked. This is 
	 * required for example for all UObject derived classes as they might be
	 * loaded async and therefore won't be ready immediately.
	 *
	 * @return	true if class is ready to be ticked, false otherwise.
	 */
	virtual bool IsTickable() const
	{
		return true;
	}

	/**
	 * Called from within LevelTick.cpp after ticking all actors or from
	 * the rendering thread (depending on bIsRenderingThreadObject)
	 *
	 * @param DeltaTime	Game time passed since the last call.
	 */
	virtual void Tick( float DeltaTime );

	virtual TStatId GetStatId() const override;

	/**
	 * Sets the connection settings used to connect and login to the task database server
	 *
	 * @param	InConnectionSettings	The new connection settings
	 */
	void SetConnectionSettings( const FTaskDataManagerConnectionSettings& InConnectionSettings )
	{
		ConnectionSettings = InConnectionSettings;
	}

	/**
	 * Returns the user's real name (retrieved from the server, if available)
	 *
	 * @return	User's real name string
	 */
	const FString& GetUserRealName() const
	{
		return UserRealName;
	}


	/**
	 * Returns list of allowed task resolution options
	 *
	 * @return	List of allowed resolutions
	 */
	const TArray< FString >& GetResolutionValues() const
	{
		return ResolutionValues;
	}

	/**
	 * Returns the string name for 'Open' task status
	 *
	 * @return	Open task status name
	 */
	const FString& GetOpenTaskStatusPrefix() const
	{
		return OpenTaskStatusPrefix;
	}

	/**
	 * Returns the current connection status of the task data manager
	 *
	 * @return	Current status
	 */
	ETaskDataManagerStatus::Type GetConnectionStatus() const
	{
		return ConnectionStatus;
	}

	/**
	 * Returns the current status to display in the GUI
	 *
	 * @return	Current GUI status
	 */
	ETaskDataManagerStatus::Type GetGUIStatus() const
	{
		// Default to reporting connection status
		ETaskDataManagerStatus::Type GUIStatus = ConnectionStatus;
		
		// If we're connected and we have something more interesting to display, then report that instead
		if( GUIStatus == ETaskDataManagerStatus::Connected && QueryStatus != ETaskDataManagerStatus::Unknown )
		{
			GUIStatus = QueryStatus;
		}

		return GUIStatus;
	}

	/**
	 * Changes the active filter
	 *
	 * @param	InFilterName	The new filter to use
	 */
	void ChangeActiveFilter( const FString& InFilterName )
	{
		if( ActiveFilterName != InFilterName )
		{
			// OK, we're changing filters!
			ActiveFilterName = InFilterName;
		}
	}

	/**
	 * Returns the active filter name
	 *
	 * @return	The current active filter name
	 */
	const FString& GetActiveFilterName() const
	{
		return ActiveFilterName;
	}

	/**
	 * Sets the 'focused task number' (the task we should retrieve details about)
	 *
	 * @param	InTaskNumber	The task number to keep track of (or INDEX_NONE for 'none')
	 */
	void SetFocusedTaskNumber( const uint32 InTaskNumber )
	{
		if( FocusedTaskNumber != InTaskNumber )
		{
			// We have a new focused task!
			FocusedTaskNumber = InTaskNumber;
		}
	}

	/**
	 * Returns the current focused task number
	 *
	 * @return	The current focused task number
	 */
	uint32 GetFocusedTaskNumber() const
	{
		return FocusedTaskNumber;
	}

	/**
	 * Starts process of marking the specified task numbers as completed
	 *
	 * @param	InTaskNumbers		Numbers of tasks to mark as completed
	 * @param	InResolutionData	User-entered resolution data for these tasks
	 */
	void StartMarkingTasksComplete( const TArray< uint32 >& InTaskNumbers,
									const FTaskResolutionData& InResolutionData );

	/**
	 * Returns the list of cached filter names
	 *
	 * @return	List of cached filter name strings
	 */
	const TArray< FString >& GetCachedFilterNames() const
	{
		return CachedFilterNames;
	}

	/**
	 * Returns the list of cached task entries
	 *
	 * @return	List of cached task entries
	 */
	const TArray< FTaskDatabaseEntry >& GetCachedTaskArray() const
	{
		return CachedTaskArray;
	}

	/**
	 * Searches for details about the specified task number in our cache and returns it if found
	 *
	 * @return	Returns pointer to the task details, or NULL if not found
	 */
	const FTaskDatabaseEntryDetails* FindCachedTaskDetails( const uint32 InTaskNumber ) const
	{
		return CachedTaskDetailsMap.Find( InTaskNumber );
	}

	/** Initiates or re-initiates a connection to the task database server */
	void AttemptConnection();

	/** Initiates a disconnection from the task database server */
	void AttemptDisconnection();

	/** Starts a forced a refresh of task list/description data from the server */
	void ClearTaskDataAndInitiateRefresh();

	/** Updates our state machine.  Should be called frequently (every Tick is OK) */
	void UpdateTaskDataManager();

	/** We've been disconnected (either voluntarily or otherwise), so clean up state and refresh GUI */
	void CleanUpAfterDisconnect();

	/**
	 * Called when a response is received from the task database
	 *
	 * @param	InGenericResponse	Response data for the completed request.  This is a polymorphic object
	 *								that should be casted to the appropriate response type for the request.
	 */
	virtual void OnTaskDatabaseRequestCompleted( const FTaskDatabaseResponse* InGenericResponse );

private:

	/** Callback object for GUI updates */
	FTaskDataGUIInterface* GUICallbackObject;

	/** Connection settings used to connect and login to the task database server */
	FTaskDataManagerConnectionSettings ConnectionSettings;

	/** The connected user's real name (retrieved from the server, if available) */
	FString UserRealName;

	/** List of valid task resolution strings, cached from the server */
	TArray< FString > ResolutionValues;

	/** Status name for 'Open' tasks */
	FString OpenTaskStatusPrefix;

	/** Current connection status */
	ETaskDataManagerStatus::Type ConnectionStatus;

	/** Current query status */
	ETaskDataManagerStatus::Type QueryStatus;

	// ...

	/** True if we need to refresh the list of filters from the server */
	bool bShouldRefreshFilterNames;

	/** Active database filter name.  This must be set before we can query tasks. */
	FString ActiveFilterName;

	/** Task number that we're currently interested in. */
	uint32 FocusedTaskNumber;

	/** List of task numbers that the user wants marked as complete */
	TArray< uint32 > TaskNumbersToMarkComplete;

	/** User-entered resolution data for completing tasks */
	FTaskResolutionData ResolutionData;

	/** Task number that we're currently processing through a request */
	uint32 CurrentlyProcessingTaskNumber;

	/** Array of cached filters from server */
	TArray<FString> CachedFilterNames;

	/** Array of cached tasks from server */
	TArray< FTaskDatabaseEntry > CachedTaskArray;

	/** Filter name used to retrieve the last list of tasks */
	FString CachedLastTaskArrayFilterName;

	/** Map of task number to cached task details */
	TMap<int32, FTaskDatabaseEntryDetails> CachedTaskDetailsMap;
};
