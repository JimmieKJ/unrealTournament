// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TaskDatabase.h: Implements support for communicating with a task database
=============================================================================*/

#pragma once

#include "TaskDatabaseDefs.h"

/**
 * ETaskDatabaseRequestType enumerates the possible task database requests
 */
namespace ETaskDatabaseRequestType
{
	enum Type
	{
		/** No request, or invalid request */
		None = 0,

		/** Query available databases */
		QueryAvailableDatabases,

		/** Connect to the server and log in */
		ConnectToDatabase,

		/** Log the user out and disconnect from the database */
		DisconnectFromDatabase,

		/** Query list of filters from the database */
		QueryFilters,

		/** Query tasks from the database */
		QueryTasks,

		/** Query details about a specific task */
		QueryTaskDetails,

		/** Mark task as complete (e.g. fix bug) */
		MarkTaskComplete,
	};
}


/**
 * Generic response to a task database request
 */
class FTaskDatabaseResponse
{
public:

	/** The request type that triggered this response */
	ETaskDatabaseRequestType::Type RequestType;

	/** True if the request completed successfully */
	bool bSucceeded;

	/** True if we failed and it was because of a lost connection to the server */
	bool bFailedBecauseOfDisconnection;

	/** Detailed error message, in the case of failure */
	FString ErrorMessage;
};


/**
 * Response to 'ConnectToDatabase' task database request
 */
class FTaskDatabaseResponse_ConnectToDatabase
	: public FTaskDatabaseResponse
{
public:

	/** The connected user's real name */
	FString UserRealName;

	/** List of valid bug 'resolution' string values */
	TArray<FString> ResolutionValues;

	/** Status string name for 'Open' tasks (*not* completed) */
	FString OpenTaskStatusPrefix;
};


/**
 * Response to 'QueryAvailableDatabases' task database request
 */
class FTaskDatabaseResponse_QueryAvailableDatabases
	: public FTaskDatabaseResponse
{
public:

	/** Array of available database names */
	TArray<FString> DatabaseNames;
};


/**
 * Response to 'QueryFilters' task database request
 */
class FTaskDatabaseResponse_QueryFilters
	: public FTaskDatabaseResponse
{
public:

	/** Array of filter names */
	TArray<FString> FilterNames;
};


/**
 * Response to 'QueryTasks' task database request
 */
class FTaskDatabaseResponse_QueryTasks
	: public FTaskDatabaseResponse
{
public:

	/** Array of task entries that match the query */
	TArray<FTaskDatabaseEntry> TaskEntries;
};


/**
 * Response to 'QueryTaskDetails' task database request
 */
class FTaskDatabaseResponse_QueryTaskDetails
	: public FTaskDatabaseResponse
{
public:

	/** Details about the requested task */
	FTaskDatabaseEntryDetails TaskEntryDetails;
};


/**
 * Task database system, public API
 */
namespace TaskDatabaseSystem
{
	/**
	 * Initialize the task database system.  This must be called before anything else.
	 *
	 * @return	True if successful
	 */
	bool Init();

	/** Clean up the task database system.  Call this after before exiting the app. */
	void Destroy();

	/**
	 * Returns true if the task database system was initialized successfully
	 *
	 * @return	True if initialized
	 */
	bool IsInitialized();

	/**
	 * Returns true if we're currently waiting for a response from the server
	 *
	 * @return	True if a request is in progress
	 */
	bool IsRequestInProgress();

	/**
	 * Returns true if we're currently connected to a database
	 *
	 * @return	True if currently connected
	 */
	bool IsConnected();

	/** Updates the task database and checks for any completed requests, firing callbacks if needed.  This
	    should usually be called with every engine Tick. */
	void Update();

	/**
	 * Adds the specified object as a 'listener' for completed task database requests.  Remember to remove
	 * the object using UnregisterListener() before it's deleted!
	 *
	 * @param	Listener	The object to register.  Should not be NULL.
	 */
	void RegisterListener( FTaskDatabaseListener* Listener );

	/**
	 * Removes the specified object from the list of 'listeners'
	 *
	 * @param	Listener	The object to unregister.  Should have been previously registered.
	 */
	void UnregisterListener( FTaskDatabaseListener* Listener );

	/**
	 * Initiates asynchronous query for available task databases
	 *
	 * @param	InServerName		Server host name
	 * @param	InServerPort		Server port
	 * @param	InLoginUserName		User name
	 * @param	InLoginPassword		Password
	 * @return	True if async requested was kicked off successfully
	 */
	bool QueryAvailableDatabases_Async( const FString& InServerName, const uint32 InServerPort, const FString& InLoginUserName, const FString& InLoginPassword );

	/**
	 * Asynchronously connects to the task database and attempts to login to a specific database
	 *
	 * @param	InServerName		Server host name
	 * @param	InServerPort		Server port
	 * @param	InLoginUserName		User name
	 * @param	InLoginPassword		Password
	 * @param	InDatabaeName		The database to login to
	 * @return	True if async requested was kicked off successfully
	 */
	bool ConnectToDatabase_Async( const FString& InServerName, const uint32 InServerPort, const FString& InLoginUserName, const FString& InLoginPassword, const FString& InDatabaseName );

	/**
	 * Disconnects from the database asynchronously
	 *
	 * @return	True if async requested was kicked off successfully
	 */
	bool DisconnectFromDatabase_Async();

	/**
	 * Asynchronously queries a list of available database filters
	 *
	 * @return	True if async requested was kicked off successfully
	 */
	bool QueryFilters_Async();

	/**
	 * Asynchronously queries a list of task entries for the specified filter name
	 *
	 * @param	InFilterName		Name of the filter (can be empty string)
	 * @return	True if async requested was kicked off successfully
	 */
	bool QueryTasks_Async( const FString& InFilterName );

	/**
	 * Asynchronously queries details about a single specific task entry
	 *
	 * @param	InTaskNumber		The database task number for the task entry
	 * @return	True if async requested was kicked off successfully
	 */
	bool QueryTaskDetails_Async( const uint32 InTaskNumber );

	/**
	 * Attempts to asynchronously mark the specified task as completed
	 *
	 * @param	InTaskNumber		The database task number to mark complete
	 * @param	InResolutionData	Information about how the task should be resolved
	 * @return	True if async requested was kicked off successfully
	 */
	bool MarkTaskComplete_Async( const uint32 InTaskNumber, const FTaskResolutionData& InResolutionData );
};
