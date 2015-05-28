/************************************************************************************

Filename    :   MessageQueue.h
Content     :   Thread communication by string commands
Created     :   October 15, 2013
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_MessageQueue_h
#define OVR_MessageQueue_h

#include <pthread.h>

namespace OVR
{

// This is a multiple-producer, single-consumer message queue.

class MessageQueue
{
public:
					MessageQueue( int maxMessages );
					~MessageQueue();

	// Shut down the message queue once messages are no longer polled
	// to avoid overflowing the queue on message spam.
	void			Shutdown();

	// Thread safe, callable by any thread.
	// The msg text is copied off before return, the caller can free
	// the buffer.
	// The app will abort() with a dump of all messages if the message
	// buffer overflows.
	void			PostString( const char * msg );

	// Builds a printf string and sends it as a message.
	void			PostPrintf( const char * fmt, ... );

	// Same as above but these return false if the queue is full instead of an abort.
	bool			TryPostString( const char * msg );
	bool			TryPostPrintf( const char * fmt, ... );

	// Same as above but these wait until the message has been processed.
	void			SendString( const char * msg );
	void			SendPrintf( const char * fmt, ... );

	// The other methods are NOT thread safe, and should only be
	// called by the thread that owns the MessageQueue.

	// Returns NULL if there are no more messages, otherwise returns
	// a string that the caller is now responsible for freeing.
	const char * 	GetNextMessage();

	// Returns immediately if there is already a message in the queue.
	void			SleepUntilMessage();

	// Dumps all unread messages
	void			ClearMessages();

private:
	// If set true, print all message sends and gets to the log
	static bool		debug;

	bool			shutdown;
	const int 		maxMessages;

	struct message_t
	{
		const char *	string;
		bool			synced;
	};

	// All messages will be allocated with strdup, and returned to
	// the caller on GetNextMessage().
	message_t * 	messages;

	// PostMessage() fills in messages[tail%maxMessages], then increments tail
	// If tail > head, GetNextMessage() will fetch messages[head%maxMessages],
	// then increment head.
	volatile int	head;
	volatile int	tail;
	bool			synced;
	pthread_mutex_t	mutex;
	pthread_cond_t	posted;
	pthread_cond_t	received;

	bool PostMessage( const char * msg, bool synced, bool abortIfFull );
};

}	// namespace OVR

#endif	// OVR_MessageQueue_h
