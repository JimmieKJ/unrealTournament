/************************************************************************************

Filename    :   MessageQueue.cpp
Content     :   Thread communication by string commands
Created     :   October 15, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "MessageQueue.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/time.h>
#include <sys/resource.h>

#include "Log.h"

namespace OVR
{

bool MessageQueue::debug = false;

MessageQueue::MessageQueue( int maxMessages_ ) :
	shutdown( false ),
	maxMessages( maxMessages_ ),
	messages( new const char *[ maxMessages_ ] ),
	head( 0 ),
	tail( 0 )
{
	assert( maxMessages > 0 );

	for ( int i = 0 ; i < maxMessages ; i++ )
	{
		messages[i] = NULL;
	}
	// We want an error checking mutex
	pthread_mutexattr_t	attr;
	pthread_mutexattr_init( &attr );
	pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_ERRORCHECK );

	pthread_mutex_init( &mutex, &attr );

	pthread_mutexattr_destroy( &attr );

	pthread_cond_init( &wake, NULL /* default attributes */ );
}

MessageQueue::~MessageQueue()
{
	// Free any messages remaining on the queue.
	for ( ; ; )
	{
		const char * msg = GetNextMessage();
		if ( !msg ) {
			break;
		}
		LOG( "%p:~MessageQueue: still on queue: %s", this, msg );
		free( (void *)msg );
	}

	// Free the queue itself.
	delete[] messages;

	pthread_mutex_destroy( &mutex );
	pthread_cond_destroy( &wake );
}

void MessageQueue::Shutdown()
{
	LOG( "%p:MessageQueue shutdown", this );
	shutdown = true;
}

void MessageQueue::PostPrintf( const char * fmt, ... )
{
	char bigBuffer[4096];
	va_list	args;
	va_start( args, fmt );
	vsnprintf( bigBuffer, sizeof( bigBuffer ), fmt, args );
	va_end( args );
	PostString( bigBuffer );
}

// Thread safe, callable by any thread.
// The msg text is copied off before return, the caller can free
// the buffer.
// The app will abort() with a dump of all messages if the message
// buffer overflows.
void MessageQueue::PostString( const char * msg )
{
	if ( shutdown )
	{
		LOG( "%p:PostMessage( %s ) to shutdown queue", this, msg );
		return;
	}
	if ( debug )
	{
		LOG( "%p:PostMessage( %s )", this, msg );
	}
	const char * dupMsg = strdup( msg );

	pthread_mutex_lock( &mutex );

	if ( tail - head >= maxMessages )
	{
		LOG( "MessageQueue overflow" );
		for ( int i = head ; i < tail ; i++ )
		{
			LOG( "%s", messages[ i % maxMessages ] );
		}
		pthread_mutex_unlock( &mutex );
		FAIL( "Message buffer overflowed" );
	}

	const int index = tail%maxMessages;
	assert( messages[index] == NULL );
	messages[index] = dupMsg;
	tail++;

	// Signal the thread in case it is sleeping, but it
	// won't be able to start until we release the mutex.
	pthread_cond_signal( &wake );

	pthread_mutex_unlock( &mutex );
}

// Returns false if there are no more messages, otherwise returns
// a string that the caller must free.
const char * MessageQueue::GetNextMessage()
{
	pthread_mutex_lock( &mutex );
	if ( tail <= head )
	{
		pthread_mutex_unlock( &mutex );
		return NULL;
	}

	const int index = head % maxMessages;
	const char * msg = messages[ index ];
	messages[ index ] = NULL;
	head++;

	pthread_mutex_unlock( &mutex );

	if ( debug )
	{
		LOG( "%p:GetNextMessage() : %s", this, msg );
	}

	return msg;
}

// Returns immediately if there is already a message in the queue.
void MessageQueue::SleepUntilMessage()
{
	pthread_mutex_lock( &mutex );
	if ( tail > head )
	{
		// we already have a message
		pthread_mutex_unlock( &mutex );
		return;
	}
	if ( debug )
	{
		LOG( "%p:SleepUntilMessage() : sleep", this );
	}
	// Atomically unlock the mutex and block until we get a message.
	pthread_cond_wait( &wake, &mutex );
	pthread_mutex_unlock( &mutex );
	if ( debug )
	{
		LOG( "%p:SleepUntilMessage() : awoke", this );
	}
}

void MessageQueue::ClearMessages()
{
	if ( debug )
	{
		LOG( "%p:ClearMessages()", this );
	}
	for ( ; ; )
	{
		const char * msg = GetNextMessage();
		if ( !msg ) {
			break;
		}
		LOG( "%p:ClearMessages: discarding %s", this, msg );
		free( (void *)msg );
	}
}

bool MessageQueue::MatchesHead( const char * head, const char * check )
{
	const int l = strlen( head );
	return 0 == strncmp( head, check, l );
}

}	// namespace OVR
