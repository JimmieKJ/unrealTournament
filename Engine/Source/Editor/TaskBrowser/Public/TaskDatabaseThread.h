// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TaskDatabaseThread.h: Implements logic for task database worker thread
=============================================================================*/

#pragma once

#include "TaskDatabase.h"


/**
 * Base class for describing an in-flight task database request
 */
class FTaskDatabaseRequest
{
public:

	/** The request type */
	ETaskDatabaseRequestType::Type RequestType;
};


/**
 * Task database request data for 'QueryAvailableDatabases'
 */
class FTaskDatabaseRequest_QueryAvailableDatabases
	: public FTaskDatabaseRequest
{
public:

	/** Server URL */
	FString ServerURL;

	/** Login user name */
	FString LoginUserName;

	/** Login password */
	FString LoginPassword;
};


/**
 * Task database request data for 'ConnectToDatabase'
 */
class FTaskDatabaseRequest_ConnectToDatabase
	: public FTaskDatabaseRequest
{
public:

	/** Server URL */
	FString ServerURL;

	/** Login user name */
	FString LoginUserName;

	/** Login password */
	FString LoginPassword;

	/** The database name to connect to */
	FString DatabaseName;
};


/**
 * Task database request data for 'QueryTasks'
 */
class FTaskDatabaseRequest_QueryTasks
	: public FTaskDatabaseRequest
{
public:

	/** Filter name string */
	FString FilterName;
};


/**
 * Task database request data for 'QueryTaskDetails'
 */
class FTaskDatabaseRequest_QueryTaskDetails
	: public FTaskDatabaseRequest
{
public:

	/** The task number to query details about */
	uint32 TaskNumber;
};


/**
 * Task database request data for 'MarkTaskComplete'
 */
class FTaskDatabaseRequest_MarkTaskComplete
	: public FTaskDatabaseRequest
{
public:

	/** The task number to mark as fixed */
	uint32 TaskNumber;

	/** Resolution data for this task */
	FTaskResolutionData ResolutionData;
};


/**
 * Task database thread 'runnable' instance
 */
class FTaskDatabaseThreadRunnable
	: public FRunnable
{
public:

	/**
	 * FTaskDatabaseThreadRunnable constructor
	 *
	 * @param	InTaskDatabaseProvider	The task database provider to bind to
	 */
	FTaskDatabaseThreadRunnable( FTaskDatabaseProviderInterface* InTaskDatabaseProvider );

	/** FTaskDatabaseThreadRunnable destructor */
	virtual ~FTaskDatabaseThreadRunnable();

	/**
	 * Allows per runnable object initialization. NOTE: This is called in the
	 * context of the thread object that aggregates this, not the thread that
	 * passes this runnable to a new thread.
	 *
	 * @return True if initialization was successful, false otherwise
	 */
	virtual bool Init();

	/**
	 * This is where all per object thread work is done. This is only called
	 * if the initialization was successful.
	 *
	 * @return The exit code of the runnable object
	 */
	virtual uint32 Run();

	/**
	 * This is called if a thread is requested to terminate early
	 */
	virtual void Stop();

	/**
	 * Called in the context of the aggregating thread to perform any cleanup.
	 */
	virtual void Exit();

public:

	/** Synchronization object for the task database */
	FCriticalSection CriticalSection;

	/** True if we should suspend the task database thread
		NOTE: This is shared between the main thread and the task database thread */
	volatile bool bThreadSafeShouldSuspend;

	/** True if the task database thread is ready to be assigned a request
		NOTE: This is shared between the main thread and the task database thread */
	FTaskDatabaseRequest* volatile ThreadSafeCurrentRequest;

	/** Pointer to the latest response data from the prior request
		NOTE: This is shared between the main thread and the task database thread */
	FTaskDatabaseResponse* volatile ThreadSafeResponse;

private:

	/** Task database server URL */
	FString ServerURL;

	/** Instance of the actual task database provider */
	FTaskDatabaseProviderInterface* TaskDatabaseProvider;

	/** True if the task database thread has been explicitly asked to stop.  Usually at shutdown time. */
	bool bAskedToStop;
};
