// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TaskDatabaseDefs.h"


#if WITH_TESTTRACK

class ttsoapcgi;

/**
 * Implements the task database provider for TestTrack Pro
 */
class FTestTrackProvider
	: public FTaskDatabaseProviderInterface
{
public:

	/**
	 * Static: Creates a new instance of a TestTrack task database object
	 *
	 * @param	URL for the database server
	 * @return	The new database if successful, otherwise NULL
	 */
	static FTestTrackProvider* CreateTestTrackProvider();

public:

	/**
	 * Queries the server for a list of databases that the user has access to.  This can be called before
	 * the user is logged into the server
	 *
	 * @param	InServerURL			The server URL address
	 * @param	InUserName			User name string
	 * @param	InPassword			Password string
	 * @param	OutDatabaseNames	List of available database names
	 * @return	True if successful
	 */
	virtual bool QueryAvailableDatabases( const FString& InServerURL, const FString& InUserName, const FString& InPassword, TArray< FString >& OutDatabaseNames );

	/**
	 * Attempts to connect and login to the specified database
	 *
	 * @param	InServerURL				The server URL address
	 * @param	InUserName				User name string
	 * @param	InPassword				Password string
	 * @param	InDatabaseName			Name of the database to connect to
	 * @param	OutUserRealName			[Out] The real name of our user
	 * @param	OutResolutionValues		[Out] List of valid fix resolution values
	 * @param	OutOpenTaskStatusPrefix	[Out] Name of task status for 'Open' tasks
	 * @return	True if successful
	 */
	virtual bool ConnectToDatabase( const FString& InServerURL, const FString& InUserName, const FString& InPassword, const FString& InDatabaseName, FString& OutUserRealName, TArray< FString >& OutResolutionValues, FString& OutOpenTaskStatusPrefix );

	/**
	 * Logs the user off and disconnects from the database
	 *
	 * @return	True if successful
	 */
	virtual bool DisconnectFromDatabase();

	/**
	 * Retrieves a list of filter names from the database
	 *
	 * @param	OutFilterNames	List of filter names
	 * @return	Returns true if successful
	 */
	virtual bool QueryFilters( TArray< FString >& OutFilterNames );

	/**
	 * Retrieves a list of tasks from the database that matches the specified filter name
	 *
	 * @param	InFilterName	Filter name to restrict the request by
	 * @param	OutTaskList		List of downloaded tasks
	 * @return	Returns true if successful
	 */
	virtual bool QueryTasks( const FString& InFilterName, TArray< FTaskDatabaseEntry >& OutTaskList );

	/**
	 * Retrieves details about a specific task from the database
	 *
	 * @param	InNumber		Task number
	 * @param	OutDetails		[Out] Details for the requested task
	 * @return	Returns true if successful
	 */
	virtual bool QueryTaskDetails( const uint32 InNumber, FTaskDatabaseEntryDetails& OutDetails );

	/**
	 * Marks the specified task as complete
	 *
	 * @param	InNumber		Task number
	 * @param	InResolutionData	Resolution data for this task
	 * @return	Returns true if successful
	 */
	virtual bool MarkTaskComplete( const uint32 InNumber, const FTaskResolutionData& InResolutionData );

	/**
	 * Returns a detailed error message for the last reported error.  This message may be displayed to the user.
	 *
	 * @return	Detailed error message
	 */
	virtual const FString& GetLastErrorMessage() const
	{
		return LastErrorMessage;
	}

	/**
	 * Returns true if the last error was caused by a disconnection from the server
	 *
	 * @return	True if the last error was a disconnection error
	 */
	virtual const bool LastErrorWasCausedByDisconnection() const
	{
		return bLastErrorWasCausedByDisconnection;
	}

protected:

	/**
	 * FTestTrackProvider constructor
	 */
	FTestTrackProvider();

public:

	/**
	 * FTestTrackProvider destructor
	 */
	virtual ~FTestTrackProvider();

protected:

	/**
	 * Initializes this task database
	 *
	 * @return	true if successful
	 */
	bool Init();

	/**
	 * Sets the TestTrack server endpoint address using the specified URL string
	 *
	 * @param	InServerURL		The server URL address
	 */
	void SetupServerEndpoint( const FString& InServerURL );

	/**
	 * Checks the result code and returns whether the operation succeeded.  For failure cases, keeps track
	 * of the actual error message so it can be retrieved later.
	 *
	 * @param	ResultCode	The result code to test
	 * @return	True if the result code reported success
	 */
	bool VerifyTTPSucceeded( const int32 ResultCode );

protected:

	/** SOAP interface to TestTrack web server */
	ttsoapcgi* TestTrackSoap;

	/** Detailed error message for the last error that occured */
	FString LastErrorMessage;

	/** True if the last error was caused by a disconnection */
	bool bLastErrorWasCausedByDisconnection;

	/** Cached ANSI server endpoint URL string */
	ANSICHAR* ANSIServerEndpointURLString;

	/** Cached identity handle for the TestTrack server, generated at login-time */
	uint64 IDCookie;

	/** Cached 'real name' for the currently connected user */
	FString UserRealName;

};
#endif	// WITH_TESTTRACK
