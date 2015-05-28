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
#include <unistd.h>

#include <sys/time.h>
#include <sys/resource.h>

#include "Android/LogUtils.h"

namespace OVR
{

bool MessageQueue::debug = false;

MessageQueue::MessageQueue( int maxMessages_ ) :
	shutdown( false ),
	maxMessages( maxMessages_ ),
	messages( new message_t[ maxMessages_ ] ),
	head( 0 ),
	tail( 0 )
{
	assert( maxMessages > 0 );

	for ( int i = 0; i < maxMessages; i++ )
	{
		messages[i].string = NULL;
		messages[i].synced = false;
	}

	pthread_mutexattr_t	attr;
	pthread_mutexattr_init( &attr );
	pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_ERRORCHECK );
	pthread_mutex_init( &mutex, &attr );
	pthread_mutexattr_destroy( &attr );
	pthread_cond_init( &posted, NULL );
	pthread_cond_init( &received, NULL );
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
	pthread_cond_destroy( &posted );
	pthread_cond_destroy( &received );
}

void MessageQueue::Shutdown()
{
	LOG( "%p:MessageQueue shutdown", this );
	shutdown = true;
}

// Thread safe, callable by any thread.
// The msg text is copied off before return, the caller can free
// the buffer.
// The app will abort() with a dump of all messages if the message
// buffer overflows.
bool MessageQueue::PostMessage( const char * msg, bool synced, bool abortIfFull )
{
	if ( shutdown )
	{
		LOG( "%p:PostMessage( %s ) to shutdown queue", this, msg );
		return false;
	}
	if ( debug )
	{
		LOG( "%p:PostMessage( %s )", this, msg );
	}

	pthread_mutex_lock( &mutex );
	if ( tail - head >= maxMessages )
	{
		pthread_mutex_unlock( &mutex );
		if ( abortIfFull )
		{
			LOG( "MessageQueue overflow" );
			for ( int i = head; i <= tail; i++ )
			{
				LOG( "%s", messages[i % maxMessages].string );
			}
			FAIL( "Message buffer overflowed" );
		}
		return false;
	}
	const int index = tail % maxMessages;
	messages[index].string = strdup( msg );
	messages[index].synced = synced;
	tail++;
	pthread_cond_signal( &posted );
	if ( synced )
	{
		pthread_cond_wait( &received, &mutex );
	}
	pthread_mutex_unlock( &mutex );

	return true;
}

void MessageQueue::PostString( const char * msg )
{
	PostMessage( msg, false, true );
}

void MessageQueue::PostPrintf( const char * fmt, ... )
{
	char bigBuffer[4096];
	va_list	args;
	va_start( args, fmt );
	vsnprintf( bigBuffer, sizeof( bigBuffer ), fmt, args );
	va_end( args );
	PostMessage( bigBuffer, false, true );
}

bool MessageQueue::TryPostString( const char * msg )
{
	return PostMessage( msg, false, false );
}

bool MessageQueue::TryPostPrintf( const char * fmt, ... )
{
	char bigBuffer[4096];
	va_list	args;
	va_start( args, fmt );
	vsnprintf( bigBuffer, sizeof( bigBuffer ), fmt, args );
	va_end( args );
	return PostMessage( bigBuffer, false, false );
}

void MessageQueue::SendString( const char * msg )
{
	PostMessage( msg, true, true );
}

void MessageQueue::SendPrintf( const char * fmt, ... )
{
	char bigBuffer[4096];
	va_list	args;
	va_start( args, fmt );
	vsnprintf( bigBuffer, sizeof( bigBuffer ), fmt, args );
	va_end( args );
	PostMessage( bigBuffer, true, true );
}

// Returns false if there are no more messages, otherwise returns
// a string that the caller must free.
const char * MessageQueue::GetNextMessage()
{
	if ( synced )
	{
		pthread_cond_signal( &received );
		synced = false;
	}

	if ( tail <= head )
	{
		return NULL;
	}

	pthread_mutex_lock( &mutex );
	const int index = head % maxMessages;
	const char * msg = messages[index].string;
	synced = messages[index].synced;
	messages[index].string = NULL;
	messages[index].synced = false;
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
	if ( tail > head )
	{
		return;
	}

	if ( debug )
	{
		LOG( "%p:SleepUntilMessage() : sleep", this );
	}

	if ( synced )
	{
		pthread_cond_signal( &received );
		synced = false;
	}

	pthread_mutex_lock( &mutex );
	pthread_cond_wait( &posted, &mutex );
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
	for ( const char * msg = GetNextMessage(); msg != NULL; msg = GetNextMessage() )
	{
		LOG( "%p:ClearMessages: discarding %s", this, msg );
		free( (void *)msg );
	}
}

}	// namespace OVR
