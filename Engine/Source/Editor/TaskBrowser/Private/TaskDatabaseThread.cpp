// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "TaskBrowserPrivatePCH.h"
#include "TaskDatabaseThread.h"


/**
 * FTaskDatabaseThreadRunnable constructor
 *
 * @param	InTaskDatabaseProvider	The task database provider to bind to
 */
FTaskDatabaseThreadRunnable::FTaskDatabaseThreadRunnable( FTaskDatabaseProviderInterface* InTaskDatabaseProvider )
	: bThreadSafeShouldSuspend( false ),
	  ThreadSafeCurrentRequest( NULL ),
	  ThreadSafeResponse( NULL ),
	  TaskDatabaseProvider( InTaskDatabaseProvider ),
	  bAskedToStop( false )
{
	// NOTE: This code always runs on the main thread, not the task database thread
	// NOTE: This code runs after the task database thread has shut down
}


/** FTaskDatabaseThreadRunnable destructor */
FTaskDatabaseThreadRunnable::~FTaskDatabaseThreadRunnable()
{
	// NOTE: This code always runs on the main thread, not the task database thread
	// NOTE: This code runs before the the task database thread is started

	// Task database thread should have cleaned everything up by now
	check( TaskDatabaseProvider == NULL );

	// Clean up left over request/response memory if we need to
	if( ThreadSafeCurrentRequest != NULL )
	{
		delete ThreadSafeCurrentRequest;
		ThreadSafeCurrentRequest = NULL;
	}
	if( ThreadSafeResponse != NULL )
	{
		delete ThreadSafeResponse;
		ThreadSafeResponse = NULL;
	}
}


/**
 * Allows per runnable object initialization. NOTE: This is called in the
 * context of the thread object that aggregates this, not the thread that
 * passes this runnable to a new thread.
 *
 * @return True if initialization was successful, false otherwise
 */
bool FTaskDatabaseThreadRunnable::Init()
{
	// Make sure that a task database provider was assigned at construction
	check( TaskDatabaseProvider != NULL );

	return true;
}


/**
 * This is where all per object thread work is done. This is only called
 * if the initialization was successful.
 *
 * @return The exit code of the runnable object
 */
uint32 FTaskDatabaseThreadRunnable::Run()
{
	TDLOG( TEXT( "TDThread: Run() started" ) );

	while( !bAskedToStop )
	{
		const FTaskDatabaseRequest* GenericRequestCopy = NULL;
		{
			// @todo: Make sure we really need crit sects in all cases.  A lot of operations should be atomic
			FScopeLock ScopeLock( &CriticalSection );
			GenericRequestCopy = ThreadSafeCurrentRequest;
		}

		if( GenericRequestCopy != NULL )
		{
			// Polymorphic response data.  We'll allocate an event-specific instance of this.
			FTaskDatabaseResponse* GenericResponse = NULL;
			
			TDLOG( TEXT( "TDThread: Starting work on request (Type: %i)" ), ( int32 )GenericRequestCopy->RequestType );

			// We have work to do!
			switch( GenericRequestCopy->RequestType )
			{
				case ETaskDatabaseRequestType::QueryAvailableDatabases:
					{
						const FTaskDatabaseRequest_QueryAvailableDatabases* Request =
							static_cast< const FTaskDatabaseRequest_QueryAvailableDatabases* >( GenericRequestCopy );
						FTaskDatabaseResponse_QueryAvailableDatabases* Response =
							new FTaskDatabaseResponse_QueryAvailableDatabases();
						GenericResponse = Response;

						// Query available databases and store whether or not we succeeded
						GenericResponse->bSucceeded =
							TaskDatabaseProvider->QueryAvailableDatabases(
								Request->ServerURL,
								Request->LoginUserName,
								Request->LoginPassword,
								Response->DatabaseNames );	// Out
					}
					break;


				case ETaskDatabaseRequestType::ConnectToDatabase:
					{
						const FTaskDatabaseRequest_ConnectToDatabase* Request =
							static_cast< const FTaskDatabaseRequest_ConnectToDatabase* >( GenericRequestCopy );
						FTaskDatabaseResponse_ConnectToDatabase* Response =
							new FTaskDatabaseResponse_ConnectToDatabase();
						GenericResponse = Response;

						// Query available databases and store whether or not we succeeded
						GenericResponse->bSucceeded =
							TaskDatabaseProvider->ConnectToDatabase(
								Request->ServerURL,
								Request->LoginUserName,
								Request->LoginPassword,
								Request->DatabaseName,
								Response->UserRealName,				// Out
								Response->ResolutionValues,			// Out
								Response->OpenTaskStatusPrefix );	// Out
					}
					break;


				case ETaskDatabaseRequestType::DisconnectFromDatabase:
					{
						const FTaskDatabaseRequest* Request =
							static_cast< const FTaskDatabaseRequest* >( GenericRequestCopy );
						FTaskDatabaseResponse* Response = new FTaskDatabaseResponse();
						GenericResponse = Response;

						// Query available databases and store whether or not we succeeded
						GenericResponse->bSucceeded =
							TaskDatabaseProvider->DisconnectFromDatabase();
					}
					break;


				case ETaskDatabaseRequestType::QueryFilters:
					{
						const FTaskDatabaseRequest* Request =
							static_cast< const FTaskDatabaseRequest* >( GenericRequestCopy );
						FTaskDatabaseResponse_QueryFilters* Response = new FTaskDatabaseResponse_QueryFilters();
						GenericResponse = Response;

						// Query list of filters
						GenericResponse->bSucceeded =
							TaskDatabaseProvider->QueryFilters(
								Response->FilterNames );	// Out
					}
					break;


				case ETaskDatabaseRequestType::QueryTasks:
					{
						const FTaskDatabaseRequest_QueryTasks* Request =
							static_cast< const FTaskDatabaseRequest_QueryTasks* >( GenericRequestCopy );
						FTaskDatabaseResponse_QueryTasks* Response = new FTaskDatabaseResponse_QueryTasks();
						GenericResponse = Response;

						// Query list of tasks
						GenericResponse->bSucceeded =
							TaskDatabaseProvider->QueryTasks(
								Request->FilterName,
								Response->TaskEntries );	// Out
					}
					break;


				case ETaskDatabaseRequestType::QueryTaskDetails:
					{
						const FTaskDatabaseRequest_QueryTaskDetails* Request =
							static_cast< const FTaskDatabaseRequest_QueryTaskDetails* >( GenericRequestCopy );
						FTaskDatabaseResponse_QueryTaskDetails* Response = new FTaskDatabaseResponse_QueryTaskDetails();
						GenericResponse = Response;

						// Query details about a specific task
						GenericResponse->bSucceeded =
							TaskDatabaseProvider->QueryTaskDetails(
								Request->TaskNumber,
								Response->TaskEntryDetails );	// Out
					}
					break;


				case ETaskDatabaseRequestType::MarkTaskComplete:
					{
						const FTaskDatabaseRequest_MarkTaskComplete* Request =
							static_cast< const FTaskDatabaseRequest_MarkTaskComplete* >( GenericRequestCopy );
						FTaskDatabaseResponse* Response = new FTaskDatabaseResponse();
						GenericResponse = Response;

						// Mark the specified task as complete
						GenericResponse->bSucceeded =
							TaskDatabaseProvider->MarkTaskComplete(
								Request->TaskNumber,
								Request->ResolutionData );
					}
					break;


				default:
					{
						// Unknown request
						check( 0 );
					}
					break;
			}

			// There should really always be a response, otherwise it's a code error
			check( GenericResponse != NULL );
			if( GenericResponse != NULL )
			{
				if( GenericResponse->bSucceeded )
				{
					TDLOG( TEXT( "TDThread: Completed SUCCESSFUL request (Type: %i)" ), ( int32 )GenericRequestCopy->RequestType );

					GenericResponse->bFailedBecauseOfDisconnection = false;
				}
				else
				{
					TDLOG( TEXT( "TDThread: Completed FAILED request (Type: %i)" ), ( int32 )GenericRequestCopy->RequestType );

					// Set the error message string
					GenericResponse->ErrorMessage = TaskDatabaseProvider->GetLastErrorMessage();

					// Did we fail because we were disconnected from the server?
					GenericResponse->bFailedBecauseOfDisconnection =
						TaskDatabaseProvider->LastErrorWasCausedByDisconnection();

					if( GenericResponse->bFailedBecauseOfDisconnection )
					{
						TDLOG( TEXT( "TDThread: We were disconnected from the server due to this error" ) );
					}
				}

				// Store the request type in the response payload
				GenericResponse->RequestType = GenericRequestCopy->RequestType;

				// Request completed, so we'll report back to the main thread
				{
					FScopeLock ScopeLock( &CriticalSection );

					// The main thread will check this response data frequently, and before issuing
					// any new requests to us.  We simply need to set it safely.
					ThreadSafeResponse = GenericResponse;

					// We can delete the original request data now
					delete ThreadSafeCurrentRequest;
					ThreadSafeCurrentRequest = NULL;
				}
			}
		}


		if( bThreadSafeShouldSuspend )
		{
			// We've been asked to suspend, so sleep for a long time
			FPlatformProcess::Sleep( 1.0f );
		}
		else
		{
			// Give a time slice between tasks.  We're don't want to use a lot of CPU on this thread.
			FPlatformProcess::Sleep( 0.01f );
		}
	}

	TDLOG( TEXT( "TDThread: Run() finished" ) );

	return 0;
}


/**
 * This is called if a thread is requested to terminate early
 */
void FTaskDatabaseThreadRunnable::Stop()
{
	TDLOG( TEXT( "TDThread: Stop() started" ) );

	// Ask the thread to stop after it's current operation
	bAskedToStop = true;

	TDLOG( TEXT( "TDThread: Stop() finished" ) );
}


/**
 * Called in the context of the aggregating thread to perform any cleanup.
 */
void FTaskDatabaseThreadRunnable::Exit()
{
	TDLOG( TEXT( "TDThread: Exit() started" ) );

	// Cleanup task database provider
	if( TaskDatabaseProvider != NULL )
	{
		delete TaskDatabaseProvider;
		TaskDatabaseProvider = NULL;
	}

	TDLOG( TEXT( "TDThread: Exit() finished" ) );
}
