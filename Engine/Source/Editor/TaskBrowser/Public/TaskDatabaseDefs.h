// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


DECLARE_LOG_CATEGORY_EXTERN(LogTaskDatabaseDefs, Log, All);

class FTaskDatabaseResponse;


/** Define LOG_TASK_DATABASE to 1 to enable debug logging throughout the task database system */
#define LOG_TASK_DATABASE 0

#if LOG_TASK_DATABASE
	#define TDLOG( Text, ... ) UE_LOG(LogTaskDatabaseDefs, Log, TEXT( "TaskDatabase|  " ) Text, ##__VA_ARGS__ )
#else
	#define TDLOG( Text, ... )
#endif


/**
 * Base information about a single task in the database.
 */
class FTaskDatabaseEntryBase
{
public:

	/** Task number */
	uint32 Number;

	/** Priority */
	FString Priority;

	/** Name of this task (quick summary) */
	FString Summary;

	/** Status (Open, Closed, etc.) */
	FString Status;

	/** Who entered this task */
	FString CreatedBy;

public:

	/**
	 * Default constructor.
	 */
	FTaskDatabaseEntryBase()
		: Number( 0 ),
		Priority(),
		Summary(),
		Status(),
		CreatedBy()
	{ }
};


/**
 * Information about a single task in the database.
 */
class FTaskDatabaseEntry : public FTaskDatabaseEntryBase
{
public:

	/** Assigned To */
	FString AssignedTo;

public:

	/**
	 * Default constructor.
	 */
	FTaskDatabaseEntry()
		: FTaskDatabaseEntryBase(),
		  AssignedTo()
	{ }
};


/**
 * Details about a task entry.
 */
class FTaskDatabaseEntryDetails : public FTaskDatabaseEntryBase
{
public:

	/** Description */
	FString Description;

public:

	/**
	 * Default constructor.
	 */
	FTaskDatabaseEntryDetails()
		: FTaskDatabaseEntryBase(),
		  Description()
	{ }
};


/**
 * Contains the user-entered data needed to resolve a task
 */
class FTaskResolutionData
{
public:

	/** Resolution type */
	FString ResolutionType;

	/** Comments */
	FString Comments;

	/** Changelist number */
	uint32 ChangelistNumber;

	/** Hours to complete */
	double HoursToComplete;

public:

	/**
	 * Default constructor.
	 */
	FTaskResolutionData()
		: ResolutionType(),
		  Comments(),
		  ChangelistNumber( 0 ),
		  HoursToComplete( -1.0 )		  
	{ }

	bool IsValid()
	{
		if( ( ResolutionType.Len() == 0 ) ||
			( Comments.Len() == 0 ) ||
			( ChangelistNumber == 0 ) ||
			( HoursToComplete == -1.0 ) )
		{
			return false;
		}
		return true;
	}
};


/**
 * Callback interface for objects that 'listen' to task manager events
 */
class FTaskDatabaseListener
{
public:

	/** Called when a response is received from the task database.  This is a polymorphic object that should
		be cast to the appropriate response type for the request. */
	virtual void OnTaskDatabaseRequestCompleted( const FTaskDatabaseResponse* InGenericResponse ) = 0;
};


/**
 * Interface class for implementing a task database provider
 */
class FTaskDatabaseProviderInterface
{
public:

	/** FTaskDatabaseProviderInterface destructor */
	virtual ~FTaskDatabaseProviderInterface() {}

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
	virtual bool QueryAvailableDatabases( const FString& InServerURL, const FString& InUserName, const FString& InPassword, TArray< FString >& OutDatabaseNames ) = 0;

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
	virtual bool ConnectToDatabase( const FString& InServerURL, const FString& InUserName, const FString& InPassword, const FString& InDatabaseName, FString& OutUserRealName, TArray< FString >& OutResolutionValues, FString& OutOpenTaskStatusPrefix ) = 0;

	/**
	 * Logs the user off and disconnects from the database
	 *
	 * @return	True if successful
	 */
	virtual bool DisconnectFromDatabase() = 0;

	/**
	 * Retrieves a list of filter names from the database
	 *
	 * @param	OutFilterNames	List of filter names
	 * @return	Returns true if successful
	 */
	virtual bool QueryFilters( TArray< FString >& OutFilterNames ) = 0;

	/**
	 * Retrieves a list of tasks from the database that matches the specified filter name
	 *
	 * @param	InFilterName	Filter name to restrict the request by
	 * @param	OutTaskList		List of downloaded tasks
	 * @return	Returns true if successful
	 */
	virtual bool QueryTasks( const FString& InFilterName, TArray< FTaskDatabaseEntry >& OutTaskList ) = 0;

	/**
	 * Retrieves details about a specific task from the database
	 *
	 * @param	InNumber		Task number
	 * @param	OutDetails		[Out] Details for the requested task
	 * @return	Returns true if successful
	 */
	virtual bool QueryTaskDetails( const uint32 InNumber, FTaskDatabaseEntryDetails& OutDetails ) = 0;

	/**
	 * Marks the specified task as complete
	 *
	 * @param	InNumber			Task number
	 * @param	InResolutionData	Resolution data for this task
	 * @return	Returns true if successful
	 */
	virtual bool MarkTaskComplete( const uint32 InNumber, const FTaskResolutionData& InResolutionData ) = 0;

	/**
	 * Returns a detailed error message for the last reported error.  This message may be displayed to the user.
	 *
	 * @return	Detailed error message
	 */
	virtual const FString& GetLastErrorMessage() const = 0;

	/**
	 * Returns true if the last error was caused by a disconnection from the server
	 *
	 * @return	True if the last error was a disconnection error
	 */
	virtual const bool LastErrorWasCausedByDisconnection() const = 0;
};
